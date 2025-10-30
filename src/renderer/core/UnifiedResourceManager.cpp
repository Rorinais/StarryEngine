#include "IResourceManager.h"
#include "VulkanCore/VulkanCore.hpp"
#include "WindowContext/WindowContext.hpp"

namespace StarryEngine {

    class UnifiedResourceManager : public IResourceManager {
    public:
        UnifiedResourceManager() = default;

        bool initialize(VulkanCore::Ptr core) override {
            mVulkanCore = core;
            mAllocator = core->getAllocator();

            // 初始化资源注册表
            mResourceRegistry = std::make_unique<ResourceRegistry>(
                core->getLogicalDeviceHandle(),
                mAllocator
            );

            return mResourceRegistry != nullptr;
        }

        void shutdown() override {
            if (mResourceRegistry) {
                mResourceRegistry->destroyActualResources();
            }
            mResourceRegistry.reset();
        }

        ResourceHandle createTexture(const TextureDesc& desc) override {
            if (!mResourceRegistry) return ResourceHandle();

            ResourceDescription resourceDesc;
            resourceDesc.type = ResourceType::Texture;
            resourceDesc.format = desc.format;
            resourceDesc.width = desc.width;
            resourceDesc.height = desc.height;
            resourceDesc.depth = 1;
            resourceDesc.mipLevels = desc.mipLevels;
            resourceDesc.arrayLayers = desc.arrayLayers;
            resourceDesc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
            resourceDesc.usage = desc.usage;
            resourceDesc.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;

            return mResourceRegistry->createVirtualResource("Texture", resourceDesc);
        }

        ResourceHandle createBuffer(const BufferDesc& desc) override {
            if (!mResourceRegistry) return ResourceHandle();

            ResourceDescription resourceDesc;
            resourceDesc.type = ResourceType::Buffer;
            resourceDesc.size = desc.size;
            resourceDesc.usage = desc.usage;
            resourceDesc.memoryUsage = desc.memoryUsage;

            return mResourceRegistry->createVirtualResource("Buffer", resourceDesc);
        }

        ResourceHandle getSwapchainResource() override {
            // 返回交换链资源的特殊句柄
            // 这里需要根据你的设计来实现
            return mSwapchainHandle;
        }

        VkImage getImage(ResourceHandle handle) const override {
            if (!mResourceRegistry) return VK_NULL_HANDLE;

            const auto& actualResource = mResourceRegistry->getActualResource(handle, 0);

            VkImage image = VK_NULL_HANDLE;
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, ActualResource::Image>) {
                    image = arg.image;
                }
                }, actualResource.actualResource);

            return image;
        }

        VkBuffer getBuffer(ResourceHandle handle) const override {
            if (!mResourceRegistry) return VK_NULL_HANDLE;

            const auto& actualResource = mResourceRegistry->getActualResource(handle, 0);

            VkBuffer buffer = VK_NULL_HANDLE;
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, ActualResource::Buffer>) {
                    buffer = arg.buffer;
                }
                }, actualResource.actualResource);

            return buffer;
        }

        VkImageView getImageView(ResourceHandle handle) const override {
            if (!mResourceRegistry) return VK_NULL_HANDLE;

            const auto& actualResource = mResourceRegistry->getActualResource(handle, 0);

            VkImageView imageView = VK_NULL_HANDLE;
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, ActualResource::Image>) {
                    imageView = arg.defaultView;
                }
                }, actualResource.actualResource);

            return imageView;
        }

        void onSwapchainRecreated(WindowContext* window) override {
            // 处理交换链重建事件
            // 这里需要更新交换链资源
            if (mResourceRegistry) {
                // 重新分配实际资源
                mResourceRegistry->allocateActualResources(MAX_FRAMES_IN_FLIGHT);
            }
        }

    private:
        VulkanCore::Ptr mVulkanCore;
        VmaAllocator mAllocator = VK_NULL_HANDLE;
        std::unique_ptr<ResourceRegistry> mResourceRegistry;
        ResourceHandle mSwapchainHandle;
    };

} // namespace StarryEngine