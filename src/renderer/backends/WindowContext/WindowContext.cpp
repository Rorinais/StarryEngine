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

        createDepthResources();
        mInitialized = true;
    }

    void WindowContext::cleanupSwapchain() {
        if (mDepthTexture) {
            mDepthTexture->cleanup();
            mDepthTexture.reset();
        }

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

        createDepthResources();
    }

    void WindowContext::createDepthResources() {
        if (mDepthTexture) {
            mDepthTexture.reset();
        }
        mDepthTexture = Texture::create(
            mVulkanCore->getLogicalDevice(),
            Texture::Type::Depth,
            VkExtent2D{
                mSwapChain->getExtent().width,
                mSwapChain->getExtent().height
            },
            mCommandPool
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