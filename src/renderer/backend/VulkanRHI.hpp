#pragma once
#include "Instance.hpp"
#include "QueueHandles.hpp"
#include "Device.hpp"
#include "Swapchain.hpp"
#include "FrameContext.hpp"

#include "../interface/RHI_TYPES.hpp"
#include "../interface/RHI_RESOURCE_MANAGER.hpp"


namespace StarryEngine{

    class ConfigConverter {
    public:
        static StarryEngine::Instance::Config convertInstanceConfig(const StarryEngine::RHI::RHIInitConfig& rhiConfig);
        static StarryEngine::Device::Config convertDeviceConfig(const StarryEngine::RHI::RHIInitConfig& rhiConfig);
        static StarryEngine::SwapChainConfig convertSwapChainConfig(const StarryEngine::RHI::RHIInitConfig& rhiConfig);
        static StarryEngine::FrameContext::Config convertFrameContextConfig(const StarryEngine::RHI::RHIInitConfig& rhiConfig);

    private:
        static uint32_t convertFeatureLevel(StarryEngine::RHI::FeatureLevel level);
        static StarryEngine::RHI::MessageSeverity convertToRHISeverity(VkDebugUtilsMessageSeverityFlagBitsEXT vulkanSeverity);
        static StarryEngine::RHI::MessageSource convertToRHISource(VkDebugUtilsMessageTypeFlagsEXT vulkanType);
        static const char* messageSourceToString(StarryEngine::RHI::MessageSource source);
        static const char* messageSeverityToString(StarryEngine::RHI::MessageSeverity severity);
    };

    class VulkanRHI{
    public:
        VulkanRHI() = default;
        ~VulkanRHI(){ clear(); }

        bool initialize(const StarryEngine::RHI::RHIInitConfig& config);

        StarryEngine::RHI::PipelineLayoutHandle createPipelineLayout(StarryEngine::RHI::PipelineLayoutDesc desc) {
            return mResourceManager->createPipelineLayout(desc);
        }

        StarryEngine::RHI::PipelineHandle createGraphicsPipeline(StarryEngine::RHI::GraphicsPipelineDesc desc) {
            return mResourceManager->createGraphicsPipeline(desc);
        }

        StarryEngine::RHI::ShaderHandle createShaderHandle(StarryEngine::RHI::ShaderModuleDesc desc) {
            return mResourceManager->createShader(desc);
        }
        
        StarryEngine::RHI::BufferHandle createBuffer(StarryEngine::RHI::BufferDesc desc) {
            return mResourceManager->createBuffer(desc);
		}

        void release(RHI::ShaderHandle handle) {
            mResourceManager->destroy(handle);
        }

        RHI::RHIBuffer * getBuffer(RHI::BufferHandle handle) {
            return mResourceManager->getBuffer(handle);
		}

        void clear();

    private:
        Instance::Ptr mInstance;
        Device::Ptr mDevice;
        SwapChain::Ptr mSwapChain;
        FrameContext::Ptr mFrameContext;
        VkSurfaceKHR mSurface = VK_NULL_HANDLE;

        std::shared_ptr<RHI::ResourceManager> mResourceManager;
    };
}
