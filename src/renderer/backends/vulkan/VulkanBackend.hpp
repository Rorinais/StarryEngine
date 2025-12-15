#pragma once
#include "VulkanCore/VulkanCore.hpp"
#include "WindowContext/WindowContext.hpp"
#include "RenderContext/RenderContext.hpp"
#include "../../interface/IBackend.hpp"

namespace StarryEngine {

    class VulkanBackend: public IBackend {
    public:
        VulkanBackend() = default;
        using Ptr = std::shared_ptr<VulkanBackend>;
        static Ptr create() { return std::make_shared<VulkanBackend>(); }

        bool initialize(VulkanCore::Ptr core, WindowContext::Ptr window) override;

        void shutdown() override;

        void beginFrame() override;

        VkCommandBuffer getCommandBuffer() override;

        void submitFrame() override;

        void onSwapchainRecreated() override;

        uint32_t getCurrentFrameIndex() const override { return mCurrentFrame; }

        uint32_t getCurrentImageIndex() const override { return mImageIndex; }

        FrameContext* getCurrentFrameContext() const { return mCurrentFrameContext; }

        VulkanCore::Ptr getVulkanCore() {return mVulkanCore;}

        WindowContext::Ptr getWindowContext() {return mWindowContext;}

        bool isFrameInProgress() const override { return mFrameInProgress; }

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