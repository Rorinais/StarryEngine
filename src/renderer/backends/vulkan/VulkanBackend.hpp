#pragma once
#include <vk_mem_alloc.h>
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

        // 新增：获取VMA分配器
        VmaAllocator getAllocator() const { return mVmaAllocator; }

    private:
        bool createSyncObjects();
        void cleanupSyncObjects();

        // VMA相关
        bool initializeVMA();
        void cleanupVMA();

    private:
        VulkanCore::Ptr mVulkanCore;
        WindowContext::Ptr mWindowContext;

        std::vector<VkFence> mImagesInFlight;
        std::vector<FrameContext> mFrameContexts;
        FrameContext* mCurrentFrameContext = nullptr;
        uint32_t mCurrentFrame = 0;
        uint32_t mImageIndex = 0;
        bool mFrameInProgress = false;

        // VMA分配器
        VmaAllocator mVmaAllocator = VK_NULL_HANDLE;
    };

} // namespace StarryEngine