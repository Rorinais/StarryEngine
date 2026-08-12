#pragma once
#include <renderer/backend/vulkan/VulkanRHI.hpp>      // 包含 ConfigConverter 和 VulkanRHI
#include <core/Window.hpp>
#include <logging/Logger.hpp>     // 包含日志宏

namespace StarryEngine {

    class RHIFactory {
    public:
        virtual ~RHIFactory() = default;
        virtual std::shared_ptr<RHI::IRHI> create(const RHI::RHIInitConfig& config) = 0;
    };

    class VulkanRHIFactory : public RHIFactory {
    public:
        std::shared_ptr<RHI::IRHI> create(const RHI::RHIInitConfig& config) override {
            auto rhi = std::make_shared<VulkanRHI>();
            return rhi->initialize(config) ? rhi : nullptr;
        }

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
            rhiConfig.enableTimestamps = false;  // 首帧卡死 bug（见会话汇报）；修复后改回 true
#ifdef NDEBUG
            rhiConfig.enableDebug = false;
#else
            rhiConfig.enableDebug = true;
#endif

            // 将 Vulkan 验证层消息通过日志系统输出
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

            VulkanRHIFactory factory;
            switch (api) {
            case RHI::API::Vulkan:
                return factory.create(rhiConfig);
            case RHI::API::DirectX11:
            case RHI::API::DirectX12:
            case RHI::API::OpenGL:
            case RHI::API::OpenGLES:
            case RHI::API::Metal:
                LOG_ERROR("[RHIFactory] Unsupported backend: {}", static_cast<int>(api));
                return nullptr;
            default:
                LOG_ERROR("[RHIFactory] Unknown API type");
                return nullptr;
            }
        }
    };

} // namespace StarryEngine