#include "IVulkanBackend.h"
#include "IResourceManager.h"
#include "VulkanCore/VulkanCore.hpp"
#include "WindowContext/WindowContext.hpp"
#include "FrameContext/FrameContext.hpp"

namespace StarryEngine {

    class SimpleVulkanBackend : public IVulkanBackend {
    public:
        SimpleVulkanBackend() = default;

        bool initialize(VulkanCore::Ptr core, WindowContext::Ptr window) override {
            mVulkanCore = core;
            mWindowContext = window;

            if (!createSyncObjects()) {
                return false;
            }

            return true;
        }

        void shutdown() override {
            cleanupSyncObjects();
        }

        void beginFrame() override {
            mCurrentFrameContext = &mFrameContexts[mCurrentFrame];

            // 等待上一帧完成
            mCurrentFrameContext->inFlightFence->block();
            mCurrentFrameContext->inFlightFence->resetFence();

            // 获取交换链图像
            VkResult result = vkAcquireNextImageKHR(
                mVulkanCore->getLogicalDeviceHandle(),
                mWindowContext->getSwapChain()->getHandle(),
                UINT64_MAX,
                mCurrentFrameContext->imageAvailableSemaphore->getHandle(),
                VK_NULL_HANDLE,
                &mImageIndex
            );

            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                onSwapchainRecreated();
                return;
            }
            else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error("Failed to acquire swap chain image!");
            }

            mFrameInProgress = true;

            // 开始命令缓冲区
            mCurrentFrameContext->mainCommandBuffer->reset();
            mCurrentFrameContext->mainCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        }

        VkCommandBuffer getCommandBuffer() override {
            return mCurrentFrameContext->mainCommandBuffer->getHandle();
        }

        void submitFrame() override {
            if (!mFrameInProgress) return;

            auto& ctx = *mCurrentFrameContext;

            // 结束命令记录
            ctx.mainCommandBuffer->end();

            // 提交命令缓冲区
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

            VkQueue graphicsQueue = mVulkanCore->getGraphicsQueue();
            vkQueueSubmit(graphicsQueue, 1, &submitInfo, ctx.inFlightFence->getHandle());

            // 呈现
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;

            VkSwapchainKHR swapChains[] = { mWindowContext->getSwapChain()->getHandle() };
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &mImageIndex;

            VkQueue presentQueue = mVulkanCore->getPresentQueue();
            VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                onSwapchainRecreated();
            }
            else if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to present swap chain image!");
            }

            mFrameInProgress = false;
            mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }

        void setResourceManager(IResourceManager* manager) override {
            mResourceManager = manager;
        }

        void onSwapchainRecreated() override {
            vkDeviceWaitIdle(mVulkanCore->getLogicalDeviceHandle());
            mWindowContext->recreateSwapchain();

            if (mResourceManager) {
                mResourceManager->onSwapchainRecreated(mWindowContext.get());
            }
        }

        uint32_t getCurrentFrameIndex() const override {
            return mCurrentFrame;
        }

        bool isFrameInProgress() const override {
            return mFrameInProgress;
        }

    private:
        bool createSyncObjects() {
            mFrameContexts.resize(MAX_FRAMES_IN_FLIGHT);

            for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                auto& frame = mFrameContexts[i];

                frame.imageAvailableSemaphore = Semaphore::create(mVulkanCore->getLogicalDevice());
                frame.renderFinishedSemaphore = Semaphore::create(mVulkanCore->getLogicalDevice());
                frame.inFlightFence = Fence::create(mVulkanCore->getLogicalDevice(), true);
                frame.mainCommandBuffer = CommandBuffer::create(
                    mVulkanCore->getLogicalDevice(),
                    mWindowContext->getCommandPool()
                );

                if (!frame.imageAvailableSemaphore || !frame.renderFinishedSemaphore ||
                    !frame.inFlightFence || !frame.mainCommandBuffer) {
                    return false;
                }
            }

            return true;
        }

        void cleanupSyncObjects() {
            for (auto& frame : mFrameContexts) {
                frame.imageAvailableSemaphore.reset();
                frame.renderFinishedSemaphore.reset();
                frame.inFlightFence.reset();
                frame.mainCommandBuffer.reset();
            }
            mFrameContexts.clear();
        }

    private:
        VulkanCore::Ptr mVulkanCore;
        WindowContext::Ptr mWindowContext;
        IResourceManager* mResourceManager = nullptr;

        std::vector<FrameContext> mFrameContexts;
        FrameContext* mCurrentFrameContext = nullptr;
        uint32_t mCurrentFrame = 0;
        uint32_t mImageIndex = 0;
        bool mFrameInProgress = false;
    };

} // namespace StarryEngine