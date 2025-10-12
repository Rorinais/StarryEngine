#pragma once
#include "RenderGraphTypes.hpp"
#include "ResourceSystem.hpp"
#include "RenderPassSystem.hpp"
#include "RenderGraphCompiler.hpp"
#include "RenderContext.hpp"
#include <vector>
#include <memory>

namespace StarryEngine {

    // 前向声明
    class RenderGraph;
    class DescriptorAllocator;
    class PipelineCache;

    class RenderGraphExecutor {
    public:
        RenderGraphExecutor(VkDevice device,
            DescriptorAllocator* descriptorAllocator,
            PipelineCache* pipelineCache);
        ~RenderGraphExecutor();

        // 初始化执行器
        bool initialize(uint32_t framesInFlight);

        // 执行渲染图
        void execute(RenderGraph& graph, VkCommandBuffer cmd, uint32_t frameIndex);

        // 帧开始和结束
        void beginFrame(uint32_t frameIndex);
        void endFrame(uint32_t frameIndex);

    private:
        // 执行辅助函数
        void executePass(RenderGraph& graph, RenderPassHandle passHandle,
            VkCommandBuffer cmd, uint32_t frameIndex);
        void insertBarriers(VkCommandBuffer cmd, RenderPassHandle passHandle,
            const BarrierBatch& barriers);

        // 描述符管理
        VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout layout);
        void updateDescriptorSets(const std::vector<VkWriteDescriptorSet>& writes);

        // 资源绑定
        void bindResources(VkCommandBuffer cmd, const RenderPass& pass,
            const RenderContext& context, uint32_t frameIndex);

        std::unique_ptr<RenderContext> createRenderContext(VkCommandBuffer cmd, uint32_t frameIndex, RenderGraph& graph);

    private:
        VkDevice mDevice;
        DescriptorAllocator* mDescriptorAllocator;
        PipelineCache* mPipelineCache;
        uint32_t mCurrentFrame = 0;

        struct ExecutionState {
            std::unique_ptr<RenderContext> context;
            const std::unordered_map<RenderPassHandle, BarrierBatch>* barriers = nullptr;
            const std::vector<RenderPassHandle>* executionOrder = nullptr;
        };
        ExecutionState mExecutionState;
    };

} // namespace StarryEngine