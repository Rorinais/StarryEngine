#pragma once
#include <renderer/backend/vulkan/Instance.hpp>
#include <renderer/backend/vulkan/QueueHandles.hpp>
#include <renderer/backend/vulkan/Device.hpp>
#include <renderer/backend/vulkan/Swapchain.hpp>
#include <renderer/backend/vulkan/FrameContext.hpp>

#include <renderer/interface/RHI_TYPES.hpp>


namespace StarryEngine{
    class ConfigConverter {
    public:
        static Instance::Config convertInstanceConfig(const RHI::RHIInitConfig& rhiConfig);
        static Device::Config convertDeviceConfig(const RHI::RHIInitConfig& rhiConfig);
        static SwapChainConfig convertSwapChainConfig(const RHI::RHIInitConfig& rhiConfig);
        static FrameContext::Config convertFrameContextConfig(const RHI::RHIInitConfig& rhiConfig);

        static uint32_t convertFeatureLevel(RHI::FeatureLevel level);
        static RHI::MessageSeverity convertToRHISeverity(VkDebugUtilsMessageSeverityFlagBitsEXT vulkanSeverity);
        static RHI::MessageSource convertToRHISource(VkDebugUtilsMessageTypeFlagsEXT vulkanType);
        static const char* messageSourceToString(RHI::MessageSource source);
        static const char* messageSeverityToString(RHI::MessageSeverity severity);
    };

    class VulkanRHI: public RHI::IRHI {
    public:
        VulkanRHI() = default;
        ~VulkanRHI(){ clear(); }
        void clear();

        bool initialize(const RHI::RHIInitConfig& config)override;

        bool renderFrame(const std::function<void(RHI::RHICommandEncoder*, uint32_t imageIndex)>& drawFunc)override;

        bool recreateSwapChain(uint32_t width, uint32_t height)override;

        void setupFramebuffers(RHI::RenderPassHandle renderPass);

        void updateDescriptorSet(RHI::DescriptorSetHandle setHandle, uint32_t binding, uint32_t arrayElement, const RHI::DescriptorBufferInfo& bufferInfo);

        void updateDescriptorSet(RHI::DescriptorSetHandle set, uint32_t binding, uint32_t arrayElement, const RHI::DescriptorImageInfo& imageInfo);

        RHI::Format getDepthFormat() const override;

        uint32_t getWidth() const override { return mWidth; }
        uint32_t getHeight() const override { return mHeight; }

        std::shared_ptr<RHI::ResourceManager> getResourceManager() override { return mResourceManager; }

        std::shared_ptr<FrameContext> getFrameContext() override { return mFrameContext; }
  
        RHI::TextureHandle getDepthTexture() const override { return mDepthTextureHandle; }

        const std::vector<RHI::FramebufferHandle>& getFramebuffers() const override { return mFramebuffers; }

        std::unique_ptr<RHI::RHICommandEncoder> getCommandEncoder(VkCommandBuffer cmdBuf)const override;

        void waitIdle() override {mDevice->waitIdle(); }

        void printDeviceInfo() override {
            mDevice->printDeviceInfo();
            mDevice->printQueueInfo();
            mSwapChain->printInfo();
        }

        void printResourceStatistics() override {
            mResourceManager->dumpStatistics();
        }

        void* getInstance() const override { return mDevice->getInstance()->getHandle(); }
        void* getPhysicalDevice() const override { return mDevice->getPhysicalDevice(); }
        void* getDevice()const override { return mDevice->getLogicalDevice(); }
        uint32_t getGraphicsQueueFamilyIndex()const override { return mDevice->getGraphicsQueueFamilyIndex(); }
        void* getGraphicsQueue()const override { return mDevice->getGraphicsQueue(); }
        RHI::Format getSwapChainImageFormat() const override { return RHI::FUNC::VK_TO_RHI_Format(mSwapChain->getFormat()); }
        uint32_t getSwapChainImageCount() const override { return mSwapChain->getImageCount(); }
        void* getSwapChainImageView(uint32_t index) const override { return mSwapChain->getImageView(index); }


    private:
        bool createDepthTexture();
        bool createFramebuffers();  
        void destroyFramebufferResources();

    private:
        Instance::Ptr mInstance;
        Device::Ptr mDevice;
        SwapChain::Ptr mSwapChain;
        std::shared_ptr<FrameContext> mFrameContext;
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
