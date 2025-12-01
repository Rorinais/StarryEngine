#pragma once
#include "SwapChain.hpp"
#include "../vulkanCore/VulkanCore.hpp"
#include "../../../resourceManager/textures/Texture.hpp"
#include "../renderContext/RenderContext.hpp"

namespace StarryEngine {
    class WindowContext {
    public:
        using Ptr = std::shared_ptr<WindowContext>;
        static Ptr create() { return std::make_shared<WindowContext>(); }

        ~WindowContext() {
            mSwapChain.reset();
        }


        void init(VulkanCore::Ptr vulkanCore, Window::Ptr window, CommandPool::Ptr commandPool);
        void cleanupSwapchain();
        void recreateSwapchain();
        void onSwapchainRecreated();

        SwapChain::Ptr getSwapChain() const { return mSwapChain; }
        CommandPool::Ptr getCommandPool() const { return mCommandPool; }
        VkExtent2D getSwapchainExtent() const { return mSwapChain->getExtent(); }
        VkFormat getSwapchainFormat() const { return mSwapChain->getImageFormat(); }
        const std::vector<VkImageView>& getSwapchainImageViews() const {
            return mSwapChain->getImageViews();
        }
        uint32_t getSwapchainImageCount() const { return mSwapChain->getImageCount(); }
        VkSurfaceKHR getSurface() const { return mVulkanCore->getSurface(); }
        bool isInitialized() const;
    private:
        bool mInitialized = false;
        StarryEngine::VulkanCore::Ptr mVulkanCore;
        StarryEngine::Window::Ptr mWindow;
        StarryEngine::CommandPool::Ptr mCommandPool;
        StarryEngine::SwapChain::Ptr mSwapChain;
    };
} 
