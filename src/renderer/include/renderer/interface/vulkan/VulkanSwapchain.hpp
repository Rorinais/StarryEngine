#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>

namespace StarryEngine {
    class VulkanDevice;

    struct SwapChainConfig {
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        VkSurfaceFormatKHR surfaceFormat = { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        VkBool32 clipped = VK_TRUE;

        uint32_t width = 0;
        uint32_t height = 0;

        bool enableMailboxMode = false;
        bool enableImmediateMode = false;
        uint32_t minImageCount = 2;  
    };

    class VulkanSwapChain {
    public:
        using Ptr = std::shared_ptr<VulkanSwapChain>;

        static Ptr create(std::shared_ptr<VulkanDevice> device, VkSurfaceKHR surface, const SwapChainConfig& config) {
            return std::make_shared<VulkanSwapChain>(device, surface, config);
        }

        VulkanSwapChain(std::shared_ptr<VulkanDevice> device, VkSurfaceKHR surface, const SwapChainConfig& config);
        ~VulkanSwapChain();

        VulkanSwapChain(const VulkanSwapChain&) = delete;
        VulkanSwapChain& operator=(const VulkanSwapChain&) = delete;

        VkResult acquireNextImage(VkSemaphore imageAvailableSemaphore,
            VkFence fence,
            uint64_t timeout,
            uint32_t& outImageIndex);

        VkResult present(VkQueue presentQueue,
            uint32_t imageIndex,
            VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE);

        bool recreate(uint32_t newWidth = 0, uint32_t newHeight = 0);

        VkSwapchainKHR getHandle() const { return mSwapChain; }
        VkExtent2D getExtent() const { return mExtent; }
        VkFormat getFormat() const { return mFormat; }
        VkSurfaceFormatKHR getSurfaceFormat() const { return mSurfaceFormat; }

        const std::vector<VkImage>& getImages() const { return mImages; }
        const std::vector<VkImageView>& getImageViews() const { return mImageViews; }

        VkImage getImage(uint32_t index) const {
            return (index < mImages.size()) ? mImages[index] : VK_NULL_HANDLE;
        }

        VkImageView getImageView(uint32_t index) const {
            return (index < mImageViews.size()) ? mImageViews[index] : VK_NULL_HANDLE;
        }

        uint32_t getImageCount() const { return static_cast<uint32_t>(mImages.size()); }

        bool isValid() const { return mSwapChain != VK_NULL_HANDLE; }
        bool isOutOfDate() const { return mOutOfDate; }
        bool isSuboptimal() const { return mSuboptimal; }

        void printInfo() const;

    private:
        bool createSwapChain(uint32_t width, uint32_t height, VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
        bool createImageViews();
        void cleanupSwapChain();

        VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
        VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) const;
        VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) const;
        // 依据表面能力验证/挑选 composite alpha：透明窗口请求 alpha，不支持则回退 OPAQUE
        VkCompositeAlphaFlagBitsKHR pickCompositeAlpha(
            VkCompositeAlphaFlagsKHR supported, VkCompositeAlphaFlagBitsKHR requested) const;

    private:
        std::shared_ptr<VulkanDevice> mDevice;
        VkSurfaceKHR mSurface;
        SwapChainConfig mConfig;

        VkSwapchainKHR mSwapChain = VK_NULL_HANDLE;
        VkExtent2D mExtent = {0,0};
        VkFormat mFormat = VK_FORMAT_UNDEFINED;
        VkSurfaceFormatKHR mSurfaceFormat = {};
        VkPresentModeKHR mPresentMode = VK_PRESENT_MODE_FIFO_KHR;

        std::vector<VkImage> mImages;
        std::vector<VkImageView> mImageViews;

        bool mOutOfDate = false;
        bool mSuboptimal = false;
    };

} // namespace StarryEngine