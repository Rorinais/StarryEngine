#pragma once
#include "../../interface/RHI_TYPES.hpp"
#include "vulkanContext/Instance.hpp"
#include "vulkanContext/Device.hpp"
#include "vulkanContext/SwapChain.hpp"
#include "vulkanContext/FrameContext.hpp"
#include "vulkanContext/CommandBuffer.hpp"
#include "vulkanContext/Pipeline.hpp"
#include "vulkanContext/Descriptor.hpp"
#include "vulkanContext/SyncObjects.hpp"

#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <queue>
#include <set>

namespace StarryEngine::RHI {

    class VulkanRHI : public IRHIContext {
    public:
        VulkanRHI();
        ~VulkanRHI() override;

        // ==================== 初始化/清理 ====================
        bool initialize(const RHIInitConfig& config) override;
        void shutdown() override;

        // ==================== 帧管理 ====================
        bool beginFrame() override;
        void endFrame() override;
        void present() override;
        void waitIdle() override;
        void waitForGPU() override;

        // ==================== 资源工厂方法 ====================
        RHIBuffer* createBuffer(const BufferDesc& desc) override;
        RHITexture* createTexture(const TextureDesc& desc) override;
        RHISampler* createSampler(const SamplerDesc& desc) override;
        RHIShaderModule* createShaderModule(const ShaderModuleDesc& desc) override;
        RHIPipelineLayout* createPipelineLayout(const PipelineLayoutDesc& desc) override;
        RHIPipeline* createGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
        RHIPipeline* createComputePipeline(const ComputePipelineDesc& desc) override;
        RHIPipeline* createRayTracingPipeline(const RayTracingPipelineDesc& desc) override;
        RHIRenderPass* createRenderPass(const RenderPassDesc& desc) override;
        RHIFramebuffer* createFramebuffer(const FramebufferDesc& desc) override;
        RHICommandPool* createCommandPool(const CommandPoolDesc& desc) override;
        RHIFence* createFence(const FenceDesc& desc) override;
        RHISemaphore* createSemaphore(const SemaphoreDesc& desc) override;
        RHIEvent* createEvent(const EventDesc& desc) override;
        RHIQueryPool* createQueryPool(const QueryPoolDesc& desc) override;
        RHIAccelerationStructure* createAccelerationStructure(const AccelerationStructureDesc& desc) override;
        RHIDescriptorSetLayout* createDescriptorSetLayout(const std::vector<DescriptorSetLayoutBinding>& bindings) override;
        RHIDescriptorPool* createDescriptorPool(const DescriptorPoolDesc& desc) override;
        RHISwapChain* createSwapChain(const SwapChainDesc& desc) override;

        // 批量创建
        std::vector<RHIBuffer*> createBuffers(const std::vector<BufferDesc>& descs) override;
        std::vector<RHITexture*> createTextures(const std::vector<TextureDesc>& descs) override;
        std::vector<RHISampler*> createSamplers(const std::vector<SamplerDesc>& descs) override;

        // ==================== 资源销毁 ====================
        void destroyBuffer(RHIBuffer* buffer) override;
        void destroyTexture(RHITexture* texture) override;
        void destroySampler(RHISampler* sampler) override;
        void destroyShaderModule(RHIShaderModule* shader) override;
        void destroyPipelineLayout(RHIPipelineLayout* layout) override;
        void destroyPipeline(RHIPipeline* pipeline) override;
        void destroyRenderPass(RHIRenderPass* renderPass) override;
        void destroyFramebuffer(RHIFramebuffer* framebuffer) override;
        void destroyCommandPool(RHICommandPool* commandPool) override;
        void destroyFence(RHIFence* fence) override;
        void destroySemaphore(RHISemaphore* semaphore) override;
        void destroyEvent(RHIEvent* event) override;
        void destroyQueryPool(RHIQueryPool* queryPool) override;
        void destroyAccelerationStructure(RHIAccelerationStructure* accelerationStructure) override;
        void destroyDescriptorSetLayout(RHIDescriptorSetLayout* layout) override;
        void destroyDescriptorPool(RHIDescriptorPool* pool) override;
        void destroySwapChain(RHISwapChain* swapChain) override;

        // ==================== 资源管理 ====================
        void releaseResource(IResource* resource) override;
        void retainResource(IResource* resource) override;
        void garbageCollect() override;

        // ==================== 命令提交 ====================
        void submitCommands(
            RHIQueue* queue,
            const std::vector<RHICommandBuffer*>& commandBuffers,
            const std::vector<RHISemaphore*>& waitSemaphores = {},
            const std::vector<PipelineStage>& waitStages = {},
            const std::vector<RHISemaphore*>& signalSemaphores = {},
            RHIFence* fence = nullptr) override;

        void submitCompute(
            RHIQueue* queue,
            RHICommandBuffer* commandBuffer,
            RHIFence* fence = nullptr) override;

        void submitGraphics(
            RHIQueue* queue,
            RHICommandBuffer* commandBuffer,
            RHIFence* fence = nullptr) override;

        // ==================== 内存管理 ====================
        void* allocateMemory(uint64_t size, uint64_t alignment, MemoryType type) override;
        void freeMemory(void* memory) override;
        void flushMappedMemory(void* memory, uint64_t offset, uint64_t size) override;
        void invalidateMappedMemory(void* memory, uint64_t offset, uint64_t size) override;

        // ==================== 队列获取 ====================
        RHIQueue* getGraphicsQueue() override;
        RHIQueue* getComputeQueue() override;
        RHIQueue* getTransferQueue() override;
        RHIQueue* getPresentQueue() override;
        std::vector<RHIQueue*> getAllQueues() override;

        // ==================== 交换链获取 ====================
        RHISwapChain* getSwapChain() override;

        // ==================== 状态查询 ====================
        API getAPI() const override { return API::Vulkan; }
        FeatureLevel getFeatureLevel() const override;
        const std::string& getDeviceName() const override;
        const std::string& getDriverVersion() const override;
        uint32_t getVendorID() const override;
        uint32_t getDeviceID() const override;

        uint32_t getCurrentFrameIndex() const override;
        uint32_t getCurrentImageIndex() const override;
        uint32_t getMaxFramesInFlight() const override;

        bool isInitialized() const override { return mInitialized; }
        bool isVSyncEnabled() const override;
        bool isFullscreen() const override;
        bool isHDRSupported() const override;
        bool isRayTracingSupported() const override;
        bool isMeshShadingSupported() const override;
        bool isVariableRateShadingSupported() const override;

        bool checkFeatureSupport(Feature feature) const override;
        bool checkFormatSupport(Format format, FormatFeatureFlags features) const override;

        uint32_t getMaxTextureSize() const override;
        uint32_t getMaxAnisotropy() const override;
        uint32_t getMaxColorAttachments() const override;
        uint32_t getMaxPushConstantSize() const override;
        uint32_t getMaxDescriptorSets() const override;
        uint32_t getMaxUniformBufferRange() const override;

        // ==================== 统计信息 ====================
        uint64_t getTotalAllocatedMemory() const override;
        uint64_t getPeakMemoryUsage() const override;
        uint64_t getDrawCallCount() const override;
        uint64_t getTriangleCount() const override;
        uint64_t getDispatchCount() const override;
        double getFrameTime() const override;
        double getGPUTime() const override;
        uint32_t getFPS() const override;

        void resetStatistics() override;
        void enableStatistics(bool enabled) override;

        // ==================== 调试功能 ====================
        void setDebugName(IResource* resource, const std::string& name) override;
        void beginDebugLabel(const std::string& name, const float color[4] = nullptr) override;
        void endDebugLabel() override;
        void insertDebugLabel(const std::string& name, const float color[4] = nullptr) override;

        void setBreakOnError(bool enabled) override;
        void setBreakOnWarning(bool enabled) override;
        void setValidationLevel(ValidationLevel level) override;

        void dumpMemoryStats() const override;
        void dumpResourceStats() const override;
        void dumpPipelineStats() const override;

        // ==================== 回调设置 ====================
        void setResizeCallback(ResizeCallback callback) override;
        void setErrorCallback(ErrorCallback callback) override;
        void setDebugCallback(DebugCallback callback) override;

        // ==================== 实用函数 ====================
        void transitionTextureLayout(
            RHITexture* texture,
            ImageLayout oldLayout,
            ImageLayout newLayout,
            PipelineStage srcStage,
            PipelineStage dstStage,
            AccessFlag srcAccess,
            AccessFlag dstAccess) override;

        void copyBufferToTexture(
            RHIBuffer* buffer,
            RHITexture* texture,
            const BufferImageCopyRegion& region) override;

        void copyTextureToBuffer(
            RHITexture* texture,
            RHIBuffer* buffer,
            const BufferImageCopyRegion& region) override;

        void generateTextureMipmaps(RHITexture* texture) override;

        // ==================== 原生句柄获取 ====================
        void* getNativeDevice() const override;
        void* getNativeInstance() const override;
        void* getNativePhysicalDevice() const override;
        void* getNativeCommandQueue(QueueType type) const override;

        // ==================== 缓存管理 ====================
        bool loadPipelineCache(const std::string& filename) override;
        bool savePipelineCache(const std::string& filename) override;
        void clearPipelineCache() override;

        bool loadShaderCache(const std::string& directory) override;
        bool saveShaderCache(const std::string& directory) override;
        void clearShaderCache() override;

        // ==================== 多线程支持 ====================
        void beginThreadContext(uint32_t threadId) override;
        void endThreadContext(uint32_t threadId) override;
        bool isThreadSafe() const override;

        // ==================== 热重载支持 ====================
        void reloadShaders() override;
        void reloadPipelines() override;
        void reloadTextures() override;

        // ==================== 性能分析 ====================
        void beginProfiling(const std::string& name) override;
        void endProfiling(const std::string& name) override;
        std::vector<ProfilingResult> getProfilingResults() const override;

        // ==================== 配置更新 ====================
        void updateConfig(const RHIInitConfig& newConfig) override;
        const RHIInitConfig& getConfig() const override;

    private:
        // ==================== 内部数据结构 ====================
        struct VulkanBuffer : public RHIBuffer {
            VkBuffer buffer = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;
            BufferDesc desc;
            void* mappedData = nullptr;
            uint64_t allocationSize = 0;
            std::atomic<int32_t> refCount{ 0 };

            // RHIBuffer 接口实现
            uint64_t getId() const override { return reinterpret_cast<uint64_t>(this); }
            const std::string& getName() const override { static std::string empty; return empty; }
            bool isValid() const override { return buffer != VK_NULL_HANDLE; }
            size_t getMemoryUsage() const override { return allocationSize; }
            const void* getNativeHandle() const override { return reinterpret_cast<const void*>(buffer); }
            bool isReady() const override { return true; }
            bool isBusy() const override { return false; }
            void retain() override { refCount++; }
            void release() override { refCount--; }
            int32_t getRefCount() const override { return refCount.load(); }

            void* map(uint64_t offset = 0, uint64_t size = 0) override;
            void unmap() override;
            void update(const void* data, uint64_t size, uint64_t offset = 0) override;
            void flush(uint64_t offset = 0, uint64_t size = 0) override;
            void invalidate(uint64_t offset = 0, uint64_t size = 0) override;
            BufferType getType() const override { return desc.type; }
            uint64_t getSize() const override { return desc.size; }
            uint64_t getAlignment() const override { return desc.alignment; }
            bool isCPUVisible() const override { return desc.memoryType == MemoryType::HostVisible; }
            bool isGPUOnly() const override { return desc.memoryType == MemoryType::DeviceLocal; }
            bool isPersistentMapped() const override { return mappedData != nullptr; }
            void* createView(Format format, uint64_t offset = 0, uint64_t size = 0) override;
            void destroyView(void* view) override;
            void transitionState(AccessFlag newAccess, PipelineStage newStage) override;
        };

        struct VulkanTexture : public RHITexture {
            VkImage image = VK_NULL_HANDLE;
            VkImageView defaultView = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;
            TextureDesc desc;
            uint64_t allocationSize = 0;
            std::atomic<int32_t> refCount{ 0 };
            std::unordered_set<VkImageView> additionalViews;

            // RHITexture 接口实现
            uint64_t getId() const override { return reinterpret_cast<uint64_t>(this); }
            const std::string& getName() const override { static std::string empty; return empty; }
            bool isValid() const override { return image != VK_NULL_HANDLE; }
            size_t getMemoryUsage() const override { return allocationSize; }
            const void* getNativeHandle() const override { return reinterpret_cast<const void*>(image); }
            bool isReady() const override { return true; }
            bool isBusy() const override { return false; }
            void retain() override { refCount++; }
            void release() override { refCount--; }
            int32_t getRefCount() const override { return refCount.load(); }

            TextureType getType() const override { return desc.type; }
            Format getFormat() const override { return desc.format; }
            Extent3D getExtent() const override { return desc.extent; }
            uint32_t getMipLevels() const override { return desc.mipLevels; }
            uint32_t getArrayLayers() const override { return desc.arrayLayers; }
            uint32_t getSampleCount() const override { return desc.sampleCount; }
            ImageLayout getCurrentLayout() const override;

            void* createView(ImageAspect aspect, uint32_t baseMipLevel, uint32_t levelCount,
                uint32_t baseArrayLayer, uint32_t layerCount) override;
            void destroyView(void* view) override;
            void generateMipmaps() override;
            void transitionLayout(ImageLayout newLayout, PipelineStage srcStage, PipelineStage dstStage,
                AccessFlag srcAccess, AccessFlag dstAccess,
                uint32_t baseMipLevel, uint32_t levelCount,
                uint32_t baseArrayLayer, uint32_t layerCount) override;
            void copyFromBuffer(RHIBuffer* buffer, const std::vector<BufferImageCopyRegion>& regions) override;
            void copyToBuffer(RHIBuffer* buffer, const std::vector<BufferImageCopyRegion>& regions) override;
            void copyFromTexture(RHITexture* texture, const std::vector<ImageCopyRegion>& regions) override;
        };

        struct VulkanSampler : public RHISampler {
            VkSampler sampler = VK_NULL_HANDLE;
            SamplerDesc desc;
            std::atomic<int32_t> refCount{ 0 };

            // RHISampler 接口实现
            uint64_t getId() const override { return reinterpret_cast<uint64_t>(this); }
            const std::string& getName() const override { static std::string empty; return empty; }
            bool isValid() const override { return sampler != VK_NULL_HANDLE; }
            size_t getMemoryUsage() const override { return 0; }
            const void* getNativeHandle() const override { return reinterpret_cast<const void*>(sampler); }
            bool isReady() const override { return true; }
            bool isBusy() const override { return false; }
            void retain() override { refCount++; }
            void release() override { refCount--; }
            int32_t getRefCount() const override { return refCount.load(); }

            const SamplerDesc& getDesc() const override { return desc; }
        };

        // 其他 Vulkan 资源类的类似定义...
        // VulkanShaderModule, VulkanPipelineLayout, VulkanPipeline, VulkanRenderPass, etc.

    private:
        // ==================== 私有方法 ====================
        // Vulkan 类型转换
        VkFormat convertFormat(Format format) const;
        VkBufferUsageFlags convertBufferUsage(BufferUsage usage) const;
        VkImageUsageFlags convertImageUsage(const TextureDesc& desc) const;
        VkImageType convertImageType(TextureType type) const;
        VkImageViewType convertImageViewType(TextureType type, uint32_t arrayLayers) const;
        VkFilter convertFilter(SamplerFilter filter) const;
        VkSamplerAddressMode convertAddressMode(SamplerAddressMode mode) const;
        VkCompareOp convertCompareOp(CompareOp op) const;
        VkShaderStageFlagBits convertShaderStage(ShaderStage stage) const;
        VkPrimitiveTopology convertPrimitiveTopology(PrimitiveTopology topology) const;
        VkCullModeFlags convertCullMode(CullMode mode) const;
        VkBlendFactor convertBlendFactor(BlendFactor factor) const;
        VkBlendOp convertBlendOp(BlendOp op) const;
        VkSampleCountFlagBits convertSampleCount(uint32_t samples) const;

        // 创建辅助函数
        VulkanBuffer* createBufferInternal(const BufferDesc& desc);
        VulkanTexture* createTextureInternal(const TextureDesc& desc);
        VulkanSampler* createSamplerInternal(const SamplerDesc& desc);

        // 销毁辅助函数
        void destroyBufferInternal(VulkanBuffer* buffer);
        void destroyTextureInternal(VulkanTexture* texture);
        void destroySamplerInternal(VulkanSampler* sampler);

        // 同步函数
        void createSyncObjects();
        void cleanupSyncObjects();

        // 命令缓冲区辅助
        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);

        // 内存辅助
        VmaAllocationCreateFlags convertMemoryTypeToVMA(MemoryType type) const;

        // 统计更新
        void updateStatistics();

    private:
        // ==================== 成员变量 ====================
        bool mInitialized = false;
        RHIInitConfig mConfig;

        // Vulkan 组件
        Instance::Ptr mInstance;
        Device::Ptr mDevice;
        SwapChain::Ptr mSwapChain;
        FrameContext::Ptr mFrameContext;

        // 资源存储
        std::vector<std::unique_ptr<VulkanBuffer>> mBuffers;
        std::vector<std::unique_ptr<VulkanTexture>> mTextures;
        std::vector<std::unique_ptr<VulkanSampler>> mSamplers;
        std::vector<std::unique_ptr<VulkanShaderModule>> mShaderModules;
        std::vector<std::unique_ptr<VulkanPipelineLayout>> mPipelineLayouts;
        std::vector<std::unique_ptr<VulkanPipeline>> mPipelines;

        // 队列
        RHIQueue* mGraphicsQueue = nullptr;
        RHIQueue* mComputeQueue = nullptr;
        RHIQueue* mTransferQueue = nullptr;
        RHIQueue* mPresentQueue = nullptr;

        // 同步对象
        VkSemaphore mImageAvailableSemaphore = VK_NULL_HANDLE;
        VkSemaphore mRenderFinishedSemaphore = VK_NULL_HANDLE;
        VkFence mInFlightFence = VK_NULL_HANDLE;

        // 统计
        struct Statistics {
            uint64_t totalAllocatedMemory = 0;
            uint64_t peakMemoryUsage = 0;
            uint64_t drawCallCount = 0;
            uint64_t triangleCount = 0;
            uint64_t dispatchCount = 0;
            std::chrono::high_resolution_clock::time_point frameStartTime;
            double frameTime = 0.0;
            double gpuTime = 0.0;
            uint32_t fps = 0;
            std::deque<double> frameTimeHistory;
        } mStatistics;

        // 调试功能
        bool mDebugEnabled = false;
        std::mutex mDebugMutex;

        // 回调函数
        ResizeCallback mResizeCallback = nullptr;
        ErrorCallback mErrorCallback = nullptr;
        DebugCallback mDebugCallback = nullptr;
    };

} // namespace StarryEngine::RHI