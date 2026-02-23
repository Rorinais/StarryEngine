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

        StarryEngine::RHI::RenderPassHandle createRenderPass(StarryEngine::RHI::RenderPassDesc desc) {
            return mResourceManager->createRenderPass(desc);
		}

        StarryEngine::RHI::ShaderHandle createShaderHandle(StarryEngine::RHI::ShaderModuleDesc desc) {
            return mResourceManager->createShader(desc);
        }
        
        StarryEngine::RHI::BufferHandle createBuffer(StarryEngine::RHI::BufferDesc desc) {
            return mResourceManager->createBuffer(desc);
		}

        StarryEngine::RHI::CommandPoolHandle createCommandPool(StarryEngine::RHI::CommandPoolDesc desc) {
            return mResourceManager->createCommandPool(desc);
        }

        StarryEngine::RHI::CommandBufferHandle createCommandBuffer(StarryEngine::RHI::CommandBufferDesc desc) {
            return mResourceManager->createCommandBuffer(desc);
		}

        void release(RHI::ShaderHandle handle) {
            mResourceManager->destroy(handle);
        }

        void release(RHI::PipelineLayoutHandle handle) {
            mResourceManager->destroy(handle);
		}

        RHI::RHIBuffer * getBuffer(RHI::BufferHandle handle) {
            return mResourceManager->getBuffer(handle);
		}

        RHI::RHIRenderPass* getRenderPass(RHI::RenderPassHandle handle) {
            return mResourceManager->getRenderPass(handle);
        }

        RHI::RHIFramebuffer* getFramebuffer(RHI::FramebufferHandle handle) {
            return mResourceManager->getFramebuffer(handle);
		}   

        RHI::RHIPipeline * getPipeline(RHI::PipelineHandle handle) {
            return mResourceManager->getPipeline(handle);
		}

        RHI::TextureHandle createDepthTexture(RHI::TextureDesc desc) {
			return mResourceManager->createTexture(desc);
		}

        std::vector<RHI::FramebufferHandle> createFramebuffers(RHI::RenderPassHandle renderpass,RHI::TextureHandle depthTexture=RHI::TextureHandle::Null()) {
			std::vector<RHI::FramebufferHandle> framebuffers;
			framebuffers.reserve(mSwapChain->getImageCount());

            for (size_t i = 0; i < mSwapChain->getImageCount(); i++){
				RHI::FramebufferDesc fboDesc;
				fboDesc.renderPass = mResourceManager->getRenderPass(renderpass)->getNativeHandle();
				fboDesc.extent = { mSwapChain->getExtent().width, mSwapChain->getExtent().height };
                fboDesc.attachments.push_back((void*)mSwapChain->getImageView(i));
				//TODO: 深度纹理附件未实现，后续添加
                //if (depthTexture != RHI::TextureHandle::Null()) { 
                //    auto image = static_cast<TraditionalImageFull*>(mResourceManager->getTexture(depthTexture)->getNativeHandle());
                //    fboDesc.attachments.push_back(image->view); 
                //}
                fboDesc.layers = 1;
				framebuffers.push_back(mResourceManager->createFramebuffer(fboDesc));
            }
			return framebuffers;
        }

        void destroyFramebuffers(const std::vector<RHI::FramebufferHandle> & fboHandles) {
            for (const auto& fboHandle : fboHandles) {
                mResourceManager->destroy(fboHandle);
            }
        }

        void waitIdle() {
			mDevice->waitIdle();
		}

		VkQueue getGraphicsQueue() const { return mDevice->getGraphicsQueue(); }

        void clear();

		SwapChain::Ptr getSwapChain() const { return mSwapChain; }
		FrameContext::Ptr getFrameContext() const { return mFrameContext; }

        RHI::RHICommandEncoder* getCommandEncoder(RHI::CommandBufferHandle handle) {
			auto commandBuffer = static_cast<RHI::RHI_VK_CommandBuffer*>(mResourceManager->getCommandBuffer(handle));

            return new RHI::RHI_VK_CommandEncoder(mDevice,commandBuffer,mResourceManager.get());
        }

        RHI::RHICommandEncoder* getCommandEncoder(VkCommandBuffer cmdBuf) {
            return new RHI::RHI_VK_CommandEncoder(mDevice, cmdBuf, mResourceManager.get());
        }

    private:
        Instance::Ptr mInstance;
        Device::Ptr mDevice;
        SwapChain::Ptr mSwapChain;
        FrameContext::Ptr mFrameContext;
        VkSurfaceKHR mSurface = VK_NULL_HANDLE;

        std::shared_ptr<RHI::ResourceManager> mResourceManager;
    };
}
