#include "VulkanRenderer.hpp"
#include "../core/platform/Window.hpp"
#include "backends/vulkan/VulkanBackend.hpp"
#include <iostream>

namespace StarryEngine {

    void VulkanRenderer::init(std::shared_ptr<Window> window) {
        mWindow = window;  // 保存窗口引用
        
        mVulkanCore = VulkanCore::create();
        mVulkanCore->init(window);

        auto commandPool = CommandPool::create(mVulkanCore->getLogicalDevice());
        mWindowContext = WindowContext::create();
        mWindowContext->init(mVulkanCore, window, commandPool);

        mBackend = VulkanBackend::create();

        if (!mBackend->initialize(mVulkanCore, mWindowContext)) {
            throw std::runtime_error("Failed to initialize Vulkan backend");
        }
    }

    void VulkanRenderer::shutdown() {
        mBackend->shutdown();
        mWindowContext.reset();
        mVulkanCore.reset();
        mWindow.reset();  // 释放窗口引用
    }

    void VulkanRenderer::beginFrame() {
        // TODO: 实现帧开始逻辑
    }

    void VulkanRenderer::endFrame() {
        // TODO: 实现帧结束逻辑
    }

    bool VulkanRenderer::shouldClose() const {
        if (!mWindow) return true;
        return mWindow->shouldClose();  // 返回窗口是否应该关闭
    }

    void VulkanRenderer::pollEvents() {
        if (mWindow) {
            mWindow->pollEvents();  // 通过窗口轮询事件
        }
    }

    void VulkanRenderer::render() {
        // 只保留一个 render() 函数
        if (!mBackend) return;
        
        mBackend->beginFrame();
        VkCommandBuffer cmd = mBackend->getCommandBuffer();
        uint32_t frameIndex = mBackend->getCurrentFrameIndex();
        // TODO: 这里可以添加渲染命令
        mBackend->submitFrame();
    }

    void VulkanRenderer::onSwapchainRecreated() {
        if (mBackend) {
            mBackend->onSwapchainRecreated();
        }
    }

    void VulkanRenderer::createFramebuffers(std::vector<VkFramebuffer>& framebuffer, 
                                           VkImageView depthTexture, 
                                           VkRenderPass renderpass) {
        if (!mVulkanCore || !mWindowContext) {
            throw std::runtime_error("VulkanRenderer not initialized");
        }
        
        auto swapchain = mWindowContext->getSwapChain();
        auto imageViews = swapchain->getImageViews();
        framebuffer.resize(imageViews.size());
        auto device = mVulkanCore->getLogicalDeviceHandle();
        auto extent = mWindowContext->getSwapchainExtent();

        for (size_t i = 0; i < imageViews.size(); i++) {
            std::vector<VkImageView> attachments = {
                imageViews[i],
                depthTexture
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderpass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }
        
        std::cout << "Created " << framebuffer.size() << " framebuffers" << std::endl;
    }

} // namespace StarryEngine