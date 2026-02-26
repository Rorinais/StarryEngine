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

        RHI::RHIPipelineLayout* getPipelineLayout(RHI::PipelineLayoutHandle handle) {
            return mResourceManager->getPipelineLayout(handle);
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

        std::vector<RHI::FramebufferHandle> createFramebuffers(
            RHI::RenderPassHandle renderpass,
            RHI::TextureHandle depthTexture /* = RHI::TextureHandle::Null() */)
        {
            std::vector<RHI::FramebufferHandle> framebuffers;
            framebuffers.reserve(mSwapChain->getImageCount());

            for (size_t i = 0; i < mSwapChain->getImageCount(); i++) {
                RHI::FramebufferDesc fboDesc;
                fboDesc.renderPass = mResourceManager->getRenderPass(renderpass)->getNativeHandle();
                fboDesc.extent = { mSwapChain->getExtent().width, mSwapChain->getExtent().height };

                // 添加颜色附件（交换链图像视图）
                fboDesc.attachments.push_back((void*)mSwapChain->getImageView(i));

                // 添加深度附件（如果提供了深度纹理）
                if (depthTexture != RHI::TextureHandle::Null()) {
                    RHI::RHITexture* texture = mResourceManager->getTexture(depthTexture);
                    if (texture) {
                        // getDefaultView() 返回的是 VkImageView 存储在 void* 中
                        fboDesc.attachments.push_back(texture->getDefaultView());
                    }
                    else {
                        std::cerr << "[Warning] Depth texture handle is invalid!" << std::endl;
                    }
                }

                fboDesc.layers = 1;
                framebuffers.push_back(mResourceManager->createFramebuffer(
                    fboDesc, "framebuffer_" + std::to_string(i)));
            }
            return framebuffers;
        }

        void destroyFramebuffers(const std::vector<RHI::FramebufferHandle> & fboHandles) {
            for (const auto& fboHandle : fboHandles) {
                mResourceManager->destroy(fboHandle);
            }
        }

        RHI::Format getDefaultDepthFormat() const {
            if (!mDevice) return RHI::Format::Undefined;

            switch (mDevice->findDepthFormat()) {
            case VK_FORMAT_D16_UNORM:          return RHI::Format::D16_UNorm;
            case VK_FORMAT_D32_SFLOAT:         return RHI::Format::D32_Float;
            case VK_FORMAT_D24_UNORM_S8_UINT:  return RHI::Format::D24_UNorm_S8_UInt;
            default: return RHI::Format::Undefined;
            }
        }

        RHI::DescriptorSetLayoutHandle createDescriptorSetLayout(RHI::DescriptorSetLayoutDesc desc) {
            return mResourceManager->createDescriptorSetLayout(desc, desc.debugName);
        }

        RHI::DescriptorPoolHandle createDescriptorPool(RHI::DescriptorPoolDesc desc) {
            return mResourceManager->createDescriptorPool(desc, desc.debugName);
        }

        RHI::DescriptorSetHandle allocateDescriptorSet(RHI::DescriptorSetDesc desc) {
            return mResourceManager->createDescriptorSet(desc);
        }

        void updateDescriptorSet(RHI::DescriptorSetHandle setHandle, uint32_t binding, uint32_t arrayElement,
            const RHI::DescriptorBufferInfo& bufferInfo) {
            auto* set = mResourceManager->getDescriptorSet(setHandle);
            if (!set) {
                std::cerr << "[VulkanRHI] Invalid descriptor set handle" << std::endl;
                return;
            }

            auto* buffer = mResourceManager->getBuffer(bufferInfo.buffer);
            if (!buffer) {
                std::cerr << "[VulkanRHI] Invalid buffer handle" << std::endl;
                return;
            }

            set->writeBuffer(binding, arrayElement, buffer, bufferInfo.offset, bufferInfo.range);
            set->update();
        }

        void updateDescriptorSet(RHI::DescriptorSetHandle set, uint32_t binding, uint32_t arrayElement, const RHI::DescriptorImageInfo& imageInfo) {
            auto* rhiSet = mResourceManager->getDescriptorSet(set);
            if (rhiSet) {
                auto* texture = mResourceManager->getTexture(imageInfo.texture);
                auto* sampler = mResourceManager->getSampler(imageInfo.sampler);
                if (texture && sampler) {
                    rhiSet->writeTexture(binding, arrayElement, texture, sampler, imageInfo.imageLayout);
                    rhiSet->update();
                }
            }
        }

        RHI::TextureHandle createTexture(const RHI::TextureDesc& desc) {
            return mResourceManager->createTexture(desc, desc.debugName);
        }

        RHI::RHITexture* getTexture(RHI::TextureHandle handle) {
            return mResourceManager->getTexture(handle);
        }

        RHI::SamplerHandle createSampler(const RHI::SamplerDesc& desc) {
            return mResourceManager->createSampler(desc, desc.debugName);
        }

        RHI::RHISampler* getSampler(RHI::SamplerHandle handle) {
            return mResourceManager->getSampler(handle);
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
