#include "RenderGraphExecutor.hpp"
#include "RenderGraph.hpp"
#include <cassert>

namespace StarryEngine {
    RenderGraphExecutor::RenderGraphExecutor(VkDevice device)
        : mDevice(device), mCurrentFrame(0) {
    }

    RenderGraphExecutor::~RenderGraphExecutor() {
        // 清理每帧数据
        for (auto& frameData : mFrameData) {
            // 清理描述符集等资源
        }
    }

    bool RenderGraphExecutor::initialize(uint32_t framesInFlight) {
        mFrameData.resize(framesInFlight);
        return true;
    }

    void RenderGraphExecutor::execute(RenderGraph& graph, VkCommandBuffer cmd, uint32_t frameIndex) {
        mCurrentFrame = frameIndex;

        // 获取编译结果
        const auto& compiler = graph.getCompiler();
        const auto& executionOrder = compiler.getExecutionOrder();
        const auto& barriers = compiler.getBarriers();

        // 创建渲染上下文
        RenderContextImpl context(cmd, frameIndex, &graph.getResourceRegistry(), this);

        // 按顺序执行每个pass
        for (RenderPassHandle passHandle : executionOrder) {
            // 插入屏障
            auto it = barriers.find(passHandle);
            if (it != barriers.end() && !it->second.empty()) {
                insertBarriers(cmd, passHandle, it->second);
            }

            // 执行pass
            executePass(graph, passHandle, cmd, frameIndex);
        }
    }

    void RenderGraphExecutor::beginFrame(uint32_t frameIndex) {
        mCurrentFrame = frameIndex;

        // 重置每帧数据
        mFrameData[frameIndex].descriptorSets.clear();
    }

    void RenderGraphExecutor::endFrame(uint32_t frameIndex) {
        // 清理临时资源等
    }

    void RenderGraphExecutor::executePass(RenderGraph& graph, RenderPassHandle passHandle,
        VkCommandBuffer cmd, uint32_t frameIndex) {
        const auto& passes = graph.getPasses();
        const auto& pass = passes[passHandle.getId()];

        // 创建渲染上下文
        RenderContextImpl context(cmd, frameIndex, &graph.getResourceRegistry(), this);

        // 绑定资源
        bindResources(cmd, *pass, context, frameIndex);

        // 执行pass
        pass->execute(cmd, context);
    }

    void RenderGraphExecutor::insertBarriers(VkCommandBuffer cmd, RenderPassHandle passHandle,
        const BarrierBatch& barriers) {
        VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        vkCmdPipelineBarrier(
            cmd,
            srcStageMask, dstStageMask,
            0,
            static_cast<uint32_t>(barriers.memoryBarriers.size()), barriers.memoryBarriers.data(),
            static_cast<uint32_t>(barriers.bufferBarriers.size()), barriers.bufferBarriers.data(),
            static_cast<uint32_t>(barriers.imageBarriers.size()), barriers.imageBarriers.data()
        );
    }

    void RenderGraphExecutor::bindResources(VkCommandBuffer cmd, const RenderPass& pass,
        const RenderContextImpl& context, uint32_t frameIndex) {
        // 这里实现资源绑定逻辑，如描述符集绑定等
        // 简化实现，实际中需要根据pass的资源使用情况来绑定
    }

    VkDescriptorSet RenderGraphExecutor::allocateDescriptorSet(VkDescriptorSetLayout layout) {
        // 简化的描述符集分配
        // 实际中应该使用描述符池

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = VK_NULL_HANDLE; // 需要实际的描述符池
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkResult result = vkAllocateDescriptorSets(mDevice, &allocInfo, &descriptorSet);

        if (result == VK_SUCCESS) {
            // 存储到当前帧数据中
            mFrameData[mCurrentFrame].descriptorSets.push_back(descriptorSet);
        }

        return descriptorSet;
    }

    void RenderGraphExecutor::updateDescriptorSets(const std::vector<VkWriteDescriptorSet>& writes) {
        vkUpdateDescriptorSets(mDevice,
            static_cast<uint32_t>(writes.size()), writes.data(),
            0, nullptr);
    }

    VkImage RenderContextImpl::getImage(ResourceHandle handle) const {
        auto& actualResource = resourceRegistry->getActualResource(handle, frameIndex);
        return actualResource.image.image;
    }

    VkBuffer RenderContextImpl::getBuffer(ResourceHandle handle) const {
        auto& actualResource = resourceRegistry->getActualResource(handle, frameIndex);
        return actualResource.buffer.buffer;
    }

    VkImageView RenderContextImpl::getImageView(ResourceHandle handle) const {
        auto& actualResource = resourceRegistry->getActualResource(handle, frameIndex);
        return actualResource.image.defaultView;
    }

    VkDescriptorSet RenderContextImpl::getDescriptorSet(const VkDescriptorSetLayoutCreateInfo& layoutInfo,
        const std::vector<VkWriteDescriptorSet>& writes) {
        // 创建或获取描述符集布局
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        // 简化的布局缓存实现
        // ...

        // 分配描述符集
        VkDescriptorSet descriptorSet = executor->allocateDescriptorSet(layout);

        // 更新描述符集
        std::vector<VkWriteDescriptorSet> updatedWrites = writes;
        for (auto& write : updatedWrites) {
            write.dstSet = descriptorSet;
        }
        executor->updateDescriptorSets(updatedWrites);

        return descriptorSet;
    }
}