#pragma once
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <queue>

#include "../../interface/IRHI.hpp"
#include "./vulkanContext/Instance.hpp"
#include "./vulkanContext/Device.hpp"
#include "./vulkanContext/SwapChain.hpp"
#include "./vulkanContext/FrameContext.hpp"

namespace StarryEngine::RHI {
    class VulkanBackend : public IRHI {
    private:
        struct BufferData {
            VkBuffer buffer = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;
            BufferDesc desc;
            void* mappedData = nullptr;
            uint64_t allocationSize = 0;
        };

        struct TextureData {
            VkImage image = VK_NULL_HANDLE;
            VkImageView view = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;
            TextureDesc desc;
            uint64_t allocationSize = 0;
        };

        struct SamplerData {
            VkSampler sampler = VK_NULL_HANDLE;
            SamplerDesc desc;
        };

        struct ShaderModuleData {
            VkShaderModule module = VK_NULL_HANDLE;
            ShaderModuleDesc desc;
        };

        struct PipelineLayoutData {
            VkPipelineLayout layout = VK_NULL_HANDLE;
            VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
            PipelineLayoutDesc desc;
        };

        struct PipelineData {
            VkPipeline pipeline = VK_NULL_HANDLE;
            PipelineLayoutHandle layoutHandle;
            bool isCompute = false;
            std::string debugName;
        };

        // 内部命令状态
        struct CommandState {
            VkCommandBuffer currentCommandBuffer = VK_NULL_HANDLE;
            PipelineHandle boundPipeline;
            std::vector<BufferHandle> boundVertexBuffers;
            BufferHandle boundIndexBuffer;
            bool isIndexBuffer32Bit = false;
            uint64_t indexBufferOffset = 0;
        };

    public:
        struct Config {
            Instance::Config instanceConfig;
            Device::Config deviceConfig;
            SwapChainConfig swapChainConfig;
            FrameContext::Config frameContextConfig;

            void* nativeWindowHandle = nullptr;
            uint32_t initialWidth = 1280;
            uint32_t initialHeight = 720;

            bool enableDebugMarkers = true;
            bool enableMemoryTracking = true;
            bool enablePipelineCache = true;

            uint32_t maxFramesInFlight = 2;
            bool enableVSync = true;
            bool enableValidationLayers = true;

            Config() {
                instanceConfig.requiredExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
            }
        };

        VulkanBackend();
        ~VulkanBackend() override;

        // IRHI 接口实现
        bool initialize(void* windowHandle, uint32_t width, uint32_t height) override;
        void shutdown() override;

        bool beginFrame() override;
        void endFrame() override;
        void present() override;
        void waitIdle() override;

        BufferHandle createBuffer(const BufferDesc& desc) override;
        void destroyBuffer(BufferHandle handle) override;

        TextureHandle createTexture(const TextureDesc& desc) override;
        void destroyTexture(TextureHandle handle) override;

        SamplerHandle createSampler(const SamplerDesc& desc) override;
        void destroySampler(SamplerHandle handle) override;

        ShaderModuleHandle createShaderModule(const ShaderModuleDesc& desc) override;
        void destroyShaderModule(ShaderModuleHandle handle) override;

        PipelineLayoutHandle createPipelineLayout(const PipelineLayoutDesc& desc) override;
        void destroyPipelineLayout(PipelineLayoutHandle handle) override;

        PipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
        PipelineHandle createComputePipeline(const ComputePipelineDesc& desc) override;
        void destroyPipeline(PipelineHandle handle) override;

        void updateBuffer(BufferHandle handle, const void* data, uint64_t offset, uint64_t size) override;
        void* mapBuffer(BufferHandle handle, uint64_t offset, uint64_t size) override;
        void unmapBuffer(BufferHandle handle) override;

        void generateTextureMips(TextureHandle handle) override;

        void beginCommandList() override;
        void endCommandList() override;

        void setViewport(const Viewport& viewport) override;
        void setScissor(const Rect& scissor) override;

        void clearColor(TextureHandle target, const float color[4]) override;
        void clearDepthStencil(TextureHandle target, float depth, uint8_t stencil) override;

        void setPipeline(PipelineHandle pipeline) override;
        void setVertexBuffer(BufferHandle buffer, uint32_t slot, uint64_t offset) override;
        void setIndexBuffer(BufferHandle buffer, uint64_t offset, bool use32Bit) override;
        void setConstantBuffer(uint32_t set, uint32_t binding, BufferHandle buffer) override;
        void setTexture(uint32_t set, uint32_t binding, TextureHandle texture) override;
        void setSampler(uint32_t set, uint32_t binding, SamplerHandle sampler) override;

        void setRenderTargets(const std::vector<TextureHandle>& colorTargets,
            TextureHandle depthStencil) override;

        void draw(uint32_t vertexCount, uint32_t instanceCount,
            uint32_t firstVertex, uint32_t firstInstance) override;
        void drawIndexed(uint32_t indexCount, uint32_t instanceCount,
            uint32_t firstIndex, int32_t vertexOffset,
            uint32_t firstInstance) override;

        void dispatch(uint32_t groupCountX, uint32_t groupCountY,
            uint32_t groupCountZ) override;

        void copyBuffer(BufferHandle src, BufferHandle dst, uint64_t size,
            uint64_t srcOffset, uint64_t dstOffset) override;
        void copyBufferToTexture(BufferHandle src, TextureHandle dst) override;
        void copyTextureToBuffer(TextureHandle src, BufferHandle dst) override;

        uint32_t getCurrentFrameIndex() const override;
        uint32_t getCurrentImageIndex() const override;

        uint32_t getMaxTextureSize() const override;
        uint32_t getMaxAnisotropy() const override;
        bool supportsFormat(Format format) const override;
        bool supportsFeature(const std::string& feature) const override;

        uint64_t getTotalAllocatedMemory() const override;
        uint64_t getPeakMemoryUsage() const override;
        uint64_t getDrawCallCount() const override;
        uint64_t getTriangleCount() const override;
        double getFrameTime() const override;
        double getGPUTime() const override;
        uint32_t getFPS() const override;

        void setDebugName(BufferHandle handle, const char* name) override;
        void setDebugName(TextureHandle handle, const char* name) override;
        void setDebugName(SamplerHandle handle, const char* name) override;
        void setDebugName(PipelineHandle handle, const char* name) override;

        void beginDebugLabel(const char* name, const float color[4]) override;
        void endDebugLabel() override;
        void insertDebugLabel(const char* name, const float color[4]) override;

        void* getNativeDevice() const override;
        void* getNativeCommandQueue() const override;

        // 额外的 Vulkan 特有功能
        Device::Ptr getDevice() const { return mDevice; }
        SwapChain::Ptr getSwapChain() const { return mSwapChain; }
        VkSurfaceKHR getSurface() const { return mSurface; }

    private:
        // 转换函数
        VkFormat convertFormat(Format format) const;
        VkBufferUsageFlags convertBufferUsage(BufferUsage usage) const;
        VmaMemoryUsage convertMemoryType(MemoryType type) const;
        VkImageUsageFlags convertTextureUsage(const TextureDesc& desc) const;
        VkFilter convertSamplerFilter(SamplerFilter filter) const;
        VkSamplerAddressMode convertSamplerAddressMode(SamplerAddressMode mode) const;
        VkCompareOp convertCompareOp(CompareOp op) const;
        VkShaderStageFlagBits convertShaderStage(ShaderStage stage) const;
        VkPrimitiveTopology convertPrimitiveTopology(PrimitiveTopology topology) const;
        VkCullModeFlags convertCullMode(CullMode mode) const;
        VkBlendFactor convertBlendFactor(BlendFactor factor) const;
        VkBlendOp convertBlendOp(BlendOp op) const;

        // 内部辅助函数
        uint64_t generateHandleId();
        uint32_t calculateMipLevels(uint32_t width, uint32_t height) const;
        bool isDepthFormat(Format format) const;
        VkImageAspectFlags getImageAspectFlags(Format format) const;

        // 同步和命令相关
        void flushCommandBuffer();
        void createSyncObjects();
        void cleanupSyncObjects();

        // 资源查找
        BufferData* getBufferData(BufferHandle handle);
        TextureData* getTextureData(TextureHandle handle);
        SamplerData* getSamplerData(SamplerHandle handle);
        ShaderModuleData* getShaderModuleData(ShaderModuleHandle handle);
        PipelineLayoutData* getPipelineLayoutData(PipelineLayoutHandle handle);
        PipelineData* getPipelineData(PipelineHandle handle);

        // 统计
        void updateStatistics(uint64_t drawCalls = 0, uint64_t triangleCount = 0);

    private:
        Config mConfig;
        bool mInitialized = false;

        // 现有组件
        Instance::Ptr mInstance;
        Device::Ptr mDevice;
        SwapChain::Ptr mSwapChain;
        FrameContext::Ptr mFrameContext;

        // 表面
        VkSurfaceKHR mSurface = VK_NULL_HANDLE;

        // 资源存储
        std::unordered_map<uint64_t, BufferData> mBuffers;
        std::unordered_map<uint64_t, TextureData> mTextures;
        std::unordered_map<uint64_t, SamplerData> mSamplers;
        std::unordered_map<uint64_t, ShaderModuleData> mShaderModules;
        std::unordered_map<uint64_t, PipelineLayoutData> mPipelineLayouts;
        std::unordered_map<uint64_t, PipelineData> mPipelines;

        // 命令状态
        CommandState mCommandState;
        bool mIsRecording = false;

        // 句柄生成
        uint64_t mNextHandleId = 1;

        // 统计信息
        struct Statistics {
            uint64_t totalAllocatedMemory = 0;
            uint64_t peakMemoryUsage = 0;
            uint64_t drawCallCount = 0;
            uint64_t triangleCount = 0;
            uint64_t totalFrames = 0;
            double frameTime = 0.0;
            double gpuTime = 0.0;
            uint32_t fps = 0;

            std::chrono::high_resolution_clock::time_point frameStartTime;
            std::deque<double> frameTimes;
        } mStatistics;

        // 调试函数指针
        PFN_vkSetDebugUtilsObjectNameEXT mSetDebugUtilsObjectName = nullptr;
        PFN_vkCmdBeginDebugUtilsLabelEXT mCmdBeginDebugLabel = nullptr;
        PFN_vkCmdEndDebugUtilsLabelEXT mCmdEndDebugLabel = nullptr;
        PFN_vkCmdInsertDebugUtilsLabelEXT mCmdInsertDebugLabel = nullptr;

        // 临时命令池
        VkCommandPool mTransferCommandPool = VK_NULL_HANDLE;
    };
} // namespace StarryEngine

