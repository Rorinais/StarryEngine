#pragma once
#include <renderer/interface/RHI_ENUMS.hpp>
#include <renderer/interface/RHI_STRUCTS_DESC.hpp>
#include <renderer/interface/RHI_HANDLES_SYSTEM.hpp>
#include <renderer/interface/RHI_STRUCTS_CONFIG.hpp>
#include <renderer/interface/RHI_STRUCTS_RESOURCE.hpp>
#include <renderer/interface/RHI_RESOURCE_MANAGER.hpp>
#include <memory>
#include <vector>
#include <functional>
#include <string>
#include <glm/glm.hpp>

namespace StarryEngine::RHI {
#define SHADER_STAGE_FLAG(flag) (static_cast<std::underlying_type_t<ShaderStage>>(stage) & static_cast<std::underlying_type_t<ShaderStage>>(ShaderStage::flag))
    // ==================== RHI上下文接口 ====================
    class IRHIContext : public IResource {
    public:
        virtual ~IRHIContext() = default;

        virtual bool initialize(const RHIInitConfig& config) = 0;
        virtual void shutdown() = 0;

        virtual bool beginFrame() = 0;
        virtual void endFrame() = 0;
        virtual void present() = 0;
        virtual void waitIdle() = 0;
        virtual void waitForGPU() = 0;

        virtual RHIBuffer* createBuffer(const BufferDesc& desc) = 0;
        virtual RHITexture* createTexture(const TextureDesc& desc) = 0;
        virtual RHISampler* createSampler(const SamplerDesc& desc) = 0;
        virtual RHIShaderModule* createShaderModule(const ShaderModuleDesc& desc) = 0;
        virtual RHIPipelineLayout* createPipelineLayout(const PipelineLayoutDesc& desc) = 0;
        virtual RHIPipeline* createGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
        virtual RHIPipeline* createComputePipeline(const ComputePipelineDesc& desc) = 0;
        virtual RHIPipeline* createRayTracingPipeline(const RayTracingPipelineDesc& desc) = 0;
        virtual RHIRenderPass* createRenderPass(const RenderPassDesc& desc) = 0;
        virtual RHIFramebuffer* createFramebuffer(const FramebufferDesc& desc) = 0;
        virtual RHICommandPool* createCommandPool(const CommandPoolDesc& desc) = 0;
        virtual RHIFence* createFence(const FenceDesc& desc) = 0;
        virtual RHISemaphore* createSemaphore(const SemaphoreDesc& desc) = 0;
        virtual RHIEvent* createEvent(const EventDesc& desc) = 0;
        virtual RHIQueryPool* createQueryPool(const QueryPoolDesc& desc) = 0;
        virtual RHIDescriptorSetLayout* createDescriptorSetLayout(const std::vector<DescriptorSetLayoutBinding>& bindings) = 0;
        virtual RHIDescriptorPool* createDescriptorPool(const DescriptorPoolDesc& desc) = 0;
        virtual RHISwapChain* createSwapChain(const SwapChainDesc& desc) = 0;

        virtual std::vector<RHIBuffer*> createBuffers(const std::vector<BufferDesc>& descs) = 0;
        virtual std::vector<RHITexture*> createTextures(const std::vector<TextureDesc>& descs) = 0;
        virtual std::vector<RHISampler*> createSamplers(const std::vector<SamplerDesc>& descs) = 0;

        virtual void destroyBuffer(RHIBuffer* buffer) = 0;
        virtual void destroyTexture(RHITexture* texture) = 0;
        virtual void destroySampler(RHISampler* sampler) = 0;
        virtual void destroyShaderModule(RHIShaderModule* shader) = 0;
        virtual void destroyPipelineLayout(RHIPipelineLayout* layout) = 0;
        virtual void destroyPipeline(RHIPipeline* pipeline) = 0;
        virtual void destroyRenderPass(RHIRenderPass* renderPass) = 0;
        virtual void destroyFramebuffer(RHIFramebuffer* framebuffer) = 0;
        virtual void destroyCommandPool(RHICommandPool* commandPool) = 0;
        virtual void destroyFence(RHIFence* fence) = 0;
        virtual void destroySemaphore(RHISemaphore* semaphore) = 0;
        virtual void destroyEvent(RHIEvent* event) = 0;
        virtual void destroyQueryPool(RHIQueryPool* queryPool) = 0;
        virtual void destroyAccelerationStructure(RHIAccelerationStructure* accelerationStructure) = 0;
        virtual void destroyDescriptorSetLayout(RHIDescriptorSetLayout* layout) = 0;
        virtual void destroyDescriptorPool(RHIDescriptorPool* pool) = 0;
        virtual void destroySwapChain(RHISwapChain* swapChain) = 0;

        // === 资源管理 ===
        virtual void releaseResource(IResource* resource) = 0;
        virtual void retainResource(IResource* resource) = 0;
        virtual void garbageCollect() = 0;

        // === 命令提交 ===
        virtual void submitCommands(
            RHIQueue* queue,
            const std::vector<RHICommandBuffer*>& commandBuffers,
            const std::vector<RHISemaphore*>& waitSemaphores = {},
            const std::vector<PipelineStage>& waitStages = {},
            const std::vector<RHISemaphore*>& signalSemaphores = {},
            RHIFence* fence = nullptr) = 0;

        virtual void submitCompute(
            RHIQueue* queue,
            RHICommandBuffer* commandBuffer,
            RHIFence* fence = nullptr) = 0;

        virtual void submitGraphics(
            RHIQueue* queue,
            RHICommandBuffer* commandBuffer,
            RHIFence* fence = nullptr) = 0;

        // === 内存管理 ===
        virtual void* allocateMemory(uint64_t size, uint64_t alignment, MemoryType type) = 0;
        virtual void freeMemory(void* memory) = 0;
        virtual void flushMappedMemory(void* memory, uint64_t offset, uint64_t size) = 0;
        virtual void invalidateMappedMemory(void* memory, uint64_t offset, uint64_t size) = 0;

        // === 队列获取 ===
        virtual RHIQueue* getGraphicsQueue() = 0;
        virtual RHIQueue* getComputeQueue() = 0;
        virtual RHIQueue* getTransferQueue() = 0;
        virtual RHIQueue* getPresentQueue() = 0;
        virtual std::vector<RHIQueue*> getAllQueues() = 0;

        // === 交换链获取 ===
        virtual RHISwapChain* getSwapChain() = 0;

        // === 状态查询 ===
        virtual API getAPI() const = 0;
        virtual FeatureLevel getFeatureLevel() const = 0;
        virtual const std::string& getDeviceName() const = 0;
        virtual const std::string& getDriverVersion() const = 0;
        virtual uint32_t getVendorID() const = 0;
        virtual uint32_t getDeviceID() const = 0;

        virtual uint32_t getCurrentFrameIndex() const = 0;
        virtual uint32_t getCurrentImageIndex() const = 0;
        virtual uint32_t getMaxFramesInFlight() const = 0;

        virtual bool isInitialized() const = 0;
        virtual bool isVSyncEnabled() const = 0;
        virtual bool isFullscreen() const = 0;
        virtual bool isHDRSupported() const = 0;
        virtual bool isRayTracingSupported() const = 0;
        virtual bool isMeshShadingSupported() const = 0;
        virtual bool isVariableRateShadingSupported() const = 0;

        virtual bool checkFeatureSupport(Feature feature) const = 0;
        virtual bool checkFormatSupport(Format format, FormatFeatureFlags features) const = 0;

        virtual uint32_t getMaxTextureSize() const = 0;
        virtual uint32_t getMaxAnisotropy() const = 0;
        virtual uint32_t getMaxColorAttachments() const = 0;
        virtual uint32_t getMaxPushConstantSize() const = 0;
        virtual uint32_t getMaxDescriptorSets() const = 0;
        virtual uint32_t getMaxUniformBufferRange() const = 0;

        // === 统计信息 ===
        virtual uint64_t getTotalAllocatedMemory() const = 0;
        virtual uint64_t getPeakMemoryUsage() const = 0;
        virtual uint64_t getDrawCallCount() const = 0;
        virtual uint64_t getTriangleCount() const = 0;
        virtual uint64_t getDispatchCount() const = 0;
        virtual double getFrameTime() const = 0;
        virtual double getGPUTime() const = 0;
        virtual uint32_t getFPS() const = 0;

        virtual void resetStatistics() = 0;
        virtual void enableStatistics(bool enabled) = 0;

        // === 调试功能 ===
        virtual void setDebugName(IResource* resource, const std::string& name) = 0;
        virtual void beginDebugLabel(const std::string& name, const float color[4] = nullptr) = 0;
        virtual void endDebugLabel() = 0;
        virtual void insertDebugLabel(const std::string& name, const float color[4] = nullptr) = 0;

        virtual void setBreakOnError(bool enabled) = 0;
        virtual void setBreakOnWarning(bool enabled) = 0;
        virtual void setValidationLevel(ValidationLevel level) = 0;

        virtual void dumpMemoryStats() const = 0;
        virtual void dumpResourceStats() const = 0;
        virtual void dumpPipelineStats() const = 0;

        // === 回调设置 ===
        virtual void setResizeCallback(ResizeCallback callback) = 0;
        virtual void setErrorCallback(ErrorCallback callback) = 0;
        virtual void setDebugCallback(DebugCallback callback) = 0;

        // === 实用函数 ===
        virtual void transitionTextureLayout(
            RHITexture* texture,
            ImageLayout oldLayout,
            ImageLayout newLayout,
            PipelineStage srcStage,
            PipelineStage dstStage,
            AccessFlag srcAccess,
            AccessFlag dstAccess) = 0;

        virtual void copyBufferToTexture(
            RHIBuffer* buffer,
            RHITexture* texture,
            const BufferImageCopyRegion& region) = 0;

        virtual void copyTextureToBuffer(
            RHITexture* texture,
            RHIBuffer* buffer,
            const BufferImageCopyRegion& region) = 0;

        virtual void generateTextureMipmaps(RHITexture* texture) = 0;

        // === 原生句柄获取 ===
        virtual void* getNativeDevice() const = 0;
        virtual void* getNativeInstance() const = 0;
        virtual void* getNativePhysicalDevice() const = 0;
        virtual void* getNativeCommandQueue(QueueType type) const = 0;

        // === 缓存管理 ===
        virtual bool loadPipelineCache(const std::string& filename) = 0;
        virtual bool savePipelineCache(const std::string& filename) = 0;
        virtual void clearPipelineCache() = 0;

        virtual bool loadShaderCache(const std::string& directory) = 0;
        virtual bool saveShaderCache(const std::string& directory) = 0;
        virtual void clearShaderCache() = 0;

        // === 多线程支持 ===
        virtual void beginThreadContext(uint32_t threadId) = 0;
        virtual void endThreadContext(uint32_t threadId) = 0;
        virtual bool isThreadSafe() const = 0;

        // === 热重载支持 ===
        virtual void reloadShaders() = 0;
        virtual void reloadPipelines() = 0;
        virtual void reloadTextures() = 0;

        // === 性能分析 ===
        virtual void beginProfiling(const std::string& name) = 0;
        virtual void endProfiling(const std::string& name) = 0;
        virtual std::vector<ProfilingResult> getProfilingResults() const = 0;

        // === 配置更新 ===
        virtual void updateConfig(const RHIInitConfig& newConfig) = 0;
        virtual const RHIInitConfig& getConfig() const = 0;
    };

    class RHIUtils {
    public:
        // 格式支持查询
        static bool isDepthFormat(Format format);
        static bool isStencilFormat(Format format);
        static bool isDepthStencilFormat(Format format);
        static bool isCompressedFormat(Format format);
        static bool isSRGBFormat(Format format);
        static bool isIntegerFormat(Format format);
        static bool isFloatFormat(Format format);
        static bool isNormalizedFormat(Format format);

        static uint32_t getFormatSize(Format format);
        static uint32_t getFormatComponentCount(Format format);
        static Format getSRGBFormat(Format format);
        static Format getLinearFormat(Format format);
        static Format getDepthFormat(uint32_t depthBits, bool stencil);
        static std::string formatToString(Format format);

        // 内存对齐
        static uint64_t alignUp(uint64_t value, uint64_t alignment);
        static uint64_t alignDown(uint64_t value, uint64_t alignment);
        static bool isAligned(uint64_t value, uint64_t alignment);

        // 资源大小计算
        static uint64_t calculateTextureSize(const TextureDesc& desc);
        static uint64_t calculateBufferSize(const BufferDesc& desc);
        static uint32_t calculateMipLevels(uint32_t width, uint32_t height, uint32_t depth = 1);
        static Extent3D calculateMipExtent(const Extent3D& baseExtent, uint32_t mipLevel);

        // 着色器工具
        static std::vector<uint8_t> compileShader(
            const std::string& source,
            ShaderStage stage,
            API targetAPI,
            const std::string& entryPoint = "main",
            const std::vector<std::string>& defines = {},
            const std::vector<std::string>& includePaths = {});

        static bool decompileShader(
            const std::vector<uint8_t>& bytecode,
            std::string& source,
            API sourceAPI);

        static bool reflectShader(
            const std::vector<uint8_t>& bytecode,
            ShaderReflectionInfo& reflection,
            API shaderAPI);

        // 纹理加载
        static std::unique_ptr<RHITexture> loadTexture(
            IRHIContext* context,
            const std::string& filepath,
            bool generateMips = true,
            bool srgb = false);

        static std::unique_ptr<RHITexture> createTextureFromData(
            IRHIContext* context,
            const void* data,
            uint32_t width,
            uint32_t height,
            Format format,
            bool generateMips = true);

        // 模型加载和缓冲区创建
        template<typename VertexType>
        static std::unique_ptr<RHIBuffer> createVertexBuffer(
            IRHIContext* context,
            const std::vector<VertexType>& vertices,
            const std::string& name = "") {

            BufferDesc desc;
            desc.size = sizeof(VertexType) * vertices.size();
            desc.type = BufferType::Vertex;
            desc.memoryType = MemoryType::GPU_Only;
            desc.allowUpdate = false;
            desc.debugName = name;

            auto buffer = context->createBuffer(desc);
            if (buffer) {
                auto tmp = BufferDesc{
                    desc.size,                   // size
                    BufferType::Staging,         // type
                    Format::Undefined,           // format
                    MemoryType::CPU_To_GPU,      // memoryType
                    0,                           // stride
                    true,                        // allowUpdate
                    false,                       // allowReadback
                    false,                       // allowRawViews
                    false,                       // allowCounter
                    false,                       // allowIndirectArgs
                    false,                       // allowShaderAtomics
                    false,                       // persistentMapped
                    name + "_Staging"            // debugName
                };

                auto staging = context->createBuffer(tmp);

                if (staging) {
                    void* mapped = staging->map();
                    if (mapped) {
                        memcpy(mapped, vertices.data(), desc.size);
                        staging->unmap();

                        // 执行拷贝命令
                        // 这里需要命令缓冲区来执行拷贝
                        // 简化实现，实际需要完整的命令录制
                    }
                    context->destroyBuffer(staging);
                }
            }
            return std::unique_ptr<RHIBuffer>(buffer);
        }

        template<typename IndexType>
        static std::unique_ptr<RHIBuffer> createIndexBuffer(
            IRHIContext* context,
            const std::vector<IndexType>& indices,
            const std::string& name = "") {

            BufferDesc desc;
            desc.size = sizeof(IndexType) * indices.size();
            desc.type = BufferType::Index;
            desc.memoryType = MemoryType::GPU_Only;
            desc.allowUpdate = false;
            desc.debugName = name;

            auto buffer = context->createBuffer(desc);
            if (buffer) {
                // 类似顶点缓冲区的上传逻辑
            }
            return std::unique_ptr<RHIBuffer>(buffer);
        }

        template<typename T>
        static std::unique_ptr<RHIBuffer> createUniformBuffer(
            IRHIContext* context,
            const std::string& name = "") {

            BufferDesc desc;
            desc.size = sizeof(T);
            desc.type = BufferType::Uniform;
            desc.memoryType = MemoryType::CPU_To_GPU;
            desc.allowUpdate = true;
            desc.persistentMapped = true;
            desc.debugName = name;

            return std::unique_ptr<RHIBuffer>(context->createBuffer(desc));
        }

        // 管线状态预设
        static GraphicsPipelineDesc createDefaultOpaquePipeline();
        static GraphicsPipelineDesc createDefaultAlphaBlendPipeline();
        static GraphicsPipelineDesc createDefaultWireframePipeline();
        static GraphicsPipelineDesc createDefaultSkyboxPipeline();
        static GraphicsPipelineDesc createDefaultPostProcessPipeline();

        static RasterizerState createDefaultRasterizerState();
        static DepthStencilState createDefaultDepthStencilState();
        static ColorBlendState createDefaultBlendState();

        // 调试工具
        static void setDebugColor(float* color, uint32_t resourceId);
        static std::string resourceTypeToString(IResource* resource);
        static std::string shaderStageToString(ShaderStage stage);

        // 性能分析
        static void beginGPUTimestamp(IRHIContext* context, const std::string& name);
        static void endGPUTimestamp(IRHIContext* context, const std::string& name);
        static double getGPUTimestampDuration(IRHIContext* context, const std::string& name);

        // 验证和检查
        static bool validatePipelineState(const GraphicsPipelineDesc& desc);
        static bool validateResourceState(IResource* resource, const std::string& operation);
        static bool checkMemoryLeaks(IRHIContext* context);

        // 转换函数
        static Format fromVulkanFormat(void* vkFormat);
        static void* toVulkanFormat(Format format);

        static Format fromDXGIFormat(uint32_t dxgiFormat);
        static uint32_t toDXGIFormat(Format format);

        static Format fromMetalFormat(void* mtlFormat);
        static void* toMetalFormat(Format format);

        // 数学工具
        static glm::mat4 createPerspectiveMatrix(float fov, float aspect, float near, float far);
        static glm::mat4 createOrthographicMatrix(float left, float right, float bottom, float top, float near, float far);
        static glm::mat4 createViewMatrix(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up);

        // 随机工具
        static Color randomColor();
        static float randomFloat(float min = 0.0f, float max = 1.0f);
        static uint32_t randomUint(uint32_t min = 0, uint32_t max = UINT32_MAX);
    };

    class IRHI {
    public:
        virtual ~IRHI() = default;

        // 初始化
        virtual bool initialize(const RHI::RHIInitConfig& config) = 0;

        // 每帧渲染
        virtual bool renderFrame(const std::function<void(RHI::RHICommandEncoder*, uint32_t imageIndex)>& drawFunc) = 0;

        // 重建交换链（窗口大小改变时）
        virtual bool recreateSwapChain(uint32_t width, uint32_t height) = 0;

        // 获取资源管理器（用于创建缓冲、纹理等）
        virtual std::shared_ptr<RHI::ResourceManager> getResourceManager() = 0;

        // 获取帧上下文（用于同步、统计）
        virtual std::shared_ptr<FrameContext> getFrameContext() = 0;

        // 获取深度纹理句柄（如果需要）
        virtual RHI::TextureHandle getDepthTexture() const = 0;

        // 获取帧缓冲句柄列表（如果需要）
        virtual const std::vector<RHI::FramebufferHandle>& getFramebuffers() const = 0;

        virtual std::unique_ptr<RHI::RHICommandEncoder> getCommandEncoder(VkCommandBuffer cmdBuf) const = 0;

        virtual uint32_t getWidth() const = 0;
        virtual uint32_t getHeight() const = 0;

        // 等待设备空闲
        virtual void waitIdle() = 0;

        virtual void printDeviceInfo() = 0;
        virtual void printResourceStatistics() = 0;

        virtual RHI::Format getDepthFormat() const = 0;
        virtual void* getInstance()  const = 0;
        virtual void* getPhysicalDevice()  const = 0;
        virtual void* getDevice() const = 0;
        virtual uint32_t getGraphicsQueueFamilyIndex() const = 0;
        virtual void* getGraphicsQueue() const = 0;
        virtual RHI::Format getSwapChainImageFormat()  const = 0;
        virtual uint32_t getSwapChainImageCount()  const = 0;
        virtual void* getSwapChainImageView(uint32_t index) const = 0;

        // 模板方法：通过 HandleTraits 获取资源对象（需要 ResourceManager 支持）
        template<typename Handle>
        auto getResource(Handle handle) -> typename RHI::HandleTraits<Handle>::ResourceType* {
            return RHI::HandleTraits<Handle>::get(getResourceManager().get(), handle);
        }

        // 模板方法：创建资源（转发给 ResourceManager）
        template<typename Handle, typename Desc>
        Handle createResource(const Desc& desc, const std::string& name = "", const std::string& debugTag = "") {
            return RHI::HandleTraits<Handle>::create(getResourceManager().get(), desc, name, debugTag);
        }

        // 模板方法：销毁资源
        template<typename Handle>
        bool destroyResource(Handle handle) {
            return RHI::HandleTraits<Handle>::destroy(getResourceManager().get(), handle);
        }
    };


} // namespace StarryEngine::RHI