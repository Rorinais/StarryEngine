    
#pragma once

#include <interface/RHIEnums.hpp>
#include <interface/RHIHandles.hpp>
#include <interface/RHIStructs.hpp>
#include <interface/IRHIResources.hpp>

namespace StarryEngine::RHI {

    class RHICommandEncoder {
    public:
		virtual ~RHICommandEncoder() = default;
		virtual void end() = 0;

        virtual void setViewport(const Viewport& viewport) = 0;
        virtual void setViewports(const std::vector<Viewport>& viewports) = 0;
        virtual void setScissor(const Rect2D& scissor) = 0;
        virtual void setScissors(const std::vector<Rect2D>& scissors) = 0;
        virtual void setLineWidth(float width) = 0;
        virtual void setDepthBias(float constantFactor, float clamp, float slopeFactor) = 0;
        virtual void setBlendConstants(const float constants[4]) = 0;
        virtual void setDepthBounds(float minDepth, float maxDepth) = 0;
        virtual void setStencilCompareMask(StencilFace face, uint32_t compareMask) = 0;
        virtual void setStencilWriteMask(StencilFace face, uint32_t writeMask) = 0;
        virtual void setStencilReference(StencilFace face, uint32_t reference) = 0;

        virtual void bindGraphicPipeline(PipelineHandle pipeline) = 0;
        virtual void bindComputePipeline(PipelineHandle pipeline) = 0;
        virtual void bindVertexBuffers(uint32_t firstBinding,const std::vector<BufferHandle>& buffers,const std::vector<uint64_t>& offsets) = 0;
        virtual void bindIndexBuffer(BufferHandle buffer,uint64_t offset,IndexType indexType) = 0;

        virtual void bindDescriptorSets(
            PipelineBindPoint bindPoint,
            PipelineLayoutHandle layout,
            uint32_t firstSet,
            const std::vector<DescriptorSetHandle>& descriptorSets,
            const std::vector<uint32_t>& dynamicOffsets = {}) = 0;

        virtual void pushConstants(
            PipelineLayoutHandle layout,
            ShaderStage stage,
            uint32_t offset,
            uint32_t size,
            const void* values) = 0;

        virtual void draw(
            uint32_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t firstVertex = 0,
            uint32_t firstInstance = 0) = 0;

        virtual void drawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount = 1,
            uint32_t firstIndex = 0,
            int32_t vertexOffset = 0,
            uint32_t firstInstance = 0) = 0;

        virtual void drawIndirect(
            BufferHandle buffer,
            uint64_t offset,
            uint32_t drawCount,
            uint32_t stride) = 0;

        virtual void drawIndexedIndirect(
            BufferHandle buffer,
            uint64_t offset,
            uint32_t drawCount,
            uint32_t stride) = 0;

        virtual void drawIndirectCount(
            BufferHandle buffer,
            uint64_t offset,
            BufferHandle countBuffer,
            uint64_t countOffset,
            uint32_t maxDrawCount,
            uint32_t stride) = 0;

        virtual void drawIndexedIndirectCount(
            BufferHandle buffer,
            uint64_t offset,
            BufferHandle countBuffer,
            uint64_t countOffset,
            uint32_t maxDrawCount,
            uint32_t stride) = 0;

        virtual void dispatch(
            uint32_t groupCountX,
            uint32_t groupCountY = 1,
            uint32_t groupCountZ = 1) = 0;

        virtual void dispatchIndirect(
            BufferHandle buffer,
            uint64_t offset) = 0;

        virtual void traceRays(
            BufferHandle raygenTable,
            BufferHandle missTable,
            BufferHandle hitTable,
            BufferHandle callableTable,
            uint32_t width,
            uint32_t height,
            uint32_t depth) = 0;

        virtual void beginRenderPass(const RenderPassBeginInfo& beginInfo,SubpassContents contents = SubpassContents::Inline) = 0;
        virtual void nextSubpass(SubpassContents contents = SubpassContents::Inline) = 0;
        virtual void endRenderPass() = 0;
        virtual void beginSecondary(const SecondaryCommandBufferBeginInfo& beginInfo) = 0;
        virtual void executeCommands(const std::vector<void*>& nativeCommandBuffers) = 0;

        virtual void pipelineBarrier(
            PipelineStage srcStage,
            PipelineStage dstStage,
            DependencyFlags flags,
            const std::vector<ImageMemoryBarrier>& memoryBarriers,
            const std::vector<BufferBarrier>& bufferBarriers,
            const std::vector<ImageBarrier>& imageBarriers) = 0;

        virtual void copyBuffer(
            BufferHandle src,
            BufferHandle dst,
            const std::vector<BufferCopyRegion>& regions) = 0;

        virtual void copyImage(
            TextureHandle src,
            TextureHandle dst,
            const std::vector<ImageCopyRegion>& regions) = 0;

        virtual void copyBufferToImage(
            BufferHandle src,
            TextureHandle dst,
            const std::vector<BufferImageCopyRegion>& regions) = 0;

        virtual void copyImageToBuffer(
            TextureHandle src,
            BufferHandle dst,
            const std::vector<BufferImageCopyRegion>& regions) = 0;

        virtual void blitImage(
            TextureHandle src,
            ImageLayout srcLayout,
            TextureHandle dst,
            ImageLayout dstLayout,
            const std::vector<ImageBlitRegion>& regions,
            Filter filter) = 0;

        virtual void clearColorImage(
            TextureHandle image,
            ImageLayout layout,
            const Color& color,
            const std::vector<ImageSubresourceRange>& ranges) = 0;

        virtual void clearDepthStencilImage(
            TextureHandle image,
            ImageLayout layout,
            float depth,
            uint32_t stencil,
        const std::vector<ImageSubresourceRange>& ranges) = 0;

        virtual void clearAttachments(const std::vector<ClearAttachment>& attachments,const std::vector<ClearRect>& rects) = 0;

        virtual void fillBuffer(BufferHandle buffer,uint64_t offset,uint64_t size,uint32_t data) = 0;

        virtual void updateBuffer(BufferHandle buffer,uint64_t offset,uint64_t size,const void* data) = 0;

        virtual void beginDebugLabel(const char* label, const float color[4]) = 0;
        virtual void endDebugLabel() = 0;
        virtual void insertDebugLabel(const char* label, const float color[4]) = 0;

        virtual void* getCommandBuffer() = 0;
    };
}