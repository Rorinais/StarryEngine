#include <interface/vulkan/VulkanRHI.hpp>
#include <interface/vulkan/VulkanInstance.hpp>
#include <interface/vulkan/VulkanDevice.hpp>
#include <interface/vulkan/VulkanSwapchain.hpp>
#include <interface/vulkan/VulkanFrameContext.hpp>
#include <interface/RHIManager.hpp>
#include <interface/IRHIResources.hpp>
#include <interface/vulkan/VulkanFactory.hpp>
#include <interface/vulkan/VulkanResources.hpp>
#include <interface/vulkan/VulkanCommandEncoder.hpp>
#include <interface/vulkan/VulkanConversion.hpp>

#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <string>

namespace StarryEngine {
    VulkanInstance::Config ConfigConverter::convertInstanceConfig(const RHI::RHIInitConfig& rhiConfig) {
        VulkanInstance::Config config;
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

        config.debugSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        config.debugType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        if (rhiConfig.debugCallback) {
            config.debugCallback = [rhiConfig](
                VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                VkDebugUtilsMessageTypeFlagsEXT type, 
                const VkDebugUtilsMessengerCallbackDataEXT* data
                ){
                RHI::MessageSeverity rhiSeverity = convertToRHISeverity(severity);

                RHI::MessageSource rhiSource = convertToRHISource(type);

                std::string message = data->pMessage;
                if (data->pMessageIdName) message = std::string("[") + data->pMessageIdName + "] " + message;

                rhiConfig.debugCallback(rhiSeverity, rhiSource, message);
            };
        }
        return config;
    }

    VulkanDevice::Config ConfigConverter::convertDeviceConfig(const RHI::RHIInitConfig& rhiConfig) {
        VulkanDevice::Config config;
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

    SwapChainConfig ConfigConverter::convertSwapChainConfig(const RHI::RHIInitConfig& rhiConfig) {
        SwapChainConfig config;
        config.width = rhiConfig.windowWidth;
        config.height = rhiConfig.windowHeight;

        switch (rhiConfig.presentMode) {
        case RHI::RHIInitConfig::PresentMode::FIFO:
            config.presentMode = VK_PRESENT_MODE_FIFO_KHR;
            break;
        case RHI::RHIInitConfig::PresentMode::MAILBOX:
            config.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        case RHI::RHIInitConfig::PresentMode::IMMEDIATE:
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
        // 透明窗口请求 alpha 合成；最终是否生效由 VulkanSwapChain 依据表面能力验证（不支持则回退 OPAQUE）
        config.compositeAlpha = rhiConfig.requestTransparentSwapchain
            ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
            : VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        config.clipped = VK_TRUE;

        config.enableMailboxMode = rhiConfig.enableMailboxMode;
        config.enableImmediateMode = rhiConfig.enableImmediateMode;
        config.minImageCount = rhiConfig.swapChainImages;
        return config;
    }

    VulkanFrameContext::Config ConfigConverter::convertFrameContextConfig(const RHI::RHIInitConfig& rhiConfig) {
        VulkanFrameContext::Config config;
        config.frameCount = rhiConfig.frameBuffering;
        config.usePersistentCommandBuffers = rhiConfig.usePersistentCommandBuffers;
        config.enableTimestamps = rhiConfig.enableTimestamps;

        if (rhiConfig.allowCommandBufferReset) config.commandBufferResetFlags |= VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT;
        if (rhiConfig.allowCommandPoolReset) config.commandPoolFlags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        config.maxRecreateAttempts = rhiConfig.maxRecreateAttempts;
        config.autoRecreate = rhiConfig.autoRecreateSwapChain;
        return config;
    }

    uint32_t ConfigConverter::convertFeatureLevel(RHI::FeatureLevel level) {
        switch (level) {
        case RHI::FeatureLevel::VK_1_0: 
            return VK_MAKE_API_VERSION(0, 1, 0, 0);
        case RHI::FeatureLevel::VK_1_1: 
            return VK_MAKE_API_VERSION(0, 1, 1, 0);
        case RHI::FeatureLevel::VK_1_2: 
            return VK_MAKE_API_VERSION(0, 1, 2, 0);
        case RHI::FeatureLevel::VK_1_3: 
            return VK_MAKE_API_VERSION(0, 1, 3, 0);
        default: 
            return VK_MAKE_API_VERSION(0, 1, 3, 0); 
        }
    }

    RHI::MessageSeverity ConfigConverter::convertToRHISeverity(VkDebugUtilsMessageSeverityFlagBitsEXT vulkanSeverity) {
        switch (vulkanSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            return RHI::MessageSeverity::Verbose;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            return RHI::MessageSeverity::Info;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            return RHI::MessageSeverity::Warning;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            return RHI::MessageSeverity::Error;
        default:
            return RHI::MessageSeverity::Critical;
        }
    }

    RHI::MessageSource ConfigConverter::convertToRHISource(VkDebugUtilsMessageTypeFlagsEXT vulkanType) {
        if (vulkanType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) return RHI::MessageSource::Validation;
        if (vulkanType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) return RHI::MessageSource::Performance;
        if (vulkanType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) return RHI::MessageSource::General;
        return RHI::MessageSource::API;
    }

    const char* ConfigConverter::messageSourceToString(RHI::MessageSource source) {
        switch (source) {
        case RHI::MessageSource::General: return "General";
        case RHI::MessageSource::Validation: return "Validation";
        case RHI::MessageSource::Performance: return "Performance";
        case RHI::MessageSource::Shader: return "Shader";
        case RHI::MessageSource::API: return "API";
        default: return "Unknown";
        }
    }

    const char* ConfigConverter::messageSeverityToString(StarryEngine::RHI::MessageSeverity severity) {
        switch (severity) {
        case RHI::MessageSeverity::Verbose: return "Verbose";
        case RHI::MessageSeverity::Info: return "Info";
        case RHI::MessageSeverity::Warning: return "Warning";
        case RHI::MessageSeverity::Error: return "Error";
        case RHI::MessageSeverity::Critical: return "Critical";
        default: return "Unknown";
        }
    }

    bool VulkanRHI::initialize(const RHI::RHIInitConfig& config) {
        if (!config.windowHandle) {
            if (config.debugCallback) config.debugCallback(RHI::MessageSeverity::Error,RHI::MessageSource::API,"VulkanStarryEngine::RHI::initialize: windowHandle is null!");
            return false;
        }

        auto instanceConfig = ConfigConverter::convertInstanceConfig(config);
        auto deviceConfig = ConfigConverter::convertDeviceConfig(config);
        auto swapChainConfig = ConfigConverter::convertSwapChainConfig(config);

        // 使用实际帧缓冲尺寸（考虑 HiDPI / scaleToMonitor 缩放），而非逻辑窗口尺寸
        {
            int fbWidth = 0, fbHeight = 0;
            GLFWwindow* fbWindow = static_cast<GLFWwindow*>(config.windowHandle);
            glfwGetFramebufferSize(fbWindow, &fbWidth, &fbHeight);
            if (fbWidth > 0 && fbHeight > 0) {
                swapChainConfig.width = static_cast<uint32_t>(fbWidth);
                swapChainConfig.height = static_cast<uint32_t>(fbHeight);
            }
        }
        auto frameContextConfig = ConfigConverter::convertFrameContextConfig(config);

        try {
            mInstance = VulkanInstance::create(instanceConfig);
        }
        catch (const std::exception& e) {
            if (config.debugCallback) config.debugCallback(RHI::MessageSeverity::Error,RHI::MessageSource::API,std::string("Failed to create Vulkan instance: ") + e.what());
            return false;
        }

        if (!mInstance || !mInstance->getHandle()) return false;

        GLFWwindow* window = static_cast<GLFWwindow*>(config.windowHandle);

        if (!glfwGetWindowAttrib(window, GLFW_VISIBLE)) {
            std::cerr << "[WARNING] Window is not visible when creating surface" << std::endl;
        }

        VkResult result = glfwCreateWindowSurface(mInstance->getHandle(), window, nullptr, &mSurface);

        if (result != VK_SUCCESS) {
            std::string errorMsg = "Failed to create window surface! Error: ";
            switch (result) {
            case VK_ERROR_OUT_OF_HOST_MEMORY: errorMsg += "OUT_OF_HOST_MEMORY"; break;
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: errorMsg += "OUT_OF_DEVICE_MEMORY"; break;
            case VK_ERROR_EXTENSION_NOT_PRESENT: errorMsg += "EXTENSION_NOT_PRESENT"; break;
            default: errorMsg += std::to_string(result); break;
            }

            if (config.debugCallback) config.debugCallback(RHI::MessageSeverity::Error, RHI::MessageSource::API, errorMsg);
            return false;
        }

        try {
            mDevice = VulkanDevice::create(mInstance, mSurface, deviceConfig);
        }
        catch (const std::exception& e) {
            std::string errorMsg = std::string("Failed to create device: ") + e.what();
            std::cerr << "[ERROR] " << errorMsg << std::endl;

            if (config.debugCallback) config.debugCallback(RHI::MessageSeverity::Error,RHI::MessageSource::API,errorMsg);
            mDevice->destroySurface(mSurface);
            return false;
        }

        if (!mDevice || !mDevice->getLogicalDevice()) {
            std::cerr << "[ERROR] Device creation failed - null device" << std::endl;
            mDevice->destroySurface(mSurface);
            return false;
        }

        try {
            if (deviceConfig.enableVMA) {
                if (!mDevice->initializeVMA()) {
                    std::cerr << "[WARNING] VMA initialization failed, but continuing..." << std::endl;
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[WARNING] VMA initialization error: " << e.what() << std::endl;
        }

        try {
            mSwapChain = VulkanSwapChain::create(mDevice, mSurface, swapChainConfig);

            if (!mSwapChain || !mSwapChain->isValid()) {
                throw std::runtime_error("Swap chain creation returned invalid object");
            }

        }
        catch (const std::exception& e) {
            std::string errorMsg = std::string("Failed to create swap chain: ") + e.what();
            std::cerr << "[ERROR] " << errorMsg << std::endl;

            if (config.debugCallback) config.debugCallback(RHI::MessageSeverity::Error, RHI::MessageSource::API, errorMsg);

            clear();
            return false;
        }

        try {
            mFrameContext = VulkanFrameContext::create(mDevice, frameContextConfig);

            auto queueFamilyIndices = mDevice->getQueueFamilyIndices();
            if (!queueFamilyIndices.graphicsFamily.has_value()) {
                throw std::runtime_error("No graphics queue family found!");
            }

            if (!mFrameContext->initialize(queueFamilyIndices.graphicsFamily.value())) {
                throw std::runtime_error("Failed to initialize frame context!");
            }

            mFrameContext->initializePerImageSemaphores(mSwapChain->getImageCount());
        }
        catch (const std::exception& e) {
            std::string errorMsg = std::string("Failed to create frame context: ") + e.what();
            std::cerr << "[ERROR] " << errorMsg << std::endl;

            if (config.debugCallback) config.debugCallback(RHI::MessageSeverity::Error,RHI::MessageSource::API,errorMsg);

            clear();
            return false;
        }

        mWidth = swapChainConfig.width;
        mHeight = swapChainConfig.height;

        mAcquireFunc = [this](VkSemaphore semaphore, VkFence fence, uint32_t& index) {
            return mSwapChain->acquireNextImage(semaphore, fence, UINT64_MAX, index);
        };
        mPresentFunc = [this](VkQueue queue, uint32_t index, VkSemaphore semaphore) {
            return mSwapChain->present(queue, index, semaphore);
        };

        mFrameContext->setRecreateCallback([this](uint32_t width, uint32_t height) {
            return mSwapChain->recreate(width, height);
        });

        // 异步读回槽位（帧序列导出的缓存）：每槽 1 个持久命令缓冲 + 1 个 fence。
        // 专用非 transient 池（见 m_readbackPool 注释）。
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(mDevice->getLogicalDevice(), &poolInfo, nullptr, &m_readbackPool) != VK_SUCCESS) {
            return false;
        }
        for (uint32_t i = 0; i < kReadbackSlotCount; ++i) {
            VkCommandBufferAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool = m_readbackPool;
            allocInfo.commandBufferCount = 1;
            VkCommandBuffer cmd = VK_NULL_HANDLE;
            vkAllocateCommandBuffers(mDevice->getLogicalDevice(), &allocInfo, &cmd);

            VkFenceCreateInfo fenceInfo = {};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            VkFence fence = VK_NULL_HANDLE;
            vkCreateFence(mDevice->getLogicalDevice(), &fenceInfo, nullptr, &fence);

            m_readbackCmd.push_back(cmd);
            m_readbackFence.push_back(fence);
            m_readbackSubmitted.push_back(0);
        }

        auto factory = std::make_shared<StarryEngine::RHI::VKResourceFactory>(mDevice);
        mResourceManager = std::make_shared<StarryEngine::RHI::ResourceManager>(factory);
        mResourceManager->setDebugMode(config.enableDebug);

        if (config.debugCallback) {
            std::string info = "Vulkan RHI initialized successfully!\n";
            config.debugCallback(RHI::MessageSeverity::Info, RHI::MessageSource::API, info);
        }
        return true;
    }

    void VulkanRHI::clear() {
        if (mDevice) mDevice->waitIdle();
        destroyAsyncReadbackSlots();
        destroyFramebufferResources();

        mResourceManager.reset();
        mFrameContext.reset();
        mSwapChain.reset();
        mDevice.reset();

        if (mSurface != VK_NULL_HANDLE && mInstance) {
            vkDestroySurfaceKHR(mInstance->getHandle(), mSurface, nullptr);
            mSurface = VK_NULL_HANDLE;
        }
        mInstance.reset();
    }

    void VulkanRHI::destroyAsyncReadbackSlots() {
        if (!mDevice) return;
        for (size_t i = 0; i < m_readbackCmd.size(); ++i) {
            // 只等 submit 过的 fence（clear() 已 waitIdle，必已信号，此处兜底防 pending 下销毁）；
            // 从未 submit 的 fence 绝不能 wait（永不等信号 → 死锁），直接销毁。
            if (m_readbackFence[i]) {
                if (m_readbackSubmitted[i]) {
                    vkWaitForFences(mDevice->getLogicalDevice(), 1, &m_readbackFence[i], VK_TRUE, UINT64_MAX);
                }
                vkDestroyFence(mDevice->getLogicalDevice(), m_readbackFence[i], nullptr);
            }
            if (m_readbackCmd[i]) {
                vkFreeCommandBuffers(mDevice->getLogicalDevice(), m_readbackPool, 1, &m_readbackCmd[i]);
            }
        }
        m_readbackCmd.clear();
        m_readbackFence.clear();
        m_readbackSubmitted.clear();
        if (m_readbackPool) {
            vkDestroyCommandPool(mDevice->getLogicalDevice(), m_readbackPool, nullptr);
            m_readbackPool = VK_NULL_HANDLE;
        }
    }

    bool VulkanRHI::beginAsyncReadback(uint32_t slot, RHI::RHITexture* src, RHI::RHIBuffer* dst,
                                       const RHI::BufferImageCopyRegion& region) {
        if (slot >= m_readbackCmd.size() || !src || !dst) return false;
        auto* vkTex = dynamic_cast<RHI::VulkanTexture*>(src);
        if (!vkTex) return false;
        // 上一发 fence 已信号（调用方保证），reset 后重录 + 提交。
        vkResetFences(mDevice->getLogicalDevice(), 1, &m_readbackFence[slot]);
        vkTex->readbackAsync(dst, { region }, m_readbackCmd[slot], m_readbackFence[slot]);
        m_readbackSubmitted[slot] = 1;
        return true;
    }

    bool VulkanRHI::waitAsyncReadback(uint32_t slot, uint64_t timeoutNs) {
        if (slot >= m_readbackFence.size()) return false;
        VkResult res = vkWaitForFences(mDevice->getLogicalDevice(), 1, &m_readbackFence[slot],
                                       VK_TRUE, timeoutNs);
        return res == VK_SUCCESS;
    }

    void VulkanRHI::releaseAsyncReadback(uint32_t slot) {
        // 槽位复用在 beginAsyncReadback 的 vkResetFences；此方法表达"数据已取走、槽位可归还
        // 轮换"的契约（FrameCapture 保证 waitAsyncReadback 成功后才调用）。
        (void)slot;
    }

    bool VulkanRHI::renderFrame(const std::function<void(RHI::RHICommandEncoder*, uint32_t)>& drawFunc) {
        VulkanFrameContext::FrameInfo frameInfo = mFrameContext->beginFrame(mAcquireFunc);

        if (frameInfo.needsRecreate || frameInfo.acquireResult != VK_SUCCESS) {
            if (frameInfo.needsRecreate) mFramebufferResized = true;
            return false;
        }

        auto encoder = getCommandEncoder(frameInfo.commandBuffer);
        drawFunc(encoder.get(), frameInfo.imageIndex);

        // 帧循环责任：把当前 swapchain 图像转换到 PRESENT_SRC（present 布局要求）。
        // 渲染回调只负责录制内容；无论回调画了什么，present 前必须完成此转换。
        {
            VkImage swapImage = mSwapChain->getImage(frameInfo.imageIndex);
            VkImageMemoryBarrier presentBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            presentBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            presentBarrier.srcAccessMask = 0;
            presentBarrier.dstAccessMask = 0;
            presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            presentBarrier.image = swapImage;
            presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            presentBarrier.subresourceRange.baseMipLevel = 0;
            presentBarrier.subresourceRange.levelCount = 1;
            presentBarrier.subresourceRange.baseArrayLayer = 0;
            presentBarrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(
                frameInfo.commandBuffer,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0, 0, nullptr, 0, nullptr,
                1, &presentBarrier);
        }

        mFrameContext->endFrame(frameInfo);

        VkResult presentResult = mFrameContext->submitFrame(frameInfo, mDevice->getGraphicsQueue(), mPresentFunc);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
            mFramebufferResized = true;
            return false;
        }
        return true;
    }

    bool VulkanRHI::recreateSwapChain(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) {
            std::cerr << "[VulkanRHI] Attempted to recreate swap chain with zero dimension" << std::endl;
            return false;
        }

        mDevice->waitIdle();
        if (!mSwapChain->recreate(width, height)) return false;

        mWidth = width;
        mHeight = height;
        mFrameContext->resetAllFrames();
        mFrameContext->initializePerImageSemaphores(mSwapChain->getImageCount());

        if (mRenderPassHandle.isValid()) {
            destroyFramebufferResources();
            if (!createDepthTexture() || !createFramebuffers()) {
                std::cerr << "Failed to recreate framebuffer resources\n";
                return false;
            }
        }
        return true;
    }

    void VulkanRHI::setupFramebuffers(RHI::RenderPassHandle renderPass) {
        mRenderPassHandle = renderPass;
        mDepthFormat = getDepthFormat();
        if (mDepthFormat == RHI::Format::Undefined) {
            throw std::runtime_error("No suitable depth format found");
        }
        if (!createDepthTexture()) {
            throw std::runtime_error("Failed to create depth texture");
        }
        if (!createFramebuffers()) {
            throw std::runtime_error("Failed to create framebuffers");
        }
    }

    bool VulkanRHI::createDepthTexture() {
        if (mDepthFormat == RHI::Format::Undefined) {
            mDepthFormat = getDepthFormat();
            if (mDepthFormat == RHI::Format::Undefined) return false;
        }

        RHI::TextureDesc depthDesc;
        depthDesc.extent = { mWidth, mHeight, 1 };
        depthDesc.format = mDepthFormat;
        depthDesc.type = RHI::TextureType::Texture2D;
        depthDesc.allowDepthStencil = true;
        depthDesc.debugName = "MainDepthTexture";

        mDepthTextureHandle = mResourceManager->createTexture(depthDesc, "MainDepthTexture");
        return mDepthTextureHandle.isValid();
    }

    bool VulkanRHI::createFramebuffers() {
        for (const auto& fboHandle : mFramebuffers) {
            mResourceManager->destroy(fboHandle);
        }
        mFramebuffers.clear();

        for (size_t i = 0; i < mSwapChain->getImageCount(); ++i) {
            RHI::FramebufferDesc fboDesc;
            fboDesc.extent = { mSwapChain->getExtent().width, mSwapChain->getExtent().height };
            fboDesc.layers = 1;
            fboDesc.debugName = "framebuffer_" + std::to_string(i);

            // 附件 = 交换链图像视图（原生，无 TextureHandle）+ 可选深度纹理（句柄）
            std::vector<RHI::TextureHandle> attachments;
            // 交换链图像没有 TextureHandle（由 swapchain 直接持有 view），
            // 通过 FramebufferDesc::nativeAttachments 按顺序（颜色在前）传入，
            // VulkanFramebuffer 先拼原生视图再拼 handle 默认视图。
            fboDesc.nativeAttachments.push_back(mSwapChain->getImageView(i));
            if (mDepthTextureHandle != RHI::TextureHandle::Null()) {
                attachments.push_back(mDepthTextureHandle);
            }

            auto fbHandle = mResourceManager->createFramebuffer(
                mRenderPassHandle, attachments, fboDesc,
                fboDesc.debugName);
            if (!fbHandle.isValid()) {
                std::cerr << "[Error] Failed to create framebuffer for image " << i << std::endl;
                for (auto& h : mFramebuffers) {
                    mResourceManager->destroy(h);
                }
                return {};
            }
            mFramebuffers.push_back(fbHandle);
        }
        return !mFramebuffers.empty();
    }

    void VulkanRHI::destroyFramebufferResources() {
        for (const auto& fboHandle : mFramebuffers) {
            mResourceManager->destroy(fboHandle);
        }

        mFramebuffers.clear();
        if (mDepthTextureHandle.isValid()) {
            mResourceManager->destroy(mDepthTextureHandle);
            mDepthTextureHandle = RHI::TextureHandle::Null();
        }
    }

    RHI::Format VulkanRHI::getDepthFormat() const {
        if (!mDevice) return RHI::Format::Undefined;

        switch (mDevice->findDepthFormat()) {
        case VK_FORMAT_D16_UNORM:          return RHI::Format::D16_UNorm;
        case VK_FORMAT_D32_SFLOAT:         return RHI::Format::D32_Float;
        case VK_FORMAT_D24_UNORM_S8_UINT:  return RHI::Format::D24_UNorm_S8_UInt;
        default: return RHI::Format::Undefined;
        }
    }

    std::unique_ptr<RHI::RHICommandEncoder> VulkanRHI::getCommandEncoder(VkCommandBuffer cmdBuf) const {
        return std::make_unique<RHI::VulkanCommandEncoder>(mDevice, cmdBuf, mResourceManager.get());
    }

    std::unique_ptr<RHI::RHICommandEncoder> VulkanRHI::allocateSecondaryCommandEncoder(uint32_t workerIndex) const {
        // 在 job 内（执行线程）调用：workerIndex 是执行线程的索引 → 只碰该 worker 自己的池。
        // inline 软开关下 job 跑在主线程，currentWorkerIndex() = 哨兵 → 收敛到 0 号池（串行，安全）。
        const uint32_t idx = workerIndex;   // Refactor 无 JobSystem worker 索引概念，直接使用
        const uint32_t slot = mFrameContext->getCurrentFrameIndex();   // Phase 1 内 mCurrentFrameIndex 稳定
        VkCommandBuffer secondary = mFrameContext->allocateWorkerCommandBuffer(slot, idx, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        if (secondary == VK_NULL_HANDLE) {
            std::cerr << "[VulkanRHI] allocateSecondaryCommandEncoder: slot " << slot << " worker " << idx << " 分配失败" << std::endl;
            return nullptr;
        }
        return getCommandEncoder(secondary);
    }

    void VulkanRHI::prepareParallelRecording(uint32_t workerCount) const {
        mFrameContext->reserveWorkerCommandPools(workerCount);
    }

    void VulkanRHI::updateDescriptorSet(RHI::DescriptorSetHandle setHandle, uint32_t binding, uint32_t arrayElement,
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

    void VulkanRHI::updateDescriptorSet(RHI::DescriptorSetHandle setHandle, uint32_t binding, uint32_t arrayElement, const RHI::DescriptorImageInfo& imageInfo) {
        auto* set = mResourceManager->getDescriptorSet(setHandle);
        if (set) {
            auto* texture = mResourceManager->getTexture(imageInfo.texture);
            auto* sampler = mResourceManager->getSampler(imageInfo.sampler);
            if (texture && sampler) {
                set->writeTexture(binding, arrayElement, texture, sampler, imageInfo.imageLayout);
                set->update();
            }
        }
    }
}