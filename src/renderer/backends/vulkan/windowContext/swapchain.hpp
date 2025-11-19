#pragma once
#include "../../../core/platform/Window.hpp" 
#include "../vulkanCore/VulkanCore.hpp"

namespace StarryEngine {
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    class SwapChain {
    public:
        using Ptr = std::shared_ptr<SwapChain>;
        static Ptr create(const LogicalDevice::Ptr& logicalDevice, VkSurfaceKHR surface, const Window::Ptr& window) {
            return std::make_shared<SwapChain>(logicalDevice, surface, window);
        }

        SwapChain(const LogicalDevice::Ptr& logicalDevice, VkSurfaceKHR surface, const Window::Ptr& window);
        ~SwapChain();

        void recreate();
        void cleanup();

        static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

        VkSwapchainKHR getHandle() const { return mSwapChain; }
        const std::vector<VkImage>& getImages() const { return mSwapChainImages; }
        const std::vector<VkImageView>& getImageViews() const { return mSwapChainImageViews; }
        VkFormat getImageFormat() const { return mSwapChainImageFormat; }
        const VkExtent2D& getExtent() const { return mSwapChainExtent; }
        uint32_t getImageCount() const { return static_cast<uint32_t>(mSwapChainImages.size()); }

    private:
        void createSwapChain();
        void createImageViews();

        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    private:
        LogicalDevice::Ptr mLogicalDevice;
        StarryEngine::Window::Ptr mWindow;
        VkSurfaceKHR mSurface;

        VkSwapchainKHR mSwapChain = VK_NULL_HANDLE;
        std::vector<VkImage> mSwapChainImages;
        std::vector<VkImageView> mSwapChainImageViews;
        VkFormat mSwapChainImageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D mSwapChainExtent = { 0, 0 };
    };
}