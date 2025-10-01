#include <algorithm>
#include <sstream>
#include <iostream>
#include "SynchronizationGenerator.hpp"

namespace StarryEngine {

    // ResourceStateTracker 实现
    void ResourceStateTracker::initialize(const std::vector<VirtualResource>& resources) {
        mResourceStates.clear();
        for (const auto& resource : resources) {
            TrackedState tracked;
            tracked.state = resource.initialState;
            tracked.lastWriter = RenderPassHandle(UINT32_MAX);
            mResourceStates[resource.handle] = tracked;
        }
    }

    void ResourceStateTracker::updateState(ResourceHandle resource, const ResourceState& newState, RenderPassHandle pass) {
        auto it = mResourceStates.find(resource);
        if (it != mResourceStates.end()) {
            it->second.state = newState;
            if (newState.isWrite()) {
                it->second.lastWriter = pass;
            }
            else {
                it->second.readers.push_back(pass);
            }
        }
    }

    const ResourceState& ResourceStateTracker::getCurrentState(ResourceHandle resource) const {
        static ResourceState defaultState;
        auto it = mResourceStates.find(resource);
        if (it != mResourceStates.end()) {
            return it->second.state;
        }
        return defaultState;
    }

    RenderPassHandle ResourceStateTracker::getLastWriter(ResourceHandle resource) const {
        auto it = mResourceStates.find(resource);
        if (it != mResourceStates.end()) {
            return it->second.lastWriter;
        }
        return RenderPassHandle(UINT32_MAX);
    }

    bool ResourceStateTracker::needsTransition(ResourceHandle resource, const ResourceState& requiredState) const {
        const ResourceState& current = getCurrentState(resource);
        return current.layout != requiredState.layout ||
            current.accessMask != requiredState.accessMask ||
            current.stageMask != requiredState.stageMask;
    }

    // SynchronizationGenerator 实现
    SynchronizationGenerator::SynchronizationGenerator(VkDevice device)
        : mDevice(device) {
        // 默认配置
        mConfig.enableBarrierBatching = true;
        mConfig.enableLayoutTransitionOptimization = true;
        mConfig.enableQueueFamilyOwnershipOptimization = true;
        mConfig.maxBarriersPerBatch = 100;
    }

    std::unordered_map<RenderPassHandle, BarrierBatch> SynchronizationGenerator::generateSynchronization(
        const RenderGraphAnalysisResult& analysis,
        const std::vector<std::unique_ptr<RenderPass>>& passes,
        const std::vector<VirtualResource>& resources,
        ResourceRegistry& registry) {

        mStats.reset();
        mStateTracker.initialize(resources);

        std::unordered_map<RenderPassHandle, BarrierBatch> barriers;

        // 为每个渲染通道生成屏障
        for (RenderPassHandle passHandle : analysis.executionOrder) {
            const auto& pass = passes[passHandle.getId()];

            BarrierBatch batch = generateBarriersForPass(passHandle, *pass, resources, registry, analysis);

            if (mConfig.enableBarrierBatching) {
                std::unordered_map<RenderPassHandle, BarrierBatch> tempBarriers;
                tempBarriers[passHandle] = batch;
                optimizeBarriers(tempBarriers);
            }

            if (!batch.empty()) {
                barriers[passHandle] = batch;
            }

            // 更新资源状态
            for (const auto& usage : pass->getResourceUsages()) {
                ResourceState newState;
                newState.layout = usage.layout;
                newState.accessMask = usage.accessFlags;
                newState.stageMask = usage.stageFlags;
                mStateTracker.updateState(usage.resource, newState, passHandle);
            }
        }

        return barriers;
    }

    BarrierBatch SynchronizationGenerator::generateBarriersForPass(
        RenderPassHandle passHandle,
        const RenderPass& pass,
        const std::vector<VirtualResource>& resources,
        ResourceRegistry& registry,
        const RenderGraphAnalysisResult& analysis) {

        BarrierBatch batch;

        for (const auto& usage : pass.getResourceUsages()) {
            const auto& virtResource = resources[usage.resource.getId()];

            // 跳过外部资源的状态管理
            if (virtResource.isImported) {
                continue;
            }

            // 检查是否需要状态转换
            ResourceState requiredState;
            requiredState.layout = usage.layout;
            requiredState.accessMask = usage.accessFlags;
            requiredState.stageMask = usage.stageFlags;

            if (!mStateTracker.needsTransition(usage.resource, requiredState)) {
                continue;
            }

            const ResourceState& currentState = mStateTracker.getCurrentState(usage.resource);

            // 验证状态转换
            if (!validateStateTransition(currentState, requiredState)) {
                std::cerr << "Invalid state transition for resource: " << virtResource.name << std::endl;
                continue;
            }

            // 跳过无操作转换
            if (isNoOpTransition(currentState, requiredState)) {
                continue;
            }

            // 获取实际资源
            const auto& actualResource = registry.getActualResource(usage.resource, 0);

            // 根据资源类型创建相应的屏障
            if (virtResource.description.type == ResourceType::SampledImage ||
                virtResource.description.type == ResourceType::ColorAttachment ||
                virtResource.description.type == ResourceType::DepthStencilAttachment) {

                VkImageMemoryBarrier imageBarrier = createImageBarrier(
                    actualResource, virtResource, currentState, requiredState);
                batch.imageBarriers.push_back(imageBarrier);
                mStats.imageBarriers++;

            }
            else if (virtResource.description.type == ResourceType::UniformBuffer ||
                virtResource.description.type == ResourceType::StorageBuffer) {

                VkBufferMemoryBarrier bufferBarrier = createBufferBarrier(
                    actualResource, virtResource, currentState, requiredState);
                batch.bufferBarriers.push_back(bufferBarrier);
                mStats.bufferBarriers++;
            }

            // 如果需要内存屏障
            if (currentState.stageMask != requiredState.stageMask &&
                (currentState.accessMask != 0 || requiredState.accessMask != 0)) {

                VkMemoryBarrier memoryBarrier = createMemoryBarrier(currentState, requiredState);
                batch.memoryBarriers.push_back(memoryBarrier);
                mStats.memoryBarriers++;
            }
        }

        mStats.totalBarriers = mStats.imageBarriers + mStats.bufferBarriers + mStats.memoryBarriers;
        return batch;
    }

    VkImageMemoryBarrier SynchronizationGenerator::createImageBarrier(
        const ActualResource& resource,
        const VirtualResource& virtResource,
        const ResourceState& currentState,
        const ResourceState& requiredState) const {

        VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.oldLayout = currentState.layout;
        barrier.newLayout = requiredState.layout;
        barrier.srcAccessMask = currentState.accessMask;
        barrier.dstAccessMask = requiredState.accessMask;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = resource.image.image;

        // 设置子资源范围
        barrier.subresourceRange.aspectMask = determineAspectMask(virtResource.description.format);
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = virtResource.description.mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = virtResource.description.arrayLayers;

        return barrier;
    }

    VkBufferMemoryBarrier SynchronizationGenerator::createBufferBarrier(
        const ActualResource& resource,
        const VirtualResource& virtResource,
        const ResourceState& currentState,
        const ResourceState& requiredState) const {

        VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
        barrier.srcAccessMask = currentState.accessMask;
        barrier.dstAccessMask = requiredState.accessMask;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = resource.buffer.buffer;
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;

        return barrier;
    }

    VkMemoryBarrier SynchronizationGenerator::createMemoryBarrier(
        const ResourceState& currentState,
        const ResourceState& requiredState) const {

        VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        barrier.srcAccessMask = currentState.accessMask;
        barrier.dstAccessMask = requiredState.accessMask;
        return barrier;
    }

    void SynchronizationGenerator::optimizeBarriers(std::unordered_map<RenderPassHandle, BarrierBatch>& barriers) {
        for (auto& [passHandle, batch] : barriers) {
            if (mConfig.enableBarrierBatching) {
                batchSimilarBarriers(batch);
            }

            removeRedundantBarriers(batch);

            if (mConfig.enableLayoutTransitionOptimization) {
                optimizeLayoutTransitions(batch);
            }

            mStats.optimizedBarriers += static_cast<uint32_t>(
                batch.imageBarriers.size() +
                batch.bufferBarriers.size() +
                batch.memoryBarriers.size());
        }
    }

    void SynchronizationGenerator::batchSimilarBarriers(BarrierBatch& batch) {
        // 简化的批处理实现 - 按图像和布局对图像屏障进行分组
        std::unordered_map<VkImage, std::vector<VkImageMemoryBarrier*>> imageBarriers;
        for (auto& barrier : batch.imageBarriers) {
            imageBarriers[barrier.image].push_back(&barrier);
        }

        // 按缓冲区对缓冲区屏障进行分组
        std::unordered_map<VkBuffer, std::vector<VkBufferMemoryBarrier*>> bufferBarriers;
        for (auto& barrier : batch.bufferBarriers) {
            bufferBarriers[barrier.buffer].push_back(&barrier);
        }

        // 这里可以实现更复杂的批处理逻辑
    }

    void SynchronizationGenerator::removeRedundantBarriers(BarrierBatch& batch) {
        // 移除无操作的内存屏障
        batch.memoryBarriers.erase(
            std::remove_if(batch.memoryBarriers.begin(), batch.memoryBarriers.end(),
                [](const VkMemoryBarrier& barrier) {
                    return barrier.srcAccessMask == 0 && barrier.dstAccessMask == 0;
                }),
            batch.memoryBarriers.end());

        // 移除无操作的图像屏障
        batch.imageBarriers.erase(
            std::remove_if(batch.imageBarriers.begin(), batch.imageBarriers.end(),
                [](const VkImageMemoryBarrier& barrier) {
                    return barrier.oldLayout == barrier.newLayout &&
                        barrier.srcAccessMask == barrier.dstAccessMask;
                }),
            batch.imageBarriers.end());

        // 移除无操作的缓冲区屏障
        batch.bufferBarriers.erase(
            std::remove_if(batch.bufferBarriers.begin(), batch.bufferBarriers.end(),
                [](const VkBufferMemoryBarrier& barrier) {
                    return barrier.srcAccessMask == barrier.dstAccessMask;
                }),
            batch.bufferBarriers.end());
    }

    void SynchronizationGenerator::optimizeLayoutTransitions(BarrierBatch& batch) {
        // 优化 UNDEFINED -> 某种布局的转换
        for (auto& barrier : batch.imageBarriers) {
            if (barrier.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
                barrier.srcAccessMask = 0;
            }

            // 优化到 PRESENT_SRC_KHR 的转换
            if (barrier.newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                barrier.dstAccessMask = 0;
            }
        }
    }

    bool SynchronizationGenerator::validateStateTransition(const ResourceState& from, const ResourceState& to) const {
        // 简化的状态转换验证
        if (from.layout == VK_IMAGE_LAYOUT_UNDEFINED) {
            return true;
        }

        if (from.layout == VK_IMAGE_LAYOUT_PREINITIALIZED) {
            return to.layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ||
                to.layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ||
                to.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ||
                to.layout == VK_IMAGE_LAYOUT_GENERAL;
        }

        return true;
    }

    bool SynchronizationGenerator::isNoOpTransition(const ResourceState& from, const ResourceState& to) const {
        return from.layout == to.layout &&
            from.accessMask == to.accessMask &&
            from.stageMask == to.stageMask;
    }

    VkPipelineStageFlags SynchronizationGenerator::determineSrcStageMask(const ResourceState& state) const {
        VkPipelineStageFlags stages = 0;

        if (state.accessMask & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) {
            stages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        }
        if (state.accessMask & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) {
            stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        }
        if (state.accessMask & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT)) {
            stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        if (state.accessMask & VK_ACCESS_SHADER_WRITE_BIT) {
            stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        if (state.accessMask & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) {
            stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        if (state.accessMask & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) {
            stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        }
        if (state.accessMask & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) {
            stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        if (state.accessMask & VK_ACCESS_HOST_READ_BIT) {
            stages |= VK_PIPELINE_STAGE_HOST_BIT;
        }

        return stages ? stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }

    VkPipelineStageFlags SynchronizationGenerator::determineDstStageMask(const ResourceState& state) const {
        VkPipelineStageFlags stages = 0;

        if (state.accessMask & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) {
            stages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        }
        if (state.accessMask & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) {
            stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        }
        if (state.accessMask & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT)) {
            stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        if (state.accessMask & VK_ACCESS_SHADER_WRITE_BIT) {
            stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        if (state.accessMask & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) {
            stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        if (state.accessMask & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) {
            stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        }
        if (state.accessMask & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) {
            stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        if (state.accessMask & VK_ACCESS_HOST_READ_BIT) {
            stages |= VK_PIPELINE_STAGE_HOST_BIT;
        }

        return stages ? stages : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    VkImageAspectFlags SynchronizationGenerator::determineAspectMask(VkFormat format) const {
        if (isDepthStencilFormat(format)) {
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        else {
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    bool SynchronizationGenerator::isDepthStencilFormat(VkFormat format) const {
        switch (format) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return true;
        default:
            return false;
        }
    }

    bool SynchronizationGenerator::isColorFormat(VkFormat format) const {
        return !isDepthStencilFormat(format);
    }

    void SynchronizationGenerator::dumpBarrierInfo(const std::unordered_map<RenderPassHandle, BarrierBatch>& barriers) const {
        std::cout << "=== Synchronization Barrier Info ===" << std::endl;
        std::cout << "Total passes with barriers: " << barriers.size() << std::endl;

        for (const auto& [passHandle, batch] : barriers) {
            std::cout << "Pass " << passHandle.getId() << ":" << std::endl;
            std::cout << "  Image barriers: " << batch.imageBarriers.size() << std::endl;
            std::cout << "  Buffer barriers: " << batch.bufferBarriers.size() << std::endl;
            std::cout << "  Memory barriers: " << batch.memoryBarriers.size() << std::endl;

            for (const auto& barrier : batch.imageBarriers) {
                std::cout << "    Image barrier: " << imageLayoutToString(barrier.oldLayout)
                    << " -> " << imageLayoutToString(barrier.newLayout) << std::endl;
            }
        }

        std::cout << "=== Synchronization Stats ===" << std::endl;
        std::cout << "Total barriers: " << mStats.totalBarriers << std::endl;
        std::cout << "Image barriers: " << mStats.imageBarriers << std::endl;
        std::cout << "Buffer barriers: " << mStats.bufferBarriers << std::endl;
        std::cout << "Memory barriers: " << mStats.memoryBarriers << std::endl;
        std::cout << "Optimized barriers: " << mStats.optimizedBarriers << std::endl;
    }

    std::string SynchronizationGenerator::resourceStateToString(const ResourceState& state) const {
        std::stringstream ss;
        ss << "Layout: " << imageLayoutToString(state.layout)
            << ", Access: 0x" << std::hex << state.accessMask
            << ", Stage: 0x" << state.stageMask;
        return ss.str();
    }

    std::string SynchronizationGenerator::imageLayoutToString(VkImageLayout layout) const {
        switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED: return "UNDEFINED";
        case VK_IMAGE_LAYOUT_GENERAL: return "GENERAL";
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return "COLOR_ATTACHMENT";
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return "DEPTH_STENCIL_ATTACHMENT";
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: return "DEPTH_STENCIL_READ_ONLY";
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return "SHADER_READ_ONLY";
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return "TRANSFER_SRC";
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return "TRANSFER_DST";
        case VK_IMAGE_LAYOUT_PREINITIALIZED: return "PREINITIALIZED";
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return "PRESENT_SRC";
        default: return "UNKNOWN";
        }
    }

} // namespace StarryEngine