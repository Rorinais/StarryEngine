#pragma once
#include <interface/RHIFactory.hpp>
#include <interface/vulkan/VulkanDevice.hpp>
#include <interface/vulkan/VulkanRHI.hpp>
#include <core/Window.hpp>
#include <logging/Logger.hpp>

    
namespace StarryEngine::RHI {

    class VKResourceFactory : public IResourceFactory {
    public:
        VKResourceFactory(VulkanDevice::Ptr device) : mDevice(device) {}

        ~VKResourceFactory() override = default;

        std::unique_ptr<RHIBuffer> createBuffer(const BufferDesc& desc) override;
        std::unique_ptr<RHITexture> createTexture(const TextureDesc& desc) override;
        std::unique_ptr<RHIShaderModule> createShader(const ShaderModuleDesc& desc) override;
        std::unique_ptr<RHISampler> createSampler(const SamplerDesc& desc) override;
        std::unique_ptr<RHIRenderPass> createRenderPass(const RenderPassDesc& desc) override;
        std::unique_ptr<RHIFramebuffer> createFramebuffer(
            RHIRenderPass* renderPass,
            const std::vector<RHITexture*>& attachments,
            const FramebufferDesc& desc) override;
        std::unique_ptr<RHIDescriptorPool> createDescriptorPool(const DescriptorPoolDesc& desc) override;

        std::unique_ptr<RHIDescriptorSetLayout> createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) override;

        std::unique_ptr<RHIPipeline> createComputePipeline(
            const ComputePipelineDesc& desc,
            RHIShaderModule* computeShader,
            RHIPipelineLayout* layout) override;

        std::unique_ptr<RHIPipeline> createGraphicPipeline(
            const GraphicsPipelineDesc& desc,
            RHIPipelineLayout* layout,
            RHIShaderModule* vertexShader,
            RHIShaderModule* fragmentShader,
            RHIRenderPass* renderPass) override;

        std::unique_ptr<RHIPipelineLayout> createPipelineLayout(
            const PipelineLayoutDesc& desc,
            const std::vector<RHIDescriptorSetLayout*>& descriptorSetLayouts) override;

        std::unique_ptr<RHIDescriptorSet> createDescriptorSet(
            const DescriptorSetDesc& desc,
            RHIDescriptorPool* pool,
            RHIDescriptorSetLayout* layout) override;

        std::vector<std::unique_ptr<RHIDescriptorSet>> createDescriptorSets(
            uint32_t count, const DescriptorSetDesc& desc,
            RHIDescriptorPool* pool,
            RHIDescriptorSetLayout* layout) override;
    private:
        VulkanDevice::Ptr mDevice;
    };
}

namespace StarryEngine {

    /// 与旧 renderer/backend/RHIFactory.hpp 等价的入口：构建 RHIInitConfig 并创建 VulkanRHI。
    class VulkanRHIFactory {
    public:
        static std::shared_ptr<RHI::IRHI> createDefault(RHI::API api, Window::Ptr window,
            uint32_t width, uint32_t height,
            uint32_t flightFrame = 2,
            bool transparent = false) {
            RHI::RHIInitConfig rhiConfig;
            rhiConfig.windowHandle = window->getHandle();
            rhiConfig.windowWidth = width;
            rhiConfig.windowHeight = height;
            rhiConfig.requestTransparentSwapchain = transparent;
            rhiConfig.appName = "StarryEngine Application";
            rhiConfig.appVersion = { 1, 0, 0 };
            rhiConfig.engineName = "StarryEngine";
            rhiConfig.engineVersion = { 1, 0, 0 };
            rhiConfig.deviceFeatures.samplerAnisotropy = true;
            rhiConfig.deviceFeatures.textureCompression = true;
            rhiConfig.deviceFeatures.synchronization = true;
            rhiConfig.deviceFeatures.dynamicRendering = true;
            rhiConfig.presentMode = RHI::RHIInitConfig::PresentMode::FIFO;
            rhiConfig.swapChainImages = flightFrame;
            rhiConfig.srgb = true;
            rhiConfig.frameBuffering = flightFrame;
            rhiConfig.usePersistentCommandBuffers = true;
            rhiConfig.enableTimestamps = true;
#ifdef NDEBUG
            rhiConfig.enableDebug = false;
#else
            rhiConfig.enableDebug = true;
#endif
            rhiConfig.debugCallback = [](RHI::MessageSeverity severity,
                RHI::MessageSource source,
                const std::string& message) {
                    const char* sourceStr = ConfigConverter::messageSourceToString(source);
                    switch (severity) {
                    case RHI::MessageSeverity::Verbose:
                        LOG_TRACE("[{}] {}", sourceStr, message);
                        break;
                    case RHI::MessageSeverity::Info:
                        LOG_INFO("[{}] {}", sourceStr, message);
                        break;
                    case RHI::MessageSeverity::Warning:
                        LOG_WARN("[{}] {}", sourceStr, message);
                        break;
                    case RHI::MessageSeverity::Error:
                        LOG_ERROR("[{}] {}", sourceStr, message);
                        break;
                    case RHI::MessageSeverity::Critical:
                        LOG_CRITICAL("[{}] {}", sourceStr, message);
                        break;
                    default:
                        LOG_INFO("[{}] {}", sourceStr, message);
                        break;
                    }
            };

            if (api != RHI::API::Vulkan) {
                LOG_ERROR("[RHIFactory] Unsupported backend: {}", static_cast<int>(api));
                return nullptr;
            }
            auto rhi = std::make_shared<VulkanRHI>();
            return rhi->initialize(rhiConfig) ? rhi : nullptr;
        }
    };

} // namespace StarryEngine
