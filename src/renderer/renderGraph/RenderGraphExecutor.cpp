#include "RenderGraphExecutor.hpp"
#include "RenderGraph.hpp"
#include "RenderContext.hpp"
#include "DescriptorAllocator.hpp" // Added this line
#include "PipelineCache.hpp"
#include <cassert>

namespace StarryEngine {

    RenderGraphExecutor::RenderGraphExecutor(VkDevice device,
        DescriptorAllocator* descriptorAllocator,
        PipelineCache* pipelineCache)
        : mDevice(device),
        mDescriptorAllocator(descriptorAllocator),
        mPipelineCache(pipelineCache),
        mCurrentFrame(0) {
    }

    RenderGraphExecutor::~RenderGraphExecutor() {
        // 清理执行状态
        mExecutionState.context.reset();
    }

    bool RenderGraphExecutor::initialize(uint32_t framesInFlight) {
        mCurrentFrame = 0;
        // 可以在这里初始化每帧数据
        return true;
    }

    void RenderGraphExecutor::execute(RenderGraph& graph, VkCommandBuffer cmd, uint32_t frameIndex) {
        mCurrentFrame = frameIndex;

        // 获取编译结果
        const auto& executionOrder = graph.getCompiler().getExecutionOrder();
        const auto& barriers = graph.getCompiler().getBarriers();

        // 创建渲染上下文
        auto context = createRenderContext(cmd, frameIndex, graph);
        if (!context) {
            return;
        }

        mExecutionState.context = std::move(context);
        mExecutionState.barriers = &barriers;
        mExecutionState.executionOrder = &executionOrder;

        // 按顺序执行每个pass
        for (RenderPassHandle passHandle : executionOrder) {
            executePass(graph, passHandle, cmd, frameIndex);
        }

        // 清理执行状态
        mExecutionState.context.reset();
        mExecutionState.barriers = nullptr;
        mExecutionState.executionOrder = nullptr;
    }

    void RenderGraphExecutor::beginFrame(uint32_t frameIndex) {
        mCurrentFrame = frameIndex;
        if (mExecutionState.context) {
            mExecutionState.context->beginFrame();
        }
    }

    void RenderGraphExecutor::endFrame(uint32_t frameIndex) {
        if (mExecutionState.context) {
            mExecutionState.context->endFrame();
        }
    }

    void RenderGraphExecutor::executePass(RenderGraph& graph, RenderPassHandle passHandle,
        VkCommandBuffer cmd, uint32_t frameIndex) {

        const auto& passes = graph.getPasses();

        // 边界检查
        if (passHandle.getId() >= passes.size()) {
            return;
        }

        const auto& pass = passes[passHandle.getId()];

        // 插入屏障
        auto barrierIt = mExecutionState.barriers->find(passHandle);
        if (barrierIt != mExecutionState.barriers->end() && !barrierIt->second.empty()) {
            insertBarriers(cmd, passHandle, barrierIt->second);
        }

        // 执行pass
        if (mExecutionState.context) {
            pass->execute(nullptr, *mExecutionState.context);
        }
    }

    void RenderGraphExecutor::insertBarriers(VkCommandBuffer cmd, RenderPassHandle passHandle,
        const BarrierBatch& barriers) {

        if (barriers.empty()) {
            return;
        }

        // 确定管线阶段掩码
        VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        vkCmdPipelineBarrier(
            cmd,
            srcStageMask, dstStageMask,
            0,
            static_cast<uint32_t>(barriers.memoryBarriers.size()),
            barriers.memoryBarriers.empty() ? nullptr : barriers.memoryBarriers.data(),
            static_cast<uint32_t>(barriers.bufferBarriers.size()),
            barriers.bufferBarriers.empty() ? nullptr : barriers.bufferBarriers.data(),
            static_cast<uint32_t>(barriers.imageBarriers.size()),
            barriers.imageBarriers.empty() ? nullptr : barriers.imageBarriers.data()
        );
    }

    std::unique_ptr<RenderContext> RenderGraphExecutor::createRenderContext(
        VkCommandBuffer cmd, uint32_t frameIndex, RenderGraph& graph) {

        return std::make_unique<RenderContext>(
            mDevice,
            cmd,
            frameIndex,
            &graph.getResourceRegistry(),
            mDescriptorAllocator,
            mPipelineCache
        );
    }

    // 以下方法不再需要，因为功能已经在RenderContext中实现
    void RenderGraphExecutor::bindResources(VkCommandBuffer cmd, const RenderPass& pass,
        const RenderContext& context, uint32_t frameIndex) {
        // 资源绑定现在在RenderContext中处理
    }

    VkDescriptorSet RenderGraphExecutor::allocateDescriptorSet(VkDescriptorSetLayout layout) {
        if (mDescriptorAllocator) {
            return mDescriptorAllocator->allocateDescriptorSet(layout);

        }
        return VK_NULL_HANDLE;
    }

    void RenderGraphExecutor::updateDescriptorSets(const std::vector<VkWriteDescriptorSet>& writes) {
        if (!writes.empty()) {
            vkUpdateDescriptorSets(mDevice,
                static_cast<uint32_t>(writes.size()), writes.data(),
                0, nullptr);
        }
    }

} // namespace StarryEngine