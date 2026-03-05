#pragma once
#include "Instance.hpp"
#include "QueueHandles.hpp"
#include "Device.hpp"
#include "Swapchain.hpp"
#include "FrameContext.hpp"

#include "../interface/RHI_TYPES.hpp"
#include "../interface/RHI_RESOURCE_MANAGER.hpp"


namespace StarryEngine{
    template<typename Handle>
    struct HandleTraits;

    // BufferHandle
    template<>
    struct HandleTraits<RHI::BufferHandle> {
        using ResourceType = RHI::RHIBuffer;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::BufferHandle handle) {
            return mgr->getBuffer(handle);
        }
        static RHI::BufferHandle create(RHI::ResourceManager* mgr, const RHI::BufferDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createBuffer(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::BufferHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // TextureHandle
    template<>
    struct HandleTraits<RHI::TextureHandle> {
        using ResourceType = RHI::RHITexture;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::TextureHandle handle) {
            return mgr->getTexture(handle);
        }
        static RHI::TextureHandle create(RHI::ResourceManager* mgr, const RHI::TextureDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createTexture(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::TextureHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // PipelineHandle
    template<>
    struct HandleTraits<RHI::PipelineHandle> {
        using ResourceType = RHI::RHIPipeline;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::PipelineHandle handle) {
            return mgr->getPipeline(handle);
        }
        static RHI::PipelineHandle create(RHI::ResourceManager* mgr, const RHI::GraphicsPipelineDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createGraphicsPipeline(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        // 注意：计算管线也返回 PipelineHandle，但描述符类型不同，可以再增加一个 createCompute 特化，或使用 if constexpr
        // 这里为了简化，只提供图形管线的创建，计算管线单独处理（或通过另一个特化）
        static bool destroy(RHI::ResourceManager* mgr, RHI::PipelineHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // PipelineLayoutHandle
    template<>
    struct HandleTraits<RHI::PipelineLayoutHandle> {
        using ResourceType = RHI::RHIPipelineLayout;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::PipelineLayoutHandle handle) {
            return mgr->getPipelineLayout(handle);
        }
        static RHI::PipelineLayoutHandle create(RHI::ResourceManager* mgr, const RHI::PipelineLayoutDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createPipelineLayout(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::PipelineLayoutHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // ShaderHandle
    template<>
    struct HandleTraits<RHI::ShaderHandle> {
        using ResourceType = RHI::RHIShaderModule;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::ShaderHandle handle) {
            return mgr->getShader(handle);
        }
        static RHI::ShaderHandle create(RHI::ResourceManager* mgr, const RHI::ShaderModuleDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createShader(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::ShaderHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // SamplerHandle
    template<>
    struct HandleTraits<RHI::SamplerHandle> {
        using ResourceType = RHI::RHISampler;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::SamplerHandle handle) {
            return mgr->getSampler(handle);
        }
        static RHI::SamplerHandle create(RHI::ResourceManager* mgr, const RHI::SamplerDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createSampler(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::SamplerHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // RenderPassHandle
    template<>
    struct HandleTraits<RHI::RenderPassHandle> {
        using ResourceType = RHI::RHIRenderPass;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::RenderPassHandle handle) {
            return mgr->getRenderPass(handle);
        }
        static RHI::RenderPassHandle create(RHI::ResourceManager* mgr, const RHI::RenderPassDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createRenderPass(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::RenderPassHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // FramebufferHandle
    template<>
    struct HandleTraits<RHI::FramebufferHandle> {
        using ResourceType = RHI::RHIFramebuffer;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::FramebufferHandle handle) {
            return mgr->getFramebuffer(handle);
        }
        static RHI::FramebufferHandle create(RHI::ResourceManager* mgr, const RHI::FramebufferDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createFramebuffer(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::FramebufferHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // DescriptorSetHandle
    template<>
    struct HandleTraits<RHI::DescriptorSetHandle> {
        using ResourceType = RHI::RHIDescriptorSet;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::DescriptorSetHandle handle) {
            return mgr->getDescriptorSet(handle);
        }
        static RHI::DescriptorSetHandle create(RHI::ResourceManager* mgr, const RHI::DescriptorSetDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createDescriptorSet(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::DescriptorSetHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // DescriptorPoolHandle
    template<>
    struct HandleTraits<RHI::DescriptorPoolHandle> {
        using ResourceType = RHI::RHIDescriptorPool;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::DescriptorPoolHandle handle) {
            return mgr->getDescriptorPool(handle);
        }
        static RHI::DescriptorPoolHandle create(RHI::ResourceManager* mgr, const RHI::DescriptorPoolDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createDescriptorPool(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::DescriptorPoolHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // DescriptorSetLayoutHandle
    template<>
    struct HandleTraits<RHI::DescriptorSetLayoutHandle> {
        using ResourceType = RHI::RHIDescriptorSetLayout;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::DescriptorSetLayoutHandle handle) {
            return mgr->getDescriptorSetLayout(handle);
        }
        static RHI::DescriptorSetLayoutHandle create(RHI::ResourceManager* mgr, const RHI::DescriptorSetLayoutDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createDescriptorSetLayout(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::DescriptorSetLayoutHandle handle) {
            return mgr->destroy(handle);
        }
    };

    class ConfigConverter {
    public:
        static Instance::Config convertInstanceConfig(const RHI::RHIInitConfig& rhiConfig);
        static Device::Config convertDeviceConfig(const RHI::RHIInitConfig& rhiConfig);
        static SwapChainConfig convertSwapChainConfig(const RHI::RHIInitConfig& rhiConfig);
        static FrameContext::Config convertFrameContextConfig(const RHI::RHIInitConfig& rhiConfig);

    private:
        static uint32_t convertFeatureLevel(RHI::FeatureLevel level);
        static RHI::MessageSeverity convertToRHISeverity(VkDebugUtilsMessageSeverityFlagBitsEXT vulkanSeverity);
        static RHI::MessageSource convertToRHISource(VkDebugUtilsMessageTypeFlagsEXT vulkanType);
        static const char* messageSourceToString(RHI::MessageSource source);
        static const char* messageSeverityToString(RHI::MessageSeverity severity);
    };

    class VulkanRHI{
    public:
        VulkanRHI() = default;
        ~VulkanRHI(){ clear(); }
        void clear();

        bool initialize(const RHI::RHIInitConfig& config);

        bool renderFrame(const std::function<void(RHI::RHICommandEncoder*, uint32_t imageIndex)>& drawFunc);

        bool recreateSwapChain(uint32_t width, uint32_t height);

        void setupFramebuffers(RHI::RenderPassHandle renderPass);

        void updateDescriptorSet(RHI::DescriptorSetHandle setHandle, uint32_t binding, uint32_t arrayElement, const RHI::DescriptorBufferInfo& bufferInfo);

        void updateDescriptorSet(RHI::DescriptorSetHandle set, uint32_t binding, uint32_t arrayElement, const RHI::DescriptorImageInfo& imageInfo);

        RHI::Format getDepthFormat() const;

        uint32_t getWidth() const { return mWidth; }
        uint32_t getHeight() const { return mHeight; }

        std::shared_ptr<RHI::ResourceManager> getResourceManager() { return mResourceManager; }

        FrameContext::Ptr getFrameContext() const { return mFrameContext; }
  
        RHI::TextureHandle getDepthTexture() const { return mDepthTextureHandle; }

        const std::vector<RHI::FramebufferHandle>& getFramebuffers() const { return mFramebuffers; }

        std::unique_ptr<RHI::RHICommandEncoder> getCommandEncoder(VkCommandBuffer cmdBuf);

        template<typename Handle>
        auto getResource(Handle handle) -> typename HandleTraits<Handle>::ResourceType* {
            return HandleTraits<Handle>::get(mResourceManager.get(), handle);
        }

        template<typename Handle>
        bool destroyResource(Handle handle) {
            return HandleTraits<Handle>::destroy(mResourceManager.get(), handle);
        }

        template<typename Handle, typename Desc>
        Handle createResource(const Desc& desc, const std::string& name = "", const std::string& debugTag = "") {
            return HandleTraits<Handle>::create(mResourceManager.get(), desc, name, debugTag);
        }

        uint32_t getSwapChainImageCount() const { return mSwapChain->getImageCount(); }
        void* getSwapChainImageView(uint32_t index) const { return mSwapChain->getImageView(index); }

        void waitIdle() {mDevice->waitIdle(); }

        void printAllDeivceInfo(){
            mDevice->printDeviceInfo();
            mDevice->printQueueInfo();
            mSwapChain->printInfo();
        }

        void printResourceStatistics() {
            mResourceManager->dumpStatistics();
        }
    private:
        bool createDepthTexture();
        bool createFramebuffers();  
        void destroyFramebufferResources();

    private:
        Instance::Ptr mInstance;
        Device::Ptr mDevice;
        SwapChain::Ptr mSwapChain;
        FrameContext::Ptr mFrameContext;
        VkSurfaceKHR mSurface = VK_NULL_HANDLE;

        std::function<VkResult(VkSemaphore, VkFence, uint32_t&)> mAcquireFunc;
        std::function<VkResult(VkQueue, uint32_t, VkSemaphore)> mPresentFunc;
        uint32_t mWidth = 0, mHeight = 0;
        bool mFramebufferResized = false; 

        std::shared_ptr<RHI::ResourceManager> mResourceManager;
    private:
        RHI::RenderPassHandle mRenderPassHandle = RHI::RenderPassHandle::Null();
        RHI::TextureHandle mDepthTextureHandle = RHI::TextureHandle::Null();
        std::vector<RHI::FramebufferHandle> mFramebuffers;
        RHI::Format mDepthFormat = RHI::Format::Undefined;
    };
}
