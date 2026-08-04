#include "Device.hpp"
#include "Swapchain.hpp"

namespace StarryEngine {

    SwapChain::SwapChain(std::shared_ptr<Device> device, VkSurfaceKHR surface, const SwapChainConfig& config)
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
        uint64_t timeout,
        uint32_t& outImageIndex) {
        if (mSwapChain == VK_NULL_HANDLE) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(
            mDevice->getLogicalDevice(),
            mSwapChain,
            timeout,
            imageAvailableSemaphore,
            fence,
            &imageIndex
        );

        switch (result) {
        case VK_SUCCESS:
            mOutOfDate = false;
            mSuboptimal = false;
            outImageIndex = imageIndex;  
            break;

        case VK_SUBOPTIMAL_KHR:
            mSuboptimal = true;
            outImageIndex = imageIndex;  
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
            mOutOfDate = true;
            return false;
        }

        mDevice->waitIdle();

        VkSwapchainKHR oldSwapchain = mSwapChain;
        std::vector<VkImageView> oldImageViews = std::move(mImageViews);
        mImageViews.clear();

        mConfig.width = newWidth;
        mConfig.height = newHeight;

        if (!createSwapChain(mConfig.width, mConfig.height, oldSwapchain)) {
            std::cerr << "[ERROR] Failed to create new swap chain" << std::endl;
            return false;
        }

        for (auto imageView : oldImageViews) {
            mDevice->destroyImageView(imageView);
        }

        if (oldSwapchain != VK_NULL_HANDLE) {
            mDevice->destroySwapChain(oldSwapchain);
        }

        if (!createImageViews()) {
            std::cerr << "[ERROR] Failed to create image views for new swap chain" << std::endl;
            return false;
        }

        mOutOfDate = false;
        mSuboptimal = false;
        return true;
    }

    bool SwapChain::createSwapChain(uint32_t width, uint32_t height, VkSwapchainKHR oldSwapchain) {
        auto support = mDevice->querySwapChainSupport();
        if (support.formats.empty() || support.presentModes.empty()) return false;

        mSurfaceFormat = chooseSurfaceFormat(support.formats);
        mPresentMode = choosePresentMode(support.presentModes);
        mExtent = chooseExtent(support.capabilities, width, height);
        mFormat = mSurfaceFormat.format;

        uint32_t imageCount = std::clamp(mConfig.minImageCount,
            support.capabilities.minImageCount,
            support.capabilities.maxImageCount > 0 ? support.capabilities.maxImageCount : UINT32_MAX);

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = mSurface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = mSurfaceFormat.format;
        createInfo.imageColorSpace = mSurfaceFormat.colorSpace;
        createInfo.imageExtent = mExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = mConfig.imageUsage;

        auto queueIndices = mDevice->getQueueFamilyIndices();
        uint32_t queueFamilyIndices[] = { queueIndices.graphicsFamily.value(), queueIndices.presentFamily.value() };
        if (queueIndices.graphicsFamily != queueIndices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = support.capabilities.currentTransform;
        createInfo.compositeAlpha = pickCompositeAlpha(support.capabilities.supportedCompositeAlpha, mConfig.compositeAlpha);
        createInfo.presentMode = mPresentMode;
        createInfo.clipped = mConfig.clipped;
        createInfo.oldSwapchain = oldSwapchain;  

        VkResult result = vkCreateSwapchainKHR(mDevice->getLogicalDevice(), &createInfo, nullptr, &mSwapChain);
        if (result != VK_SUCCESS) return false;

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
                    0,
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
            mDevice->destroyImageView(imageView);

        }
        mImageViews.clear();
		mDevice->destroySwapChain(mSwapChain);
        mImages.clear();
    }

    VkSurfaceFormatKHR SwapChain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const {
        for (const auto& format : formats) {
            if (format.format == mConfig.surfaceFormat.format &&
                format.colorSpace == mConfig.surfaceFormat.colorSpace) {
                return format;
            }
        }

        for (const auto& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }

        return formats[0];
    }

    VkPresentModeKHR SwapChain::choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) const {
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

        return mConfig.presentMode;
    }

    VkExtent2D SwapChain::chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities,uint32_t width, uint32_t height) const {
        VkExtent2D actualExtent = { width, height };

        actualExtent.width = std::clamp(actualExtent.width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);

        actualExtent.height = std::clamp(actualExtent.height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);

        return actualExtent;
    }

    VkCompositeAlphaFlagBitsKHR SwapChain::pickCompositeAlpha(
        VkCompositeAlphaFlagsKHR supported, VkCompositeAlphaFlagBitsKHR requested) const {
        // 透明窗口请求 alpha 合成：优先 PRE_MULTIPLIED，其次 POST_MULTIPLIED；
        // 表面不支持 alpha（例如某些平台/合成器关闭）则回退 OPAQUE（规范保证必然支持）
        if (requested != VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
            if (supported & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) return VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
            if (supported & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) return VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
            std::cerr << "[SwapChain] 表面不支持 alpha 合成，透明窗口回退为不透明" << std::endl;
        }
        return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
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
        std::cout << std::endl;
    }

} // namespace StarryEngine