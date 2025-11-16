#include "../interface/IVulkanBackend.hpp"
#include "VulkanCore/VulkanCore.hpp"
#include "WindowContext/WindowContext.hpp"
#include "FrameContext/FrameContext.hpp"


namespace StarryEngine {

    class SimpleVulkanBackend : public IVulkanBackend {
    public:
        SimpleVulkanBackend() = default;

        bool initialize(VulkanCore::Ptr core, WindowContext::Ptr window) override;

        void shutdown() override;

        void beginFrame() override;

        VkCommandBuffer getCommandBuffer() override;

        void submitFrame() override;

        void onSwapchainRecreated(class IResourceManager* manager=nullptr) override;

        uint32_t getCurrentFrameIndex() const override;

        bool isFrameInProgress() const override;

        VkDevice getDevice() const {
            return mVulkanCore->getLogicalDeviceHandle();
        }

    private:
        bool createSyncObjects();

        void cleanupSyncObjects();

    private:
        VulkanCore::Ptr mVulkanCore;
        WindowContext::Ptr mWindowContext;

        std::vector<FrameContext> mFrameContexts;
        FrameContext* mCurrentFrameContext = nullptr;
        uint32_t mCurrentFrame = 0;
        uint32_t mImageIndex = 0;
        bool mFrameInProgress = false;
    };

} // namespace StarryEngine