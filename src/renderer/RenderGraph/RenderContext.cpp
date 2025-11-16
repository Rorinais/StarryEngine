#include "RenderContext.hpp"
#include "ResourceSystem.hpp"
#include "PipelineCache.hpp" 
#include "DescriptorAllocator.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>

namespace StarryEngine {

    RenderContext::RenderContext(VkDevice device, VkCommandBuffer cmd, uint32_t frameIndex,
        ResourceRegistry* registry, DescriptorAllocator* descriptorAllocator,
        PipelineCache* pipelineCache)
        : mDevice(device), mCommandBuffer(cmd), mFrameIndex(frameIndex),
        mResourceRegistry(registry), mDescriptorAllocator(descriptorAllocator),
        mPipelineCache(pipelineCache) {
    }

    VkImage RenderContext::getImage(ResourceHandle handle) const {
        const auto& actualResource = mResourceRegistry->getActualResource(handle, mFrameIndex);

        // 使用 std::visit 安全访问 variant
        VkImage image = VK_NULL_HANDLE;
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, ActualResource::Image>) {
                image = arg.image;
            }
            }, actualResource.actualResource);

        return image;
    }

    VkBuffer RenderContext::getBuffer(ResourceHandle handle) const {
        const auto& actualResource = mResourceRegistry->getActualResource(handle, mFrameIndex);

        // 使用 std::visit 安全访问 variant
        VkBuffer buffer = VK_NULL_HANDLE;
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, ActualResource::Buffer>) {
                buffer = arg.buffer;
            }
            }, actualResource.actualResource);

        return buffer;
    }

    VkImageView RenderContext::getImageView(ResourceHandle handle) const {
        const auto& actualResource = mResourceRegistry->getActualResource(handle, mFrameIndex);

        // 使用 std::visit 安全访问 variant
        VkImageView imageView = VK_NULL_HANDLE;
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, ActualResource::Image>) {
                imageView = arg.defaultView;
            }
            }, actualResource.actualResource);

        return imageView;
    }

    VkImageLayout RenderContext::getImageLayout(ResourceHandle handle) const {
        const auto& actualResource = mResourceRegistry->getActualResource(handle, mFrameIndex);
        return actualResource.currentState.layout;
    }

    VkDeviceSize RenderContext::getBufferSize(ResourceHandle handle) const {
        const auto& virtualResource = mResourceRegistry->getVirtualResource(handle);
        return virtualResource.description.size;
    }

    void* RenderContext::getBufferMappedPointer(ResourceHandle handle) const {
        const auto& actualResource = mResourceRegistry->getActualResource(handle, mFrameIndex);
        return actualResource.allocationInfo.pMappedData;
    }

    const ResourceDescription& RenderContext::getResourceDescription(ResourceHandle handle) const {
        const auto& virtualResource = mResourceRegistry->getVirtualResource(handle);
        return virtualResource.description;
    }

    std::string RenderContext::getResourceName(ResourceHandle handle) const {
        const auto& virtualResource = mResourceRegistry->getVirtualResource(handle);
        return virtualResource.name;
    }

    VkDescriptorSet RenderContext::createDescriptorSet(const std::vector<VkDescriptorSetLayoutBinding>& bindings,
        const std::vector<VkWriteDescriptorSet>& writes) {

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VkDescriptorSetLayout layout;
        VkResult result = vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr, &layout);
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create descriptor set layout" << std::endl;
            return VK_NULL_HANDLE;
        }

        VkDescriptorSet descriptorSet = mDescriptorAllocator->allocateDescriptorSet(layout);
        if (descriptorSet == VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(mDevice, layout, nullptr);
            return VK_NULL_HANDLE;
        }

        // 更新描述符集
        std::vector<VkWriteDescriptorSet> updatedWrites = writes;
        for (auto& write : updatedWrites) {
            write.dstSet = descriptorSet;
        }
        vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(updatedWrites.size()), updatedWrites.data(), 0, nullptr);

        // 清理布局
        vkDestroyDescriptorSetLayout(mDevice, layout, nullptr);

        return descriptorSet;
    }

    VkDescriptorSet RenderContext::getOrCreateDescriptorSet(const std::string& key,
        const std::vector<VkDescriptorSetLayoutBinding>& bindings,
        const std::vector<VkWriteDescriptorSet>& writes) {

        auto it = mDescriptorSetCache.find(key);
        if (it != mDescriptorSetCache.end()) {
            return it->second;
        }

        VkDescriptorSet descriptorSet = createDescriptorSet(bindings, writes);
        if (descriptorSet != VK_NULL_HANDLE) {
            mDescriptorSetCache[key] = descriptorSet;
        }

        return descriptorSet;
    }

    void RenderContext::beginRenderPass(const VkRenderPassBeginInfo* renderPassBeginInfo, VkSubpassContents subpassContents) {
        vkCmdBeginRenderPass(mCommandBuffer, renderPassBeginInfo, subpassContents);
    }

    void RenderContext::endRenderPass() {
        vkCmdEndRenderPass(mCommandBuffer);
    }

    void RenderContext::bindGraphicsPipeline(VkPipeline pipeline) {
        vkCmdBindPipeline(mCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }

    void RenderContext::bindComputePipeline(const std::string& pipelineName) {
        VkPipeline pipeline = mPipelineCache->getComputePipeline(pipelineName);
        if (pipeline != VK_NULL_HANDLE) {
            vkCmdBindPipeline(mCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
            mBindingState.currentBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
            mBindingState.currentPipeline = pipeline;
            mBindingState.currentPipelineLayout = mPipelineCache->getPipelineLayout(pipelineName + "_layout");
        }
    }

    void RenderContext::setViewport(const VkViewport& viewport) {
        vkCmdSetViewport(mCommandBuffer, 0, 1, &viewport);
    }

    void RenderContext::setScissor(const VkRect2D& scissor) {
        vkCmdSetScissor(mCommandBuffer, 0, 1, &scissor);
    }

    void RenderContext::setBlendConstants(const float constants[4]) {
        vkCmdSetBlendConstants(mCommandBuffer, constants);
    }

    void RenderContext::bindVertexBuffer(VkBuffer buffer, uint32_t binding, VkDeviceSize offset) {
        if (buffer != VK_NULL_HANDLE) {
            vkCmdBindVertexBuffers(mCommandBuffer, binding, 1, &buffer, &offset);
        }
    }

    void RenderContext::bindVertexBuffers(const std::vector<VkBuffer>& buffers) {
		std::vector<VkDeviceSize> offsets(buffers.size(), 0);
        vkCmdBindVertexBuffers(mCommandBuffer, 0, static_cast<uint32_t>(buffers.size()), buffers.data(), offsets.data());
    }

    void RenderContext::bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
        if (buffer != VK_NULL_HANDLE) {
            vkCmdBindIndexBuffer(mCommandBuffer, buffer, offset, indexType);
        }
    }

    void RenderContext::bindDescriptorSet(VkPipelineBindPoint bindPoint, VkDescriptorSet descriptorSet,
        uint32_t firstSet, VkPipelineLayout layout) {

        if (layout == VK_NULL_HANDLE) {
            layout = mBindingState.currentPipelineLayout;
        }

        if (layout != VK_NULL_HANDLE && descriptorSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(mCommandBuffer, bindPoint, layout, firstSet, 1, &descriptorSet, 0, nullptr);
        }
    }

    void RenderContext::bindDescriptorSets(VkPipelineBindPoint bindPoint, VkPipelineLayout layout,
        uint32_t firstSet, const std::vector<VkDescriptorSet>& descriptorSets) {

        if (layout == VK_NULL_HANDLE) {
            layout = mBindingState.currentPipelineLayout;
        }

        if (layout != VK_NULL_HANDLE && !descriptorSets.empty()) {
            vkCmdBindDescriptorSets(mCommandBuffer, bindPoint, layout, firstSet,
                static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
        }
    }

    // 绘制命令实现
    void RenderContext::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
        vkCmdDraw(mCommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void RenderContext::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
        int32_t vertexOffset, uint32_t firstInstance) {
        vkCmdDrawIndexed(mCommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void RenderContext::drawIndirect(ResourceHandle bufferHandle, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
        VkBuffer buffer = getBuffer(bufferHandle);
        if (buffer != VK_NULL_HANDLE) {
            vkCmdDrawIndirect(mCommandBuffer, buffer, offset, drawCount, stride);
        }
    }

    void RenderContext::drawIndexedIndirect(ResourceHandle bufferHandle, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
        VkBuffer buffer = getBuffer(bufferHandle);
        if (buffer != VK_NULL_HANDLE) {
            vkCmdDrawIndexedIndirect(mCommandBuffer, buffer, offset, drawCount, stride);
        }
    }

    void RenderContext::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
        vkCmdDispatch(mCommandBuffer, groupCountX, groupCountY, groupCountZ);
    }

    void RenderContext::dispatchIndirect(ResourceHandle bufferHandle, VkDeviceSize offset) {
        VkBuffer buffer = getBuffer(bufferHandle);
        if (buffer != VK_NULL_HANDLE) {
            vkCmdDispatchIndirect(mCommandBuffer, buffer, offset);
        }
    }

    void RenderContext::beginFrame() {
        cleanupTemporaryResources();
    }

    void RenderContext::endFrame() {
        // 每帧结束时可以执行一些清理工作
    }

    void RenderContext::cleanupTemporaryResources() {
        // 清理超过一定帧数的临时资源
        auto it = mTemporaryResources.begin();
        while (it != mTemporaryResources.end()) {
            if (mFrameIndex - it->frameCreated > 2) { // 保留2帧
                // 销毁临时资源
                it = mTemporaryResources.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    void RenderContext::validateResourceAccess(ResourceHandle handle, VkImageLayout expectedLayout) const {
        // 资源访问验证逻辑
        // 可以检查资源状态、布局等
    }

    // 资源更新命令实现
    void RenderContext::updateBuffer(ResourceHandle bufferHandle, const void* data, size_t size, VkDeviceSize offset) {
        VkBuffer buffer = getBuffer(bufferHandle);
        if (buffer != VK_NULL_HANDLE && data != nullptr) {
            void* mappedData = getBufferMappedPointer(bufferHandle);
            if (mappedData) {
                // 如果缓冲区已经被映射，直接复制
                memcpy(static_cast<char*>(mappedData) + offset, data, size);
            }
            else {
                // 否则使用 vkCmdUpdateBuffer（限制：最大 65536 字节）
                if (size <= 65536) {
                    vkCmdUpdateBuffer(mCommandBuffer, buffer, offset, size, data);
                }
                else {
                    // 对于大尺寸数据，需要其他方法（如暂存缓冲区）
                    std::cerr << "Warning: Large buffer update not implemented, size: " << size << std::endl;
                }
            }
        }
    }

    void RenderContext::fillBuffer(ResourceHandle bufferHandle, uint32_t data, VkDeviceSize offset, VkDeviceSize size) {
        VkBuffer buffer = getBuffer(bufferHandle);
        if (buffer != VK_NULL_HANDLE) {
            vkCmdFillBuffer(mCommandBuffer, buffer, offset, size, data);
        }
    }

    void RenderContext::copyBuffer(ResourceHandle srcBuffer, ResourceHandle dstBuffer, const std::vector<VkBufferCopy>& regions) {
        VkBuffer src = getBuffer(srcBuffer);
        VkBuffer dst = getBuffer(dstBuffer);
        if (src != VK_NULL_HANDLE && dst != VK_NULL_HANDLE && !regions.empty()) {
            vkCmdCopyBuffer(mCommandBuffer, src, dst, static_cast<uint32_t>(regions.size()), regions.data());
        }
    }

    void RenderContext::copyImage(ResourceHandle srcImage, ResourceHandle dstImage, const std::vector<VkImageCopy>& regions) {
        VkImage src = getImage(srcImage);
        VkImage dst = getImage(dstImage);
        if (src != VK_NULL_HANDLE && dst != VK_NULL_HANDLE && !regions.empty()) {
            vkCmdCopyImage(mCommandBuffer,
                src, getImageLayout(srcImage),
                dst, getImageLayout(dstImage),
                static_cast<uint32_t>(regions.size()), regions.data());
        }
    }

    void RenderContext::blitImage(ResourceHandle srcImage, ResourceHandle dstImage, const std::vector<VkImageBlit>& regions, VkFilter filter) {
        VkImage src = getImage(srcImage);
        VkImage dst = getImage(dstImage);
        if (src != VK_NULL_HANDLE && dst != VK_NULL_HANDLE && !regions.empty()) {
            vkCmdBlitImage(mCommandBuffer,
                src, getImageLayout(srcImage),
                dst, getImageLayout(dstImage),
                static_cast<uint32_t>(regions.size()), regions.data(),
                filter);
        }
    }

    // 动态资源管理实现
    ResourceHandle RenderContext::createTemporaryBuffer(const ResourceDescription& desc, const void* initialData) {
        // 创建临时缓冲区
        ResourceHandle handle = mResourceRegistry->createVirtualResource("TemporaryBuffer", desc);

        // 分配实际资源
        // 注意：这里简化实现，实际中需要更复杂的生命周期管理
        mResourceRegistry->allocateActualResources(1); // 为当前帧分配

        // 如果有初始数据，上传数据
        if (initialData != nullptr) {
            uploadToTemporary(handle, initialData, desc.size);
        }

        // 记录临时资源
        mTemporaryResources.push_back({ handle, mFrameIndex });

        return handle;
    }

    ResourceHandle RenderContext::createTemporaryImage(const ResourceDescription& desc) {
        // 创建临时图像
        ResourceHandle handle = mResourceRegistry->createVirtualResource("TemporaryImage", desc);

        // 分配实际资源
        mResourceRegistry->allocateActualResources(1); // 为当前帧分配

        // 记录临时资源
        mTemporaryResources.push_back({ handle, mFrameIndex });

        return handle;
    }

    void RenderContext::uploadToTemporary(ResourceHandle handle, const void* data, size_t size) {
        // 上传数据到临时资源
        if (data != nullptr && size > 0) {
            updateBuffer(handle, data, size, 0);
        }
    }

} // namespace StarryEngine