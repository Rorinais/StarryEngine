#pragma once
#include "VulkanCore/VulkanCore.hpp"
#include "WindowContext/WindowContext.hpp"
#include "RenderContext/RenderContext.hpp"


namespace StarryEngine {

    class SimpleVulkanBackend {
    public:
        SimpleVulkanBackend() = default;

        bool initialize(VulkanCore::Ptr core, WindowContext::Ptr window) ;

        void shutdown() ;

        void beginFrame() ;

        VkCommandBuffer getCommandBuffer() ;

        void submitFrame() ;

        void onSwapchainRecreated() ;

		uint32_t getCurrentFrameIndex() const { return mCurrentFrame; }

        uint32_t getCurrentImageIndex() const { return mImageIndex; }

		FrameContext* getCurrentFrameContext() const { return mCurrentFrameContext; }

		bool isFrameInProgress() const { return mFrameInProgress; }

    private:
        bool createSyncObjects();

        void cleanupSyncObjects();

    private:
        VulkanCore::Ptr mVulkanCore;
        WindowContext::Ptr mWindowContext;

        std::vector<VkFence> mImagesInFlight;
        std::vector<FrameContext> mFrameContexts;
        FrameContext* mCurrentFrameContext = nullptr;
        uint32_t mCurrentFrame = 0;
        uint32_t mImageIndex = 0;
        bool mFrameInProgress = false;
    };

} // namespace StarryEngine