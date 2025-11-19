#pragma once
#include "commandPool.hpp"
#include "commandBuffer.hpp"
#include "sync/fence.hpp"
#include "sync/semaphore.hpp"
#include <vulkan/vulkan.h>
#include <functional>
#include <unordered_map>
#include <string>
#include <memory>

namespace StarryEngine {
    struct FrameContext {
        CommandBuffer::Ptr mainCommandBuffer;
        Semaphore::Ptr imageAvailableSemaphore;
        Semaphore::Ptr renderFinishedSemaphore;
        Fence::Ptr inFlightFence; 
        std::unique_ptr<class RenderContext> renderContext;

        void initRenderContext(std::shared_ptr<LogicalDevice> device,uint32_t frameIndex) {
            renderContext = std::make_unique<RenderContext>(
                device,
                mainCommandBuffer->getHandle(),
                frameIndex
            );
        }
    };

    class RenderContext {
    public:
        RenderContext(std::shared_ptr<LogicalDevice> device, VkCommandBuffer cmd, uint32_t frameIndex);

        // 渲染通道管理
        void beginRenderPass(const VkRenderPassBeginInfo* renderPassBeginInfo, VkSubpassContents subpassContents);
        void endRenderPass();

        // 管线状态管理
        void bindGraphicsPipeline(VkPipeline pipeline);
        void bindComputePipeline(VkPipeline pipeline);
        void setViewport(const VkViewport& viewport);
        void setScissor(const VkRect2D& scissor);
        void setBlendConstants(const float constants[4]);

        // 资源绑定
        void bindVertexBuffer(VkBuffer buffer, uint32_t binding = 0, VkDeviceSize offset = 0);
        void bindVertexBuffers(const std::vector<VkBuffer>& buffers);
        void bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset = 0,
            VkIndexType indexType = VK_INDEX_TYPE_UINT32);
        void bindDescriptorSet(VkPipelineBindPoint bindPoint, VkDescriptorSet descriptorSet,
            uint32_t firstSet = 0, VkPipelineLayout layout = VK_NULL_HANDLE);
        void bindDescriptorSets(VkPipelineBindPoint bindPoint, VkPipelineLayout layout,
            uint32_t firstSet, const std::vector<VkDescriptorSet>& descriptorSets);

        // 绘制和分发命令
        void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);
        void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0,
            int32_t vertexOffset = 0, uint32_t firstInstance = 0);
        void drawIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride);
        void drawIndexedIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride);
        void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
        void dispatchIndirect(VkBuffer buffer, VkDeviceSize offset);

        // 获取底层对象
        VkCommandBuffer getCommandBuffer() const { return mCommandBuffer; }
        uint32_t getFrameIndex() const { return mFrameIndex; }
        std::shared_ptr<LogicalDevice> getLogicalDevice() const { return mDevice; }
        VkDevice getDevice() const { return mDevice->getHandle(); }
    private:
        std::shared_ptr<LogicalDevice> mDevice;
        VkCommandBuffer mCommandBuffer;
        uint32_t mFrameIndex;
    };
} // namespace StarryEngine