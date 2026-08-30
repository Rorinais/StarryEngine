#pragma once
#include <interface/vulkan/VulkanDevice.hpp>
#include <interface/vulkan/VulkanResources.hpp>
#include <interface/RHIManager.hpp>
#include <interface/RHICommandEncoder.hpp>

namespace StarryEngine::RHI {

   class VulkanCommandEncoder : public RHICommandEncoder {
    public:
		VulkanCommandEncoder(std::shared_ptr<VulkanDevice> device, VulkanCommandBuffer* commandBuffer, ResourceManager* ptr) :
			mDevice(device), mCommandBuffer(commandBuffer), mResourceManager(ptr),mEnded(false) {
            if (mCommandBuffer != nullptr) mCommandBuffer->begin();
		}

        VulkanCommandEncoder(std::shared_ptr<VulkanDevice> device, VkCommandBuffer cmdBuf, ResourceManager* ptr)
            : mDevice(device), mCommandBuffer(nullptr), mResourceManager(ptr), mEnded(false), m_vkCmdBuf(cmdBuf) {
        }

        ~VulkanCommandEncoder() override { if (mCommandBuffer != nullptr && !mEnded) mCommandBuffer->end(); }

		void end() override {
            if (mCommandBuffer != nullptr && !mEnded) {
                mCommandBuffer->end();
                mEnded = true;
            }
            else if (m_vkCmdBuf != VK_NULL_HANDLE && !mEnded) {
                vkEndCommandBuffer(m_vkCmdBuf);
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
        void bindGraphicPipeline(PipelineHandle pipeline)override;

        void bindComputePipeline(PipelineHandle pipeline)override;

        void bindVertexBuffers(
            uint32_t firstBinding,
            const std::vector<BufferHandle>& buffers,
            const std::vector<uint64_t>& offsets)override;
        void bindIndexBuffer(
            BufferHandle buffer,
            uint64_t offset,
            IndexType indexType)override;

        // 描述符集绑定
        void bindDescriptorSets(
            PipelineBindPoint bindPoint,
            PipelineLayoutHandle layout,
            uint32_t firstSet,
            const std::vector<DescriptorSetHandle>& descriptorSets,
            const std::vector<uint32_t>& dynamicOffsets = {})override;

        // 推送常量
        void pushConstants(
            PipelineLayoutHandle layout,
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
            BufferHandle buffer,
            uint64_t offset,
            uint32_t drawCount,
            uint32_t stride)override;

        void drawIndexedIndirect(
            BufferHandle buffer,
            uint64_t offset,
            uint32_t drawCount,
            uint32_t stride)override;

        void drawIndirectCount(
            BufferHandle buffer,
            uint64_t offset,
            BufferHandle countBuffer,
            uint64_t countOffset,
            uint32_t maxDrawCount,
            uint32_t stride)override;

        void drawIndexedIndirectCount(
            BufferHandle buffer,
            uint64_t offset,
            BufferHandle countBuffer,
            uint64_t countOffset,
            uint32_t maxDrawCount,
            uint32_t stride)override;

        // 计算命令
        void dispatch(
            uint32_t groupCountX,
            uint32_t groupCountY = 1,
            uint32_t groupCountZ = 1)override;

        void dispatchIndirect(
            BufferHandle buffer,
            uint64_t offset)override;

        // 光线追踪命令
        void traceRays(
            BufferHandle raygenTable,
            BufferHandle missTable,
            BufferHandle hitTable,
            BufferHandle callableTable,
            uint32_t width,
            uint32_t height,
            uint32_t depth)override;

        // 渲染通道
        void beginRenderPass(
            const RenderPassBeginInfo& beginInfo,
            SubpassContents contents = SubpassContents::Inline)override;

        void nextSubpass(SubpassContents contents = SubpassContents::Inline)override;
        void endRenderPass()override;

        // 动态渲染（VK_KHR_dynamic_rendering）
        void beginRendering(const RenderingInfo& renderingInfo) override;
        void endRendering() override;

        // secondary 命令缓冲：以继承信息开始录制（并行命令录制用）
        void beginSecondary(const SecondaryCommandBufferBeginInfo& beginInfo)override;

        // 执行次命令缓冲区（native 句柄）
        void executeCommands(const std::vector<void*>& nativeCommandBuffers)override;

        // 资源屏障
        void pipelineBarrier(
            PipelineStage srcStage,
            PipelineStage dstStage,
            DependencyFlags flags,
            const std::vector<ImageMemoryBarrier>& memoryBarriers,
            const std::vector<BufferBarrier>& bufferBarriers,
            const std::vector<ImageBarrier>& imageBarriers)override;

        // 拷贝操作
        void copyBuffer(
            BufferHandle src,
            BufferHandle dst,
            const std::vector<BufferCopyRegion>& regions)override;

        void copyImage(
            TextureHandle src,
            TextureHandle dst,
            const std::vector<ImageCopyRegion>& regions)override;

        void copyBufferToImage(
            BufferHandle src,
            TextureHandle dst,
            const std::vector<BufferImageCopyRegion>& regions)override;

        void copyImageToBuffer(
            TextureHandle src,
            BufferHandle dst,
            const std::vector<BufferImageCopyRegion>& regions)override;

        void blitImage(
            TextureHandle src,
            ImageLayout srcLayout,
            TextureHandle dst,
            ImageLayout dstLayout,
            const std::vector<ImageBlitRegion>& regions,
            Filter filter)override;

        // 清除操作
        void clearColorImage(
            TextureHandle image,
            ImageLayout layout,
            const Color& color,
            const std::vector<ImageSubresourceRange>& ranges)override;

        void clearDepthStencilImage(
            TextureHandle image,
            ImageLayout layout,
            float depth,
            uint32_t stencil,
            const std::vector<ImageSubresourceRange>& ranges)override;

        void clearAttachments(
            const std::vector<ClearAttachment>& attachments,
            const std::vector<ClearRect>& rects)override;

        // 填充缓冲区
        void fillBuffer(
            BufferHandle buffer,
            uint64_t offset,
            uint64_t size,
            uint32_t data)override;

        // 更新缓冲区
        void updateBuffer(
            BufferHandle buffer,
            uint64_t offset,
            uint64_t size,
            const void* data)override;

        // 调试标记
        void beginDebugLabel(const char* label, const float color[4])override;
        void endDebugLabel()override;
        void insertDebugLabel(const char* label, const float color[4]) override;
        void* getCommandBuffer() { return reinterpret_cast<void*>(m_vkCmdBuf); }

    private:
        VkCommandBuffer getVkCommandBuffer() const { return m_vkCmdBuf; }
	private:
			std::shared_ptr<VulkanDevice> mDevice;
			VulkanCommandBuffer* mCommandBuffer;
			ResourceManager* mResourceManager;
			bool mEnded = false;

            VkCommandBuffer m_vkCmdBuf = VK_NULL_HANDLE;
    };
}