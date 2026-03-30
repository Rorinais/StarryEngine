#pragma once
#include <unordered_map>
#include <iostream>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <string>
#include <array>
#include <functional>
#include <cassert>
#include <optional>
#include <chrono>
#include <thread>

#include "RHI_HANDLES_SYSTEM.hpp"
#include "RHI_VK_RESOURCE.hpp"
#include "RHI_TO_VK_FUNC.hpp"

namespace StarryEngine::RHI {
    class ResourceManager;

    class IResourceFactory {
    public:
        virtual ~IResourceFactory() = default;

        virtual std::unique_ptr<RHIBuffer> createBuffer(const BufferDesc& desc) = 0;
        virtual std::unique_ptr<RHITexture> createTexture(const TextureDesc& desc) = 0;
        virtual std::unique_ptr<RHIPipeline> createPipeline(const GraphicsPipelineDesc& desc) = 0;
        virtual std::unique_ptr<RHIPipeline> createComputePipeline(const ComputePipelineDesc& desc) = 0;
        virtual std::unique_ptr<RHIPipelineLayout> createPipelineLayout(const PipelineLayoutDesc& desc) = 0;
        virtual std::unique_ptr<RHIShaderModule> createShader(const ShaderModuleDesc& desc) = 0;
        virtual std::unique_ptr<RHISampler> createSampler(const SamplerDesc& desc) = 0;
        virtual std::unique_ptr<RHIRenderPass> createRenderPass(const RenderPassDesc& desc) = 0;
        virtual std::unique_ptr<RHIFramebuffer> createFramebuffer(const FramebufferDesc& desc) = 0;
        virtual std::unique_ptr<RHIDescriptorSet> createDescriptorSet(const DescriptorSetDesc& desc) = 0;
        virtual std::unique_ptr<RHIDescriptorPool> createDescriptorPool(const DescriptorPoolDesc& desc) = 0;
        virtual std::unique_ptr<RHIDescriptorSetLayout> createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) = 0;
        //virtual std::unique_ptr<RHICommandBuffer> createCommandBuffer(const CommandBufferDesc& desc) = 0;
        //virtual std::unique_ptr<RHICommandPool> createCommandPool(const CommandPoolDesc& desc) = 0;
        //virtual std::unique_ptr<RHIFence> createFence(const FenceDesc& desc) = 0;
        //virtual std::unique_ptr<RHISemaphore> createSemaphore(const SemaphoreDesc& desc) = 0;
        //virtual std::unique_ptr<RHIEvent> createEvent(const EventDesc& desc) = 0;
        //virtual std::unique_ptr<RHIQueryPool> createQueryPool(const QueryPoolDesc& desc) = 0;
        //virtual std::unique_ptr<RHIAccelerationStructure> createAccelerationStructure(const AccelerationStructureDesc& desc) = 0;
        //virtual std::unique_ptr<RHISwapChain> createSwapChain(const SwapChainDesc& desc) = 0;
        //virtual std::unique_ptr<RHIQueue> createQueue(const QueueDesc& desc) = 0;

        //virtual std::vector<std::unique_ptr<RHICommandBuffer>> createCommandBuffers(uint32_t count,const CommandBufferDesc& desc) = 0;
        virtual std::vector<std::unique_ptr<RHIDescriptorSet>> createDescriptorSets(uint32_t count,const DescriptorSetDesc& desc) = 0;

        virtual void setResourceManager(ResourceManager * ptr) = 0;
    };

    class VKResourceFactory : public IResourceFactory {
    public:
        VKResourceFactory(Device::Ptr device) : mDevice(device) {}

        ~VKResourceFactory() override = default;

        std::unique_ptr<RHIBuffer> createBuffer(const BufferDesc& desc) override;

        std::unique_ptr<RHITexture> createTexture(const TextureDesc& desc) override;

        std::unique_ptr<RHIPipeline> createPipeline(const GraphicsPipelineDesc& desc) override;

        std::unique_ptr<RHIPipeline> createComputePipeline(const ComputePipelineDesc& desc) override;

        std::unique_ptr<RHIPipelineLayout> createPipelineLayout(const PipelineLayoutDesc& desc) override;

        std::unique_ptr<RHIShaderModule> createShader(const ShaderModuleDesc& desc) override;

        std::unique_ptr<RHISampler> createSampler(const SamplerDesc& desc) override;

        std::unique_ptr<RHIRenderPass> createRenderPass(const RenderPassDesc& desc) override;

        std::unique_ptr<RHIFramebuffer> createFramebuffer(const FramebufferDesc& desc) override;

        std::unique_ptr<RHIDescriptorSet> createDescriptorSet(const DescriptorSetDesc& desc) override;

        std::unique_ptr<RHIDescriptorPool> createDescriptorPool(const DescriptorPoolDesc& desc) override;

        std::unique_ptr<RHIDescriptorSetLayout> createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) override;

        //std::unique_ptr<RHICommandBuffer> createCommandBuffer(const CommandBufferDesc& desc) override;

        //std::unique_ptr<RHICommandPool> createCommandPool(const CommandPoolDesc& desc) override;

        //std::unique_ptr<RHIFence> createFence(const FenceDesc& desc) override;

        //std::unique_ptr<RHISemaphore> createSemaphore(const SemaphoreDesc& desc) override;

        //std::unique_ptr<RHIEvent> createEvent(const EventDesc& desc) override;

        //std::unique_ptr<RHIQueryPool> createQueryPool(const QueryPoolDesc& desc) override;

        //std::unique_ptr<RHIAccelerationStructure> createAccelerationStructure(const AccelerationStructureDesc& desc) override;

        //std::unique_ptr<RHISwapChain> createSwapChain(const SwapChainDesc& desc) override;

        //std::unique_ptr<RHIQueue> createQueue(const QueueDesc& desc) override;

        //std::vector<std::unique_ptr<RHICommandBuffer>> createCommandBuffers(uint32_t count, const CommandBufferDesc& desc) override;

        std::vector<std::unique_ptr<RHIDescriptorSet>> createDescriptorSets(uint32_t count, const DescriptorSetDesc& desc) override;

        void setResourceManager(ResourceManager* ptr) override;
    private:
        Device::Ptr mDevice;

        ResourceManager* mResourceManager = nullptr;
    };

    class RHI_VK_CommandEncoder : public RHICommandEncoder {
    public:
		RHI_VK_CommandEncoder(Device::Ptr device, RHI_VK_CommandBuffer* commandBuffer, ResourceManager* ptr) :
			mDevice(device), mCommandBuffer(commandBuffer), mResourceManager(ptr),mEnded(false) {
            if (mCommandBuffer != nullptr) mCommandBuffer->begin();
		}

        RHI_VK_CommandEncoder(Device::Ptr device, VkCommandBuffer cmdBuf, ResourceManager* ptr)
            : mDevice(device), mCommandBuffer(nullptr), mResourceManager(ptr), mEnded(false), m_vkCmdBuf(cmdBuf) {
        }

        ~RHI_VK_CommandEncoder() override { if (mCommandBuffer != nullptr && !mEnded) mCommandBuffer->end(); }

		void end() override {
			if (mCommandBuffer != nullptr && !mEnded) {
				mCommandBuffer->end();
				mEnded = true;
			}
		}

        // 状态设置
        void setViewport(const Viewport& viewport)override;
        void setViewports(const std::vector<Viewport>& viewports)override;
        void setScissor(const Rect2D& scissor)override;
        void setScissors(const std::vector<Rect2D>& scissors)override;
        void setLineWidth(float width)override;
        void setDepthBias(float constantFactor, float clamp, float slopeFactor)override;
        void setBlendConstants(const float constants[4])override;
        void setDepthBounds(float minDepth, float maxDepth)override;
        void setStencilCompareMask(StencilFace face, uint32_t compareMask)override;
        void setStencilWriteMask(StencilFace face, uint32_t writeMask)override;
        void setStencilReference(StencilFace face, uint32_t reference)override;

        // 管线绑定
        void bindPipeline(RHIPipeline* pipeline)override;
        void bindVertexBuffers(
            uint32_t firstBinding,
            const std::vector<RHIBuffer*>& buffers,
            const std::vector<uint64_t>& offsets)override;
        void bindIndexBuffer(
            RHIBuffer* buffer,
            uint64_t offset,
            IndexType indexType)override;

        // 描述符集绑定
        void bindDescriptorSets(
            PipelineBindPoint bindPoint,
            RHIPipelineLayout* layout,
            uint32_t firstSet,
            const std::vector<DescriptorSetHandle>& descriptorSets,
            const std::vector<uint32_t>& dynamicOffsets = {})override;

        // 推送常量
        void pushConstants(
            RHIPipelineLayout* layout,
            ShaderStage stage,
            uint32_t offset,
            uint32_t size,
            const void* values)override;

        // 绘图命令
        void draw(
            uint32_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t firstVertex = 0,
            uint32_t firstInstance = 0)override;

        void drawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount = 1,
            uint32_t firstIndex = 0,
            int32_t vertexOffset = 0,
            uint32_t firstInstance = 0)override;

        void drawIndirect(
            RHIBuffer* buffer,
            uint64_t offset,
            uint32_t drawCount,
            uint32_t stride)override;

        void drawIndexedIndirect(
            RHIBuffer* buffer,
            uint64_t offset,
            uint32_t drawCount,
            uint32_t stride)override;

        void drawIndirectCount(
            RHIBuffer* buffer,
            uint64_t offset,
            RHIBuffer* countBuffer,
            uint64_t countOffset,
            uint32_t maxDrawCount,
            uint32_t stride)override;

        void drawIndexedIndirectCount(
            RHIBuffer* buffer,
            uint64_t offset,
            RHIBuffer* countBuffer,
            uint64_t countOffset,
            uint32_t maxDrawCount,
            uint32_t stride)override;

        // 计算命令
        void dispatch(
            uint32_t groupCountX,
            uint32_t groupCountY = 1,
            uint32_t groupCountZ = 1)override;

        void dispatchIndirect(
            RHIBuffer* buffer,
            uint64_t offset)override;

        // 光线追踪命令
        void traceRays(
            RHIBuffer* raygenTable,
            RHIBuffer* missTable,
            RHIBuffer* hitTable,
            RHIBuffer* callableTable,
            uint32_t width,
            uint32_t height,
            uint32_t depth)override;

        void buildAccelerationStructure(
            const AccelerationStructureBuildInfo& buildInfo,
            RHIBuffer* scratchBuffer,
            uint64_t scratchOffset)override;

        void copyAccelerationStructure(
            RHIBuffer* src,
            RHIBuffer* dst,
            CopyAccelerationStructureMode mode)override;

        // 渲染通道
        void beginRenderPass(
            const RenderPassBeginInfo& beginInfo,
            SubpassContents contents = SubpassContents::Inline)override;

        void nextSubpass(SubpassContents contents = SubpassContents::Inline)override;
        void endRenderPass()override;

        // 执行次命令缓冲区
        void executeCommands(const std::vector<RHICommandBuffer*>& commandBuffers)override;

        // 资源屏障
        void pipelineBarrier(
            PipelineStageFlags srcStage,
            PipelineStageFlags dstStage,
            DependencyFlags flags,
            const std::vector<ImageMemoryBarrier>& memoryBarriers,
            const std::vector<BufferBarrier>& bufferBarriers,
            const std::vector<ImageBarrier>& imageBarriers)override;

        // 拷贝操作
        void copyBuffer(
            RHIBuffer* src,
            RHIBuffer* dst,
            const std::vector<BufferCopyRegion>& regions)override;

        void copyImage(
            RHITexture* src,
            RHITexture* dst,
            const std::vector<ImageCopyRegion>& regions)override;

        void copyBufferToImage(
            RHIBuffer* src,
            RHITexture* dst,
            const std::vector<BufferImageCopyRegion>& regions)override;

        void copyImageToBuffer(
            RHITexture* src,
            RHIBuffer* dst,
            const std::vector<BufferImageCopyRegion>& regions)override;

        void blitImage(
            RHITexture* src,
            ImageLayout srcLayout,
            RHITexture* dst,
            ImageLayout dstLayout,
            const std::vector<ImageBlitRegion>& regions,
            Filter filter)override;

        // 清除操作
        void clearColorImage(
            RHITexture* image,
            ImageLayout layout,
            const Color& color,
            const std::vector<ImageSubresourceRange>& ranges)override;

        void clearDepthStencilImage(
            RHITexture* image,
            ImageLayout layout,
            float depth,
            uint32_t stencil,
            const std::vector<ImageSubresourceRange>& ranges)override;

        void clearAttachments(
            const std::vector<ClearAttachment>& attachments,
            const std::vector<ClearRect>& rects)override;

        // 填充缓冲区
        void fillBuffer(
            RHIBuffer* buffer,
            uint64_t offset,
            uint64_t size,
            uint32_t data)override;

        // 更新缓冲区
        void updateBuffer(
            RHIBuffer* buffer,
            uint64_t offset,
            uint64_t size,
            const void* data)override;

        // 查询操作
        //void beginQuery(
        //    QueryPoolHandle queryPool,
        //    uint32_t query,
        //    QueryControlFlags flags = {})override;

        //void endQuery(
        //    QueryPoolHandle queryPool,
        //    uint32_t query)override;

        //void writeTimestamp(
        //    PipelineStage stage,
        //    QueryPoolHandle queryPool,
        //    uint32_t query)override;

        //void resetQueryPool(
        //    QueryPoolHandle queryPool,
        //    uint32_t firstQuery,
        //    uint32_t queryCount)override;

        //void copyQueryPoolResults(
        //    QueryPoolHandle queryPool,
        //    uint32_t firstQuery,
        //    uint32_t queryCount,
        //    RHIBuffer* dstBuffer,
        //    uint64_t dstOffset,
        //    uint64_t stride,
        //    QueryResultFlags flags)override;

        // 调试标记
        void beginDebugLabel(const char* label, const float color[4])override;
        void endDebugLabel()override;
        void insertDebugLabel(const char* label, const float color[4]) override;
        void* getCommandBuffer() { return reinterpret_cast<void*>(m_vkCmdBuf); }

        private:
        VkCommandBuffer getVkCommandBuffer() const { return m_vkCmdBuf; }
		private:
			Device::Ptr mDevice;
			RHI_VK_CommandBuffer* mCommandBuffer;
			ResourceManager* mResourceManager;
			bool mEnded = false;

            VkCommandBuffer m_vkCmdBuf = VK_NULL_HANDLE;
    };
}