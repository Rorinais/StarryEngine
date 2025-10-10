#pragma once
#include "RenderGraphTypes.hpp"
#include "ResourceSystem.hpp"
#include "RenderPassSystem.hpp"
#include "RenderGraphCompiler.hpp"
#include <vector>
#include <memory>

namespace StarryEngine {

    // 前向声明
    class RenderGraph;
    class CommandBuffer;

    // 渲染上下文实现
    class RenderContextImpl {
    public:
        RenderContextImpl(VkCommandBuffer cmd, uint32_t frame, ResourceRegistry* registry, class RenderGraphExecutor* executor)
            : commandBuffer(cmd), frameIndex(frame), resourceRegistry(registry), executor(executor) {
        }

        VkImage getImage(ResourceHandle handle) const;
        VkBuffer getBuffer(ResourceHandle handle) const;
        VkImageView getImageView(ResourceHandle handle) const;

        VkDescriptorSet getDescriptorSet(const VkDescriptorSetLayoutCreateInfo& layoutInfo,
            const std::vector<VkWriteDescriptorSet>& writes);

        VkCommandBuffer commandBuffer;
        uint32_t frameIndex;
        ResourceRegistry* resourceRegistry;
        class RenderGraphExecutor* executor;
    };

    class RenderGraphExecutor {
    private:
        // 每帧数据
        struct FrameData {
            std::vector<VkDescriptorSet> descriptorSets;
        };

        VkDevice mDevice;
        std::vector<FrameData> mFrameData;
        uint32_t mCurrentFrame;

    public:
        friend class RenderContextImpl;

        RenderGraphExecutor(VkDevice device);
        ~RenderGraphExecutor();

        // 初始化执行器
        bool initialize(uint32_t framesInFlight);

        // 执行渲染图
        void execute(RenderGraph& graph, VkCommandBuffer cmd, uint32_t frameIndex);

        // 帧开始和结束
        void beginFrame(uint32_t frameIndex);
        void endFrame(uint32_t frameIndex);

        const FrameData& getFrameData(uint32_t frameIndex) const {
            return mFrameData[frameIndex];
        }

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
            const RenderContextImpl& context, uint32_t frameIndex);
    };

} // namespace StarryEngine