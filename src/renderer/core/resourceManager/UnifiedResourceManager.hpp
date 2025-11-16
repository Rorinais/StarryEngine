#include "IResourceManager.hpp"
#include "VulkanCore/VulkanCore.hpp"
#include "WindowContext/WindowContext.hpp"
#include "RenderGraph/ResourceSystem.hpp"

namespace StarryEngine {

    class UnifiedResourceManager : public IResourceManager {
    public:
        UnifiedResourceManager() = default;

        bool initialize(VulkanCore::Ptr core) override;
        void shutdown() override;

        ResourceHandle createTexture(const std::string& name, const TextureDesc& desc) override;
        ResourceHandle createBuffer(const std::string& name, const BufferDesc& desc) override;

        ResourceHandle getSwapchainResource() override;
        VkImage getImage(ResourceHandle handle) const override;
        VkBuffer getBuffer(ResourceHandle handle) const override;
        VkImageView getImageView(ResourceHandle handle) const override;
        void* getBufferMappedPointer(ResourceHandle handle) const override;

        void onSwapchainRecreated(WindowContext* window) override;

    private:
        VulkanCore::Ptr mVulkanCore;
        VmaAllocator mAllocator = VK_NULL_HANDLE;
        std::unique_ptr<ResourceRegistry> mResourceRegistry;
        ResourceHandle mSwapchainHandle;
    };

} // namespace StarryEngine