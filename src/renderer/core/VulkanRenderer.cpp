#include "VulkanRenderer.hpp"
#include "../resources/shaders/ShaderBuilder.hpp"

namespace StarryEngine {
    void VulkanRenderer::init(VulkanCore::Ptr core, WindowContext::Ptr wContext) {
        mVulkanCore = core;
        mWindowContext = wContext;
        createSyncObjects();
    }

    VulkanRenderer::~VulkanRenderer() {
        shutdown();
    }

    void VulkanRenderer::shutdown() {
        if (!mVulkanCore || !mVulkanCore->getLogicalDeviceHandle()) return;

        vkDeviceWaitIdle(mVulkanCore->getLogicalDeviceHandle());
        cleanupSyncObjects();
        mRenderGraph.reset();
    }

    void VulkanRenderer::setRenderGraph(RenderGraph renderGraph) {
        mRenderGraph = renderGraph;
        mRenderGraph->compile(mVulkanCore, mWindowContext);
    }

    void VulkanRenderer::createSyncObjects() {
        mFrameContexts.resize(MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            mFrameContexts[i].imageAvailableSemaphore = Semaphore::create(mVulkanCore->getLogicalDevice());
            mFrameContexts[i].renderFinishedSemaphore = Semaphore::create(mVulkanCore->getLogicalDevice());
            mFrameContexts[i].inFlightFence = Fence::create(mVulkanCore->getLogicalDevice(), VK_FENCE_CREATE_SIGNALED_BIT);
            mFrameContexts[i].mainCommandBuffer = CommandBuffer::create(mVulkanCore->getLogicalDevice(), mWindowContext->getCommandPool());
        }
    }

    void VulkanRenderer::cleanupSyncObjects() {
        for (auto& frame : mFrameContexts) {
            if (frame.imageAvailableSemaphore) frame.imageAvailableSemaphore.reset();
            if (frame.renderFinishedSemaphore) frame.renderFinishedSemaphore.reset();
            if (frame.inFlightFence) frame.inFlightFence.reset();
        }
        mFrameContexts.clear();
    }

    void VulkanRenderer::beginFrame() {
        auto& currentFrameContext = mFrameContexts[mCurrentFrame];

        currentFrameContext.inFlightFence->block();
        currentFrameContext.inFlightFence->resetFence();

        VkResult result = vkAcquireNextImageKHR(
            mVulkanCore->getLogicalDeviceHandle(),
            mWindowContext->getSwapChain()->getHandle(),
            UINT64_MAX,
            currentFrameContext.imageAvailableSemaphore->getHandle(),
            VK_NULL_HANDLE,
            &mImageIndex
        );

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Failed to acquire swap chain image!");
        }
    }

    void VulkanRenderer::renderFrame() {
        auto& ctx = mFrameContexts[mCurrentFrame];
        ctx.mainCommandBuffer->reset();
        ctx.mainCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        // 执行Render Graph
        if (mRenderGraph) {
            mRenderGraph->execute(ctx.mainCommandBuffer, mImageIndex, mCurrentFrame);
        }

        ctx.mainCommandBuffer->end();

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { ctx.imageAvailableSemaphore->getHandle() };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        VkCommandBuffer commandBuffers[] = { ctx.mainCommandBuffer->getHandle() };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = commandBuffers;

        VkSemaphore signalSemaphores[] = { ctx.renderFinishedSemaphore->getHandle() };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VkFence fence = ctx.inFlightFence->getHandle();
        vkQueueSubmit(mVulkanCore->getGraphicsQueue(), 1, &submitInfo, fence);
    }

    void VulkanRenderer::endFrame() {
        VkSemaphore signalSemaphores[] = { mFrameContexts[mCurrentFrame].renderFinishedSemaphore->getHandle() };

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { mWindowContext->getSwapChain()->getHandle() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &mImageIndex;
        presentInfo.pResults = nullptr;

        VkResult result = vkQueuePresentKHR(mVulkanCore->getPresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swap chain image!");
        }

        mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void VulkanRenderer::recreateSwapChain() {
        vkDeviceWaitIdle(mVulkanCore->getLogicalDeviceHandle());
        mWindowContext->recreateSwapchain();

        if (mRenderGraph) {
            try {
                mRenderGraph->onSwapchainRecreated();
            }
            catch (const std::exception& e) {
                throw std::runtime_error("RenderGraph swapchain recreation failed: " + std::string(e.what()));
            }
        }
    }
}