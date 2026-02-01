#include"VulkanRHI.hpp"

namespace StarryEngine {
    StarryEngine::Instance::Config ConfigConverter::convertInstanceConfig(const StarryEngine::RHI::RHIInitConfig& rhiConfig) {
        StarryEngine::Instance::Config config;

        config.appName = rhiConfig.appName;
        config.engineName = rhiConfig.engineName;
        config.appVersion = rhiConfig.appVersion.packed();
        config.engineVersion = rhiConfig.engineVersion.packed();
        config.apiVersion = convertFeatureLevel(rhiConfig.featureLevel);
        config.enableValidation = rhiConfig.enableDebug;

        for (const auto& ext : rhiConfig.requiredExtensions) {
            config.requiredExtensions.push_back(ext.c_str());
        }

        for (const auto& layer : rhiConfig.requiredLayers) {
            config.validationLayers.push_back(layer.c_str());
        }

        // 设置调试过滤器
        config.debugSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        config.debugType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        // 转换调试回调
        if (rhiConfig.debugCallback) {
            config.debugCallback = [rhiConfig](VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                VkDebugUtilsMessageTypeFlagsEXT type,
                const VkDebugUtilsMessengerCallbackDataEXT* data) {
                    // 转换严重程度
                    StarryEngine::RHI::MessageSeverity rhiSeverity = convertToRHISeverity(severity);

                    // 转换消息来源
                    StarryEngine::RHI::MessageSource rhiSource = convertToRHISource(type);

                    // 构建消息字符串
                    std::string message = data->pMessage;
                    if (data->pMessageIdName) {
                        message = std::string("[") + data->pMessageIdName + "] " + message;
                    }

                    // 调用RHI回调
                    rhiConfig.debugCallback(rhiSeverity, rhiSource, message);
                };
        }

        return config;
    }

    StarryEngine::Device::Config ConfigConverter::convertDeviceConfig(const StarryEngine::RHI::RHIInitConfig& rhiConfig) {
        StarryEngine::Device::Config config;

        for (const auto& ext : rhiConfig.deviceExtensions) {
            config.extensions.push_back(ext.c_str());
        }

        config.samplerAnisotropy = rhiConfig.deviceFeatures.samplerAnisotropy ? VK_TRUE : VK_FALSE;
        config.geometryShader = rhiConfig.deviceFeatures.geometryShader ? VK_TRUE : VK_FALSE;
        config.tessellationShader = rhiConfig.deviceFeatures.tessellationShader ? VK_TRUE : VK_FALSE;
        config.fillModeNonSolid = rhiConfig.deviceFeatures.fillModeNonSolid ? VK_TRUE : VK_FALSE;
        config.wideLines = rhiConfig.deviceFeatures.wideLines ? VK_TRUE : VK_FALSE;

        config.queuePriority = rhiConfig.queuePriority;
        config.enableValidation = rhiConfig.enableDebug;
        config.enableVMA = rhiConfig.enableVMA;

        return config;
    }

    StarryEngine::SwapChainConfig ConfigConverter::convertSwapChainConfig(const StarryEngine::RHI::RHIInitConfig& rhiConfig) {
        StarryEngine::SwapChainConfig config;

        config.width = rhiConfig.windowWidth;
        config.height = rhiConfig.windowHeight;

        // 转换PresentMode
        switch (rhiConfig.presentMode) {
        case StarryEngine::RHI::RHIInitConfig::PresentMode::FIFO:
            config.presentMode = VK_PRESENT_MODE_FIFO_KHR;
            break;
        case StarryEngine::RHI::RHIInitConfig::PresentMode::MAILBOX:
            config.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        case StarryEngine::RHI::RHIInitConfig::PresentMode::IMMEDIATE:
            config.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            break;
        default:
            config.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        }

        config.surfaceFormat = {
            rhiConfig.srgb ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM,
            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        };

        config.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        config.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        config.clipped = VK_TRUE;

        config.enableMailboxMode = rhiConfig.enableMailboxMode;
        config.enableImmediateMode = rhiConfig.enableImmediateMode;
        config.minImageCount = rhiConfig.swapChainImages;

        return config;
    }

    StarryEngine::FrameContext::Config ConfigConverter::convertFrameContextConfig(const StarryEngine::RHI::RHIInitConfig& rhiConfig) {
        StarryEngine::FrameContext::Config config;

        config.frameCount = rhiConfig.frameBuffering;
        config.usePersistentCommandBuffers = rhiConfig.usePersistentCommandBuffers;
        config.enableTimestamps = rhiConfig.enableTimestamps;

        if (rhiConfig.allowCommandBufferReset) {
            config.commandBufferResetFlags |= VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT;
        }

        if (rhiConfig.allowCommandPoolReset) {
            config.commandPoolFlags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        }

        config.maxRecreateAttempts = rhiConfig.maxRecreateAttempts;
        config.autoRecreate = rhiConfig.autoRecreateSwapChain;

        return config;
    }

uint32_t ConfigConverter::convertFeatureLevel(StarryEngine::RHI::FeatureLevel level) {
    switch (level) {
    case StarryEngine::RHI::FeatureLevel::VK_1_0: 
        return VK_MAKE_API_VERSION(0, 1, 0, 0);
    case StarryEngine::RHI::FeatureLevel::VK_1_1: 
        return VK_MAKE_API_VERSION(0, 1, 1, 0);
    case StarryEngine::RHI::FeatureLevel::VK_1_2: 
        return VK_MAKE_API_VERSION(0, 1, 2, 0);
    case StarryEngine::RHI::FeatureLevel::VK_1_3: 
        return VK_MAKE_API_VERSION(0, 1, 3, 0);
    default: 
        return VK_MAKE_API_VERSION(0, 1, 3, 0); 
    }
}

    StarryEngine::RHI::MessageSeverity ConfigConverter::convertToRHISeverity(VkDebugUtilsMessageSeverityFlagBitsEXT vulkanSeverity) {
        switch (vulkanSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            return StarryEngine::RHI::MessageSeverity::Verbose;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            return StarryEngine::RHI::MessageSeverity::Info;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            return StarryEngine::RHI::MessageSeverity::Warning;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            return StarryEngine::RHI::MessageSeverity::Error;
        default:
            return StarryEngine::RHI::MessageSeverity::Critical;
        }
    }

    StarryEngine::RHI::MessageSource ConfigConverter::convertToRHISource(VkDebugUtilsMessageTypeFlagsEXT vulkanType) {
        if (vulkanType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
            return StarryEngine::RHI::MessageSource::Validation;
        }
        if (vulkanType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
            return StarryEngine::RHI::MessageSource::Performance;
        }
        if (vulkanType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
            return StarryEngine::RHI::MessageSource::General;
        }

        return StarryEngine::RHI::MessageSource::API;
    }

    const char* ConfigConverter::messageSourceToString(StarryEngine::RHI::MessageSource source) {
        switch (source) {
        case StarryEngine::RHI::MessageSource::General: return "General";
        case StarryEngine::RHI::MessageSource::Validation: return "Validation";
        case StarryEngine::RHI::MessageSource::Performance: return "Performance";
        case StarryEngine::RHI::MessageSource::Shader: return "Shader";
        case StarryEngine::RHI::MessageSource::API: return "API";
        default: return "Unknown";
        }
    }

    const char* ConfigConverter::messageSeverityToString(StarryEngine::RHI::MessageSeverity severity) {
        switch (severity) {
        case StarryEngine::RHI::MessageSeverity::Verbose: return "Verbose";
        case StarryEngine::RHI::MessageSeverity::Info: return "Info";
        case StarryEngine::RHI::MessageSeverity::Warning: return "Warning";
        case StarryEngine::RHI::MessageSeverity::Error: return "Error";
        case StarryEngine::RHI::MessageSeverity::Critical: return "Critical";
        default: return "Unknown";
        }
    }

    bool VulkanRHI::initialize(const StarryEngine::RHI::RHIInitConfig& config) {
        // 1. 检查窗口句柄
        if (!config.windowHandle) {
            if (config.debugCallback) {
                config.debugCallback(
                    StarryEngine::RHI::MessageSeverity::Error,
                    StarryEngine::RHI::MessageSource::API,
                    "VulkanStarryEngine::RHI::initialize: windowHandle is null!"
                );
            }
            return false;
        }

        // 2. 转换配置
        auto instanceConfig = ConfigConverter::convertInstanceConfig(config);
        auto deviceConfig = ConfigConverter::convertDeviceConfig(config);
        auto swapChainConfig = ConfigConverter::convertSwapChainConfig(config);
        auto frameContextConfig = ConfigConverter::convertFrameContextConfig(config);

        // 3. 创建Vulkan实例
        try {
            mInstance = Instance::create(instanceConfig);
            std::cout << "[INFO] Vulkan instance created successfully" << std::endl;
        }
        catch (const std::exception& e) {
            if (config.debugCallback) {
                config.debugCallback(
                    StarryEngine::RHI::MessageSeverity::Error,
                    StarryEngine::RHI::MessageSource::API,
                    std::string("Failed to create Vulkan instance: ") + e.what()
                );
            }
            return false;
        }

        if (!mInstance || !mInstance->getHandle()) {
            return false;
        }

        // 4. 创建Surface（使用GLFW）
        GLFWwindow* window = static_cast<GLFWwindow*>(config.windowHandle);

        // 检查窗口是否有效
        if (!glfwGetWindowAttrib(window, GLFW_VISIBLE)) {
            std::cerr << "[WARNING] Window is not visible when creating surface" << std::endl;
        }

        std::cout << "[INFO] Creating window surface..." << std::endl;
        VkResult result = glfwCreateWindowSurface(
            mInstance->getHandle(),
            window,
            nullptr,
            &mSurface
        );

        if (result != VK_SUCCESS) {
            std::string errorMsg = "Failed to create window surface! Error: ";
            switch (result) {
            case VK_ERROR_OUT_OF_HOST_MEMORY: errorMsg += "OUT_OF_HOST_MEMORY"; break;
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: errorMsg += "OUT_OF_DEVICE_MEMORY"; break;
            case VK_ERROR_EXTENSION_NOT_PRESENT: errorMsg += "EXTENSION_NOT_PRESENT"; break;
            default: errorMsg += std::to_string(result); break;
            }

            if (config.debugCallback) {
                config.debugCallback(
                    StarryEngine::RHI::MessageSeverity::Error,
                    StarryEngine::RHI::MessageSource::API,
                    errorMsg
                );
            }
            return false;
        }

        std::cout << "[INFO] Window surface created successfully" << std::endl;

        // 5. 创建设备
        try {
            std::cout << "[INFO] Creating Vulkan device..." << std::endl;
            mDevice = Device::create(mInstance, mSurface, deviceConfig);
            std::cout << "[INFO] Device created successfully" << std::endl;
        }
        catch (const std::exception& e) {
            std::string errorMsg = std::string("Failed to create device: ") + e.what();
            std::cerr << "[ERROR] " << errorMsg << std::endl;

            if (config.debugCallback) {
                config.debugCallback(
                    StarryEngine::RHI::MessageSeverity::Error,
                    StarryEngine::RHI::MessageSource::API,
                    errorMsg
                );
            }

            // 清理表面
            if (mSurface != VK_NULL_HANDLE) {
                vkDestroySurfaceKHR(mInstance->getHandle(), mSurface, nullptr);
                mSurface = VK_NULL_HANDLE;
            }

            return false;
        }

        if (!mDevice || !mDevice->getLogicalDevice()) {
            std::cerr << "[ERROR] Device creation failed - null device" << std::endl;

            // 清理表面
            if (mSurface != VK_NULL_HANDLE) {
                vkDestroySurfaceKHR(mInstance->getHandle(), mSurface, nullptr);
                mSurface = VK_NULL_HANDLE;
            }

            return false;
        }

        // 6. 延迟初始化 VMA（在设备完全创建后）
        try {
            if (deviceConfig.enableVMA) {
                std::cout << "[INFO] Initializing VMA allocator..." << std::endl;
                if (!mDevice->initializeVMA()) {
                    std::cerr << "[WARNING] VMA initialization failed, but continuing..." << std::endl;
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[WARNING] VMA initialization error: " << e.what() << std::endl;
            // 继续执行，VMA 不是必需的
        }

        // 7. 创建交换链
        try {
            std::cout << "[INFO] Creating swap chain..." << std::endl;
            mSwapChain = SwapChain::create(mDevice, mSurface, swapChainConfig);

            if (!mSwapChain || !mSwapChain->isValid()) {
                throw std::runtime_error("Swap chain creation returned invalid object");
            }

            std::cout << "[INFO] Swap chain created with " << mSwapChain->getImageCount()
                << " images" << std::endl;
        }
        catch (const std::exception& e) {
            std::string errorMsg = std::string("Failed to create swap chain: ") + e.what();
            std::cerr << "[ERROR] " << errorMsg << std::endl;

            if (config.debugCallback) {
                config.debugCallback(
                    StarryEngine::RHI::MessageSeverity::Error,
                    StarryEngine::RHI::MessageSource::API,
                    errorMsg
                );
            }

            clear();
            return false;
        }

        // 8. 创建帧上下文
        try {
            std::cout << "[INFO] Creating frame context..." << std::endl;
            mFrameContext = FrameContext::create(mDevice, frameContextConfig);

            // 获取图形队列族索引
            auto queueFamilyIndices = mDevice->getQueueFamilyIndices();
            if (!queueFamilyIndices.graphicsFamily.has_value()) {
                throw std::runtime_error("No graphics queue family found!");
            }

            // 初始化帧上下文
            if (!mFrameContext->initialize(queueFamilyIndices.graphicsFamily.value())) {
                throw std::runtime_error("Failed to initialize frame context!");
            }

            std::cout << "[INFO] Frame context created successfully" << std::endl;
        }
        catch (const std::exception& e) {
            std::string errorMsg = std::string("Failed to create frame context: ") + e.what();
            std::cerr << "[ERROR] " << errorMsg << std::endl;

            if (config.debugCallback) {
                config.debugCallback(
                    StarryEngine::RHI::MessageSeverity::Error,
                    StarryEngine::RHI::MessageSource::API,
                    errorMsg
                );
            }

            clear();
            return false;
        }

        // 9. 输出成功信息
        if (config.debugCallback) {
            std::string info = "Vulkan RHI initialized successfully!\n";
            info += "  Device: " + std::string(mDevice->getDeviceName()) + "\n";
            info += "  SwapChain: " + std::to_string(mSwapChain->getImageCount()) + " images\n";
            info += "  FrameBuffering: " + std::to_string(config.frameBuffering);

            config.debugCallback(
                StarryEngine::RHI::MessageSeverity::Info,
                StarryEngine::RHI::MessageSource::API,
                info
            );
        }

        auto factory = std::make_shared<StarryEngine::RHI::VKResourceFactory>(mDevice);
        mResourceManager = std::make_shared<StarryEngine::RHI::ResourceManager>(factory);
        mResourceManager->setDebugMode(config.enableDebug);

        return true;
    }

    void VulkanRHI::clear() {
        std::cout << "[INFO] Cleaning up Vulkan RHI..." << std::endl;

        // 等待设备空闲
        if (mDevice) {
            try {
                mDevice->waitIdle();
            }
            catch (const std::exception& e) {
                std::cerr << "[WARNING] Failed to wait for device idle: " << e.what() << std::endl;
            }
        }

        // 清理帧上下文
        if (mFrameContext) {
            try {
                mFrameContext->cleanup();
            }
            catch (const std::exception& e) {
                std::cerr << "[WARNING] Failed to cleanup frame context: " << e.what() << std::endl;
            }
            mFrameContext.reset();
        }

        // 清理交换链
        if (mSwapChain) {
            // SwapChain析构函数会自动清理
            mSwapChain.reset();
        }

        // 清理设备
        if (mDevice) {
            mDevice.reset();
        }

        // 清理Surface - 必须在实例销毁之前
        if (mSurface != VK_NULL_HANDLE && mInstance) {
            std::cout << "[INFO] Destroying window surface..." << std::endl;
            vkDestroySurfaceKHR(mInstance->getHandle(), mSurface, nullptr);
            mSurface = VK_NULL_HANDLE;
        }

        // 清理实例
        if (mInstance) {
            mInstance.reset();
        }

        std::cout << "[INFO] Vulkan RHI cleanup completed" << std::endl;
    }

}