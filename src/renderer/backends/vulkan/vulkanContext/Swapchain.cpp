#include "Device.hpp"
#include "SwapChain.hpp"

namespace StarryEngine {

    SwapChain::SwapChain(std::shared_ptr<Device> device, VkSurfaceKHR surface, const SwapChainConfig& config = SwapChainConfig())
        : mDevice(device), mSurface(surface), mConfig(config) {

        if (!mDevice || mSurface == VK_NULL_HANDLE) {
            throw std::runtime_error("Invalid device or surface for swap chain creation");
        }

        if (!createSwapChain(config.width, config.height)) {
            throw std::runtime_error("Failed to create swap chain");
        }

        if (!createImageViews()) {
            throw std::runtime_error("Failed to create swap chain image views");
        }
    }

    SwapChain::~SwapChain() {
        cleanupSwapChain();
    }

    VkResult SwapChain::acquireNextImage(VkSemaphore imageAvailableSemaphore,
        VkFence fence,
        uint64_t timeout) {

        if (mSwapChain == VK_NULL_HANDLE) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        VkResult result = vkAcquireNextImageKHR(
            mDevice->getLogicalDevice(),
            mSwapChain,
            timeout,
            imageAvailableSemaphore,
            fence,
            &mCurrentImageIndex
        );

        // 处理特殊结果
        switch (result) {
        case VK_SUCCESS:
            mOutOfDate = false;
            mSuboptimal = false;
            break;

        case VK_SUBOPTIMAL_KHR:
            mSuboptimal = true;
            break;

        case VK_ERROR_OUT_OF_DATE_KHR:
            mOutOfDate = true;
            break;

        case VK_ERROR_SURFACE_LOST_KHR:
            mOutOfDate = true;
            break;

        default:
            break;
        }

        return result;
    }

    VkResult SwapChain::present(VkQueue presentQueue,
        uint32_t imageIndex,
        VkSemaphore renderFinishedSemaphore) {

        if (mSwapChain == VK_NULL_HANDLE) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        if (renderFinishedSemaphore != VK_NULL_HANDLE) {
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
        }

        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &mSwapChain;
        presentInfo.pImageIndices = &imageIndex;

        VkResult result;
        presentInfo.pResults = &result;

        VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);

        // 处理呈现结果
        switch (presentResult) {
        case VK_SUCCESS:
            break;

        case VK_SUBOPTIMAL_KHR:
            mSuboptimal = true;
            break;

        case VK_ERROR_OUT_OF_DATE_KHR:
            mOutOfDate = true;
            break;

        case VK_ERROR_SURFACE_LOST_KHR:
            mOutOfDate = true;
            break;

        default:
            break;
        }

        return presentResult;
    }

    bool SwapChain::recreate(uint32_t newWidth, uint32_t newHeight) {
        if (newWidth == 0 || newHeight == 0) {
            // 窗口最小化，延迟重建
            mOutOfDate = true;
            return false;
        }

        // 等待设备空闲
        mDevice->waitIdle();

        // 清理旧资源
        cleanupSwapChain();

        // 更新尺寸
        if (newWidth > 0) mConfig.width = newWidth;
        if (newHeight > 0) mConfig.height = newHeight;

        // 创建新的交换链
        if (!createSwapChain(mConfig.width, mConfig.height)) {
            return false;
        }

        // 创建新的图像视图
        if (!createImageViews()) {
            return false;
        }

        mOutOfDate = false;
        mSuboptimal = false;

        return true;
    }

    bool SwapChain::createSwapChain(uint32_t width, uint32_t height) {
        // 查询支持信息
        auto support = mDevice->querySwapChainSupport();
        if (support.formats.empty() || support.presentModes.empty()) {
            return false;
        }

        // 选择表面格式、呈现模式和范围
        mSurfaceFormat = chooseSurfaceFormat(support.formats);
        mPresentMode = choosePresentMode(support.presentModes);
        mExtent = chooseExtent(support.capabilities, width, height);
        mFormat = mSurfaceFormat.format;

        // 确定图像数量
        uint32_t imageCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
            imageCount = support.capabilities.maxImageCount;
        }

        if (imageCount < mConfig.minImageCount) {
            imageCount = mConfig.minImageCount;
        }

        // 创建交换链
        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = mSurface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = mSurfaceFormat.format;
        createInfo.imageColorSpace = mSurfaceFormat.colorSpace;
        createInfo.imageExtent = mExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = mConfig.imageUsage;

        // 设置队列
        auto queueIndices = mDevice->getQueueFamilyIndices();
        uint32_t queueFamilyIndices[] = {
            queueIndices.graphicsFamily.value(),
            queueIndices.presentFamily.value()
        };

        if (queueIndices.graphicsFamily != queueIndices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = support.capabilities.currentTransform;
        createInfo.compositeAlpha = mConfig.compositeAlpha;
        createInfo.presentMode = mPresentMode;
        createInfo.clipped = mConfig.clipped;
        createInfo.oldSwapchain = mSwapChain;  // 用于重建

        VkResult result = vkCreateSwapchainKHR(mDevice->getLogicalDevice(), &createInfo, nullptr, &mSwapChain);
        if (result != VK_SUCCESS) {
            return false;
        }

        // 获取交换链图像
        vkGetSwapchainImagesKHR(mDevice->getLogicalDevice(), mSwapChain, &imageCount, nullptr);
        mImages.resize(imageCount);
        vkGetSwapchainImagesKHR(mDevice->getLogicalDevice(), mSwapChain, &imageCount, mImages.data());

        return true;
    }

    bool SwapChain::createImageViews() {
        mImageViews.resize(mImages.size());

        for (size_t i = 0; i < mImages.size(); i++) {
            try {
                mImageViews[i] = mDevice->createImageView(
                    mImages[i],
                    mFormat,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_VIEW_TYPE_2D,  
                    1,  // mipLevels
                    0,  // baseArrayLayer
                    1   // layerCount
                );
            }
            catch (const std::runtime_error& e) {
                std::cerr << "[ERROR] Failed to create image view for swap chain image "<< i << ": " << e.what() << std::endl;

                for (size_t j = 0; j < i; j++) {
                    mDevice->destroyImageView(mImageViews[j]);
                }
                mImageViews.clear();
                return false;
            }
        }

        return true;
    }

    void SwapChain::cleanupSwapChain() {
        for (auto& imageView : mImageViews) {
            if (imageView != VK_NULL_HANDLE) {
                mDevice->destroyImageView(imageView);
            }
        }
        mImageViews.clear();

        if (mSwapChain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(mDevice->getLogicalDevice(), mSwapChain, nullptr);
            mSwapChain = VK_NULL_HANDLE;
        }

        mImages.clear();
    }

    VkSurfaceFormatKHR SwapChain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const {
        // 首先尝试精确匹配配置
        for (const auto& format : formats) {
            if (format.format == mConfig.surfaceFormat.format &&
                format.colorSpace == mConfig.surfaceFormat.colorSpace) {
                return format;
            }
        }

        // 回退到 SRGB 格式
        for (const auto& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }

        // 返回第一个可用格式
        return formats[0];
    }

    VkPresentModeKHR SwapChain::choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) const {
        // 根据配置选择呈现模式
        if (mConfig.enableMailboxMode) {
            for (const auto& mode : presentModes) {
                if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    return mode;
                }
            }
        }

        if (mConfig.enableImmediateMode) {
            for (const auto& mode : presentModes) {
                if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                    return mode;
                }
            }
        }

        // 默认使用 FIFO（保证垂直同步）
        return mConfig.presentMode;
    }

    VkExtent2D SwapChain::chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities,
        uint32_t width, uint32_t height) const {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        }

        VkExtent2D actualExtent = { width, height };

        actualExtent.width = std::clamp(actualExtent.width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);

        actualExtent.height = std::clamp(actualExtent.height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);

        return actualExtent;
    }

    void SwapChain::printInfo() const {
        std::cout << "=== Swap Chain Information ===" << std::endl;
        std::cout << "Format: " << mFormat << std::endl;
        std::cout << "Extent: " << mExtent.width << "x" << mExtent.height << std::endl;
        std::cout << "Image Count: " << mImages.size() << std::endl;
        std::cout << "Present Mode: " << mPresentMode << std::endl;
        std::cout << "Status: " << (isValid() ? "Valid" : "Invalid") << std::endl;
        std::cout << "Out of Date: " << (isOutOfDate() ? "Yes" : "No") << std::endl;
        std::cout << "Suboptimal: " << (isSuboptimal() ? "Yes" : "No") << std::endl;
    }

} // namespace StarryEngine