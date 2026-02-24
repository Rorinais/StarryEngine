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

        uint32_t getWidth() const { return mWidth; }
        uint32_t getHeight() const { return mHeight; }

        FrameContext::Ptr getFrameContext() const { return mFrameContext; }

        std::unique_ptr<RHI::RHICommandEncoder> getCommandEncoder(VkCommandBuffer cmdBuf) {
            return std::make_unique<RHI::RHI_VK_CommandEncoder>(mDevice, cmdBuf, mResourceManager.get());
        }

        RHI::PipelineLayoutHandle createPipelineLayout(RHI::PipelineLayoutDesc desc) {
            return mResourceManager->createPipelineLayout(desc, desc.debugName);
        }

        RHI::PipelineHandle createGraphicsPipeline(RHI::GraphicsPipelineDesc desc) {
            return mResourceManager->createGraphicsPipeline(desc, desc.debugName);
        }

        RHI::RenderPassHandle createRenderPass(RHI::RenderPassDesc desc) {
            return mResourceManager->createRenderPass(desc, desc.debugName);
		}

        RHI::ShaderHandle createShaderHandle(RHI::ShaderModuleDesc desc) {
            return mResourceManager->createShader(desc, desc.debugName);
        }
        
        RHI::BufferHandle createBuffer(RHI::BufferDesc desc) {
            return mResourceManager->createBuffer(desc, desc.debugName);
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
			return mResourceManager->createTexture(desc, desc.debugName);
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

				framebuffers.push_back(mResourceManager->createFramebuffer(fboDesc,"framebuffer_"+std::to_string(i)));
            }
			return framebuffers;
        }

        void destroyFramebuffers(const std::vector<RHI::FramebufferHandle> & fboHandles) {
            for (const auto& fboHandle : fboHandles) {
                mResourceManager->destroy(fboHandle);
            }
        }

    private:
        Instance::Ptr mInstance;
        Device::Ptr mDevice;
        SwapChain::Ptr mSwapChain;
        FrameContext::Ptr mFrameContext;
        VkSurfaceKHR mSurface = VK_NULL_HANDLE;

        std::shared_ptr<RHI::ResourceManager> mResourceManager;

        std::function<VkResult(VkSemaphore, VkFence, uint32_t&)> mAcquireFunc;
        std::function<VkResult(VkQueue, uint32_t, VkSemaphore)> mPresentFunc;
        uint32_t mWidth = 0, mHeight = 0;
        bool mFramebufferResized = false; 
    };
}
