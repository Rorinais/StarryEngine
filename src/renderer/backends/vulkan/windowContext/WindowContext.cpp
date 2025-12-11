#include "WindowContext.hpp"
namespace StarryEngine {
    void  WindowContext::init(VulkanCore::Ptr vulkanCore, Window::Ptr window, CommandPool::Ptr commandPool) {
        mVulkanCore = vulkanCore;
        mWindow = window;
        mCommandPool = commandPool;

        mSwapChain = SwapChain::create(
            mVulkanCore->getLogicalDevice(),
            mVulkanCore->getSurface(),
            mWindow
        );
        mInitialized = true;
    }

    void WindowContext::cleanupSwapchain() {
        if (mSwapChain) {
            mSwapChain->cleanup();
            mSwapChain.reset();
        }
    }

    void WindowContext::recreateSwapchain() {
        cleanupSwapchain();

        mSwapChain = SwapChain::create(
            mVulkanCore->getLogicalDevice(),
            mVulkanCore->getSurface(),
            mWindow
        );
    }

    void WindowContext::onSwapchainRecreated() {
        cleanupSwapchain();
        recreateSwapchain();
    }

    bool WindowContext::isInitialized() const {
        return mInitialized;
    }
}