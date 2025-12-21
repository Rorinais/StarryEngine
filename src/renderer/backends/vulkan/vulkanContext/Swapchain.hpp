#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>

namespace StarryEngine {

    struct SwapChainConfig {
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        VkSurfaceFormatKHR surfaceFormat = { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        VkBool32 clipped = VK_TRUE;

        // 窗口尺寸（0表示使用表面当前尺寸）
        uint32_t width = 0;
        uint32_t height = 0;

        // 高级选项
        bool enableMailboxMode = false;
        bool enableImmediateMode = false;
        uint32_t minImageCount = 2;  // 最小图像数量
    };

    class SwapChain {
    public:
        using Ptr = std::shared_ptr<SwapChain>;

        static Ptr create(std::shared_ptr<Device> device, VkSurfaceKHR surface, const SwapChainConfig& config = SwapChainConfig()) {
            return std::make_shared<SwapChain>(device, surface, config);
        }

        SwapChain(std::shared_ptr<Device> device, VkSurfaceKHR surface, const SwapChainConfig& config = SwapChainConfig());
        ~SwapChain();

        // 禁用拷贝
        SwapChain(const SwapChain&) = delete;
        SwapChain& operator=(const SwapChain&) = delete;

        // === 核心功能 ===
        VkResult acquireNextImage(VkSemaphore imageAvailableSemaphore,
            VkFence fence = VK_NULL_HANDLE,
            uint64_t timeout = UINT64_MAX);

        VkResult present(VkQueue presentQueue,
            uint32_t imageIndex,
            VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE);

        // === 重建 ===
        bool recreate(uint32_t newWidth = 0, uint32_t newHeight = 0);

        // === 资源访问 ===
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
        uint32_t getCurrentImageIndex() const { return mCurrentImageIndex; }

        // === 状态查询 ===
        bool isValid() const { return mSwapChain != VK_NULL_HANDLE; }
        bool isOutOfDate() const { return mOutOfDate; }
        bool isSuboptimal() const { return mSuboptimal; }

        // === 调试 ===
        void printInfo() const;

    private:
        // 内部创建函数
        bool createSwapChain(uint32_t width, uint32_t height);
        bool createImageViews();
        void cleanupSwapChain();

        // 支持信息查询
        struct SupportDetails {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };

        SupportDetails querySupport() const;
        VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
        VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) const;
        VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) const;

    private:
        std::shared_ptr<Device> mDevice;
        VkSurfaceKHR mSurface;
        SwapChainConfig mConfig;

        // 交换链句柄和属性
        VkSwapchainKHR mSwapChain = VK_NULL_HANDLE;
        VkExtent2D mExtent = {0,0};
        VkFormat mFormat = VK_FORMAT_UNDEFINED;
        VkSurfaceFormatKHR mSurfaceFormat = {};
        VkPresentModeKHR mPresentMode = VK_PRESENT_MODE_FIFO_KHR;

        // 交换链图像
        std::vector<VkImage> mImages;
        std::vector<VkImageView> mImageViews;

        // 状态
        bool mOutOfDate = false;
        bool mSuboptimal = false;
        uint32_t mCurrentImageIndex = 0;
    };

} // namespace StarryEngine