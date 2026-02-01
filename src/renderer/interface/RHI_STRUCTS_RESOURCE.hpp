#pragma once
#include"RHI_ENUMS.hpp"
#include"RHI_STRUCTS_DESC.hpp"
#include <memory>
#include <vector>
#include <functional>
#include <string>
#include <glm/glm.hpp>


namespace StarryEngine::RHI {
    // ==================== 资源基类接口 ====================
    class IResource {
    public:
        virtual ~IResource() = default;

        virtual void release() = 0;
        virtual bool isValid() const = 0;
        virtual void* getNativeHandle() const = 0;
        virtual size_t getMemoryUsage() const = 0;
        virtual const char* getTypeName() const = 0;

        virtual bool operator==(const IResource& other) const {
            return getNativeHandle() == other.getNativeHandle();
        }
    };

    // ==================== 缓冲区接口 ====================
    class RHIBuffer : public IResource {
    public:
        virtual ~RHIBuffer() = default;

        // 映射/取消映射
        virtual void* map(uint64_t offset = 0, uint64_t size = 0) = 0;
        virtual void unmap() = 0;

        // 数据更新
        virtual void update(const void* data, uint64_t size, uint64_t offset = 0) = 0;
        virtual void flush(uint64_t offset = 0, uint64_t size = 0) = 0;
        virtual void invalidate(uint64_t offset = 0, uint64_t size = 0) = 0;

        // 信息查询
        virtual BufferType getType() const = 0;
        virtual uint64_t getSize() const = 0;
        virtual uint64_t getAlignment() const = 0;
        virtual bool isCPUVisible() const = 0;
        virtual bool isGPUOnly() const = 0;
        virtual bool isPersistentMapped() const = 0;

        // 视图创建
        virtual void* createView(Format format, uint64_t offset = 0, uint64_t size = 0) = 0;
        virtual void destroyView(void* view) = 0;

        // 屏障
        virtual void transitionState(AccessFlag newAccess, PipelineStage newStage) = 0;
    };

    // ==================== 纹理接口 ====================
    class RHITexture : public IResource {
    public:
        virtual ~RHITexture() = default;

        // 基本属性
        virtual TextureType getType() const = 0;
        virtual Format getFormat() const = 0;
        virtual Extent3D getExtent() const = 0;
        virtual uint32_t getMipLevels() const = 0;
        virtual uint32_t getArrayLayers() const = 0;
        virtual uint32_t getSampleCount() const = 0;
        virtual ImageLayout getCurrentLayout() const = 0;

        // 视图创建
        virtual void* createView(
            ImageAspect aspect = ImageAspect::Color,
            uint32_t baseMipLevel = 0,
            uint32_t levelCount = 1,
            uint32_t baseArrayLayer = 0,
            uint32_t layerCount = 1) = 0;

        virtual void destroyView(void* view) = 0;

        // Mipmap生成
        virtual void generateMipmaps() = 0;

        // 布局转换
        virtual void transitionLayout(
            ImageLayout newLayout,
            PipelineStage srcStage,
            PipelineStage dstStage,
            AccessFlag srcAccess,
            AccessFlag dstAccess,
            uint32_t baseMipLevel = 0,
            uint32_t levelCount = 1,
            uint32_t baseArrayLayer = 0,
            uint32_t layerCount = 1) = 0;

        // 拷贝操作
        virtual void copyFromBuffer(
            RHIBuffer* buffer,
            const std::vector<BufferImageCopyRegion>& regions) = 0;

        virtual void copyToBuffer(
            RHIBuffer* buffer,
            const std::vector<BufferImageCopyRegion>& regions) = 0;

        virtual void copyFromTexture(
            RHITexture* texture,
            const std::vector<ImageCopyRegion>& regions) = 0;
    };

    // ==================== 采样器接口 ====================
    class RHISampler : public IResource {
    public:
        virtual ~RHISampler() = default;

        virtual const SamplerDesc& getDesc() const = 0;
    };

    // ==================== 着色器模块接口 ====================
    class RHIShaderModule : public IResource {
    public:
        virtual ~RHIShaderModule() = default;

        virtual ShaderStage getStage() const = 0;
        virtual const std::string& getEntryPoint() const = 0;
        //virtual const std::vector<uint8_t>& getBytecode() const = 0;

        virtual void release() = 0;

        // 反射信息
        virtual bool hasReflectionData() const = 0;
        virtual const void* getReflectionData() const = 0;

        // 编译选项
        virtual void setDefines(const std::vector<std::string>& defines) = 0;
        virtual void setIncludePaths(const std::vector<std::string>& includePaths) = 0;

        // 重新编译（热重载）
        virtual bool recompile(const std::vector<uint8_t>& newBytecode) = 0;
    };

    // ==================== 管线布局接口 ====================
    class RHIPipelineLayout : public IResource {
    public:
        virtual ~RHIPipelineLayout() = default;

        virtual const PipelineLayoutDesc& getDesc() const = 0;

        // 描述符集布局
        virtual uint32_t getDescriptorSetCount() const = 0;
        virtual const std::vector<DescriptorSetLayoutBinding>& getDescriptorSetLayout(uint32_t set) const = 0;

        // 推送常量
        virtual uint32_t getPushConstantRangeCount() const = 0;
        virtual const PushConstantRange& getPushConstantRange(uint32_t index) const = 0;

        // 绑定点
        virtual uint32_t getBindingPoint(uint32_t set, uint32_t binding) const = 0;
    };

    // ==================== 管线接口 ====================
    class RHIPipeline : public IResource {
    public:
        virtual ~RHIPipeline() = default;

        virtual PipelineType getType() const = 0;
        virtual RHIPipelineLayout* getLayout() const = 0;

        // 状态查询
        virtual bool isComputePipeline() const = 0;
        virtual bool isGraphicsPipeline() const = 0;
        virtual bool isRayTracingPipeline() const = 0;

        // 热重载支持
        virtual bool canBeReloaded() const = 0;
        virtual bool reload(const void* newPipelineData) = 0;
    };

    // ==================== 渲染通道接口 ====================
    class RHIRenderPass : public IResource {
    public:
        virtual ~RHIRenderPass() = default;

        virtual const RenderPassDesc& getDesc() const = 0;
        virtual uint32_t getAttachmentCount() const = 0;
        virtual uint32_t getSubpassCount() const = 0;

        // 兼容性检查
        virtual bool isCompatibleWith(const RHIRenderPass* other) const = 0;
    };

    // ==================== 帧缓冲接口 ====================
    class RHIFramebuffer : public IResource {
    public:
        virtual ~RHIFramebuffer() = default;

        virtual const FramebufferDesc& getDesc() const = 0;
        virtual RHIRenderPass* getRenderPass() const = 0;
        virtual Extent2D getExtent() const = 0;
        virtual uint32_t getLayerCount() const = 0;

        // 附件访问
        virtual uint32_t getAttachmentCount() const = 0;
        virtual RHITexture* getAttachment(uint32_t index) const = 0;
    };

    // ==================== 命令缓冲区接口 ====================
    class RHICommandBuffer : public IResource {
    public:
        virtual ~RHICommandBuffer() = default;

        // 生命周期
        virtual void begin(const CommandBufferDesc& desc = {}) = 0;
        virtual void end() = 0;
        virtual void reset(bool releaseResources = false) = 0;

        // 状态设置
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

        // 管线绑定
        virtual void bindPipeline(RHIPipeline* pipeline) = 0;
        virtual void bindVertexBuffers(
            uint32_t firstBinding,
            const std::vector<RHIBuffer*>& buffers,
            const std::vector<uint64_t>& offsets) = 0;
        virtual void bindIndexBuffer(
            RHIBuffer* buffer,
            uint64_t offset,
            IndexType indexType) = 0;

        // 描述符集绑定
        virtual void bindDescriptorSets(
            PipelineBindPoint bindPoint,
            RHIPipelineLayout* layout,
            uint32_t firstSet,
            const std::vector<DescriptorSetHandle>& descriptorSets,
            const std::vector<uint32_t>& dynamicOffsets = {}) = 0;

        // 推送常量
        virtual void pushConstants(
            RHIPipelineLayout* layout,
            ShaderStage stage,
            uint32_t offset,
            uint32_t size,
            const void* values) = 0;

        // 绘图命令
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
            RHIBuffer* buffer,
            uint64_t offset,
            uint32_t drawCount,
            uint32_t stride) = 0;

        virtual void drawIndexedIndirect(
            RHIBuffer* buffer,
            uint64_t offset,
            uint32_t drawCount,
            uint32_t stride) = 0;

        virtual void drawIndirectCount(
            RHIBuffer* buffer,
            uint64_t offset,
            RHIBuffer* countBuffer,
            uint64_t countOffset,
            uint32_t maxDrawCount,
            uint32_t stride) = 0;

        virtual void drawIndexedIndirectCount(
            RHIBuffer* buffer,
            uint64_t offset,
            RHIBuffer* countBuffer,
            uint64_t countOffset,
            uint32_t maxDrawCount,
            uint32_t stride) = 0;

        // 计算命令
        virtual void dispatch(
            uint32_t groupCountX,
            uint32_t groupCountY = 1,
            uint32_t groupCountZ = 1) = 0;

        virtual void dispatchIndirect(
            RHIBuffer* buffer,
            uint64_t offset) = 0;

        // 光线追踪命令
        virtual void traceRays(
            RHIBuffer* raygenTable,
            RHIBuffer* missTable,
            RHIBuffer* hitTable,
            RHIBuffer* callableTable,
            uint32_t width,
            uint32_t height,
            uint32_t depth) = 0;

        virtual void buildAccelerationStructure(
            const AccelerationStructureBuildInfo& buildInfo,
            RHIBuffer* scratchBuffer,
            uint64_t scratchOffset) = 0;

        virtual void copyAccelerationStructure(
            RHIBuffer* src,
            RHIBuffer* dst,
            CopyAccelerationStructureMode mode) = 0;

        // 渲染通道
        virtual void beginRenderPass(
            const RenderPassBeginInfo& beginInfo,
            SubpassContents contents = SubpassContents::Inline) = 0;

        virtual void nextSubpass(SubpassContents contents = SubpassContents::Inline) = 0;
        virtual void endRenderPass() = 0;

        // 执行次命令缓冲区
        virtual void executeCommands(const std::vector<RHICommandBuffer*>& commandBuffers) = 0;

        // 资源屏障
        virtual void pipelineBarrier(
            PipelineStage srcStage,
            PipelineStage dstStage,
            DependencyFlags flags,
            const std::vector<MemoryBarrier>& memoryBarriers,
            const std::vector<BufferBarrier>& bufferBarriers,
            const std::vector<ImageBarrier>& imageBarriers) = 0;

        // 拷贝操作
        virtual void copyBuffer(
            RHIBuffer* src,
            RHIBuffer* dst,
            const std::vector<BufferCopyRegion>& regions) = 0;

        virtual void copyImage(
            RHITexture* src,
            RHITexture* dst,
            const std::vector<ImageCopyRegion>& regions) = 0;

        virtual void copyBufferToImage(
            RHIBuffer* src,
            RHITexture* dst,
            const std::vector<BufferImageCopyRegion>& regions) = 0;

        virtual void copyImageToBuffer(
            RHITexture* src,
            RHIBuffer* dst,
            const std::vector<BufferImageCopyRegion>& regions) = 0;

        virtual void blitImage(
            RHITexture* src,
            ImageLayout srcLayout,
            RHITexture* dst,
            ImageLayout dstLayout,
            const std::vector<ImageBlitRegion>& regions,
            Filter filter) = 0;

        // 清除操作
        virtual void clearColorImage(
            RHITexture* image,
            ImageLayout layout,
            const Color& color,
            const std::vector<ImageSubresourceRange>& ranges) = 0;

        virtual void clearDepthStencilImage(
            RHITexture* image,
            ImageLayout layout,
            float depth,
            uint32_t stencil,
            const std::vector<ImageSubresourceRange>& ranges) = 0;

        virtual void clearAttachments(
            const std::vector<ClearAttachment>& attachments,
            const std::vector<ClearRect>& rects) = 0;

        // 填充缓冲区
        virtual void fillBuffer(
            RHIBuffer* buffer,
            uint64_t offset,
            uint64_t size,
            uint32_t data) = 0;

        // 更新缓冲区
        virtual void updateBuffer(
            RHIBuffer* buffer,
            uint64_t offset,
            uint64_t size,
            const void* data) = 0;

        // 查询操作
        virtual void beginQuery(
            QueryPoolHandle queryPool,
            uint32_t query,
            QueryControlFlags flags = {}) = 0;

        virtual void endQuery(
            QueryPoolHandle queryPool,
            uint32_t query) = 0;

        virtual void writeTimestamp(
            PipelineStage stage,
            QueryPoolHandle queryPool,
            uint32_t query) = 0;

        virtual void resetQueryPool(
            QueryPoolHandle queryPool,
            uint32_t firstQuery,
            uint32_t queryCount) = 0;

        virtual void copyQueryPoolResults(
            QueryPoolHandle queryPool,
            uint32_t firstQuery,
            uint32_t queryCount,
            RHIBuffer* dstBuffer,
            uint64_t dstOffset,
            uint64_t stride,
            QueryResultFlags flags) = 0;

        // 调试标记
        virtual void beginDebugLabel(const char* label, const float color[4]) = 0;
        virtual void endDebugLabel() = 0;
        virtual void insertDebugLabel(const char* label, const float color[4]) = 0;
    };

    // ==================== 命令池接口 ====================
    class RHICommandPool : public IResource {
    public:
        virtual ~RHICommandPool() = default;

        virtual std::vector<RHICommandBuffer*> allocateCommandBuffers(
            uint32_t count,
            CommandBufferLevel level = CommandBufferLevel::Primary) = 0;

        virtual void freeCommandBuffers(const std::vector<RHICommandBuffer*>& commandBuffers) = 0;
        virtual void reset(bool releaseResources = false) = 0;

        virtual QueueType getQueueType() const = 0;
    };

    // ==================== 栅栏接口 ====================
    class RHIFence : public IResource {
    public:
        virtual ~RHIFence() = default;

        virtual bool wait(uint64_t timeout = UINT64_MAX) = 0;
        virtual void reset() = 0;
        virtual bool isSignaled() const = 0;

        virtual uint64_t getValue() const = 0;
        virtual void signal(uint64_t value) = 0;
        virtual bool waitForValue(uint64_t value, uint64_t timeout = UINT64_MAX) = 0;
    };

    // ==================== 信号量接口 ====================
    class RHISemaphore : public IResource {
    public:
        virtual ~RHISemaphore() = default;

        virtual uint64_t getValue() const = 0;
        virtual void signal(uint64_t value) = 0;
        virtual bool wait(uint64_t value, uint64_t timeout = UINT64_MAX) = 0;
    };

    // ==================== 事件接口 ====================
    class RHIEvent : public IResource {
    public:
        virtual ~RHIEvent() = default;

        virtual bool isSet() const = 0;
        virtual void set() = 0;
        virtual void reset() = 0;
    };

    // ==================== 查询池接口 ====================
    class RHIQueryPool : public IResource {
    public:
        virtual ~RHIQueryPool() = default;

        virtual QueryType getType() const = 0;
        virtual uint32_t getCount() const = 0;

        virtual std::vector<uint64_t> getResults(
            uint32_t firstQuery,
            uint32_t queryCount,
            uint64_t stride,
            QueryResultFlags flags) const = 0;
    };

    // ==================== 加速结构接口 ====================
    class RHIAccelerationStructure : public IResource {
    public:
        virtual ~RHIAccelerationStructure() = default;

        virtual AccelerationStructureType getType() const = 0;
        virtual uint64_t getSize() const = 0;
        virtual uint64_t getBuildScratchSize() const = 0;
        virtual uint64_t getUpdateScratchSize() const = 0;

        virtual void* getDeviceAddress() const = 0;
    };

    // ==================== 描述符集布局接口 ====================
    class RHIDescriptorSetLayout : public IResource {
    public:
        virtual ~RHIDescriptorSetLayout() = default;

        virtual const std::vector<DescriptorSetLayoutBinding>& getBindings() const = 0;
        virtual uint32_t getBindingCount() const = 0;

        virtual bool isCompatibleWith(const RHIDescriptorSetLayout* other) const = 0;
    };

    // ==================== 描述符池接口 ====================
    class RHIDescriptorPool : public IResource {
    public:
        virtual ~RHIDescriptorPool() = default;

        virtual std::vector<DescriptorSetHandle> allocateDescriptorSets(
            const std::vector<RHIDescriptorSetLayout*>& layouts) = 0;

        virtual void freeDescriptorSets(const std::vector<DescriptorSetHandle>& descriptorSets) = 0;
        virtual void reset() = 0;

        virtual uint32_t getMaxSets() const = 0;
        virtual uint32_t getRemainingSets() const = 0;
    };

    // ==================== 描述符集接口 ====================
    class RHIDescriptorSet : public IResource {
    public:
        virtual ~RHIDescriptorSet() = default;

        virtual void writeBuffer(
            uint32_t binding,
            uint32_t arrayElement,
            RHIBuffer* buffer,
            uint64_t offset = 0,
            uint64_t range = 0) = 0;

        virtual void writeTexture(
            uint32_t binding,
            uint32_t arrayElement,
            RHITexture* texture,
            RHISampler* sampler = nullptr,
            ImageLayout layout = ImageLayout::ShaderReadOnly) = 0;

        virtual void writeSampler(
            uint32_t binding,
            uint32_t arrayElement,
            RHISampler* sampler) = 0;

        virtual void writeAccelerationStructure(
            uint32_t binding,
            uint32_t arrayElement,
            RHIAccelerationStructure* accelerationStructure) = 0;

        virtual void writeInlineUniformBlock(
            uint32_t binding,
            uint32_t offset,
            uint32_t size,
            const void* data) = 0;

        virtual void update() = 0;
        virtual void copyFrom(const RHIDescriptorSet* src, const std::vector<DescriptorCopy>& copies) = 0;
    };

    // ==================== 交换链接口 ====================
    class RHISwapChain : public IResource {
    public:
        virtual ~RHISwapChain() = default;

        virtual bool acquireNextImage(
            uint64_t timeout = UINT64_MAX,
            RHISemaphore* semaphore = nullptr,
            RHIFence* fence = nullptr) = 0;

        virtual bool present(
            const std::vector<RHISemaphore*>& waitSemaphores = {}) = 0;

        virtual bool resize(uint32_t width, uint32_t height) = 0;
        virtual void setVSync(bool enabled) = 0;
        virtual void setFullscreen(bool enabled) = 0;
        virtual void setHDR(bool enabled) = 0;

        virtual uint32_t getCurrentImageIndex() const = 0;
        virtual RHITexture* getCurrentImage() const = 0;
        virtual RHITexture* getImage(uint32_t index) const = 0;
        virtual uint32_t getImageCount() const = 0;

        virtual Extent2D getExtent() const = 0;
        virtual Format getFormat() const = 0;
        virtual ColorSpace getColorSpace() const = 0;
        virtual bool isVSyncEnabled() const = 0;
        virtual bool isFullscreen() const = 0;
        virtual bool isHDREnabled() const = 0;
    };

    // ==================== 队列接口 ====================
    class RHIQueue : public IResource {
    public:
        virtual ~RHIQueue() = default;

        virtual QueueType getType() const = 0;
        virtual uint32_t getFamilyIndex() const = 0;
        virtual uint32_t getIndex() const = 0;
        virtual uint64_t getTimestampFrequency() const = 0;

        virtual void submit(
            const std::vector<SubmitInfo>& submits,
            RHIFence* fence = nullptr) = 0;

        virtual void present(const PresentInfo& presentInfo) = 0;
        virtual void waitIdle() = 0;

        virtual uint64_t getLastSubmitId() const = 0;
        virtual bool isSameFamily(const RHIQueue* other) const = 0;
    };
}