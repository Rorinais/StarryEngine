#include "UnifiedResourceManager.hpp"

namespace StarryEngine {
        bool UnifiedResourceManager::initialize(VulkanCore::Ptr core)  {
            mVulkanCore = core;
            mAllocator = core->getAllocator();

            // 初始化资源注册表
            mResourceRegistry = std::make_unique<ResourceRegistry>(
                core->getLogicalDeviceHandle(),
                mAllocator
            );

            return mResourceRegistry != nullptr;
        }

        void UnifiedResourceManager::shutdown() {
            if (mResourceRegistry) {
                mResourceRegistry->destroyActualResources();
            }
            mResourceRegistry.reset();
        }

        ResourceHandle UnifiedResourceManager::createTexture(const std::string& name, const TextureDesc& desc) {
            if (!mResourceRegistry) return ResourceHandle();

            ResourceDescription resourceDesc;
            resourceDesc.format = desc.format;
            resourceDesc.extent = { desc.width, desc.height, 1 };
            resourceDesc.width = desc.width;
            resourceDesc.height = desc.height;
            resourceDesc.depth = 1;
            resourceDesc.mipLevels = desc.mipLevels;
            resourceDesc.arrayLayers = desc.arrayLayers;
            resourceDesc.samples = VK_SAMPLE_COUNT_1_BIT;
            resourceDesc.imageUsage = desc.usage;
            resourceDesc.usage = desc.usage;  // 设置兼容字段
            resourceDesc.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
            resourceDesc.isAttachment = (desc.aspect & (VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;

            return mResourceRegistry->createVirtualResource(name, resourceDesc);
        }

        ResourceHandle UnifiedResourceManager::createBuffer(const std::string& name, const BufferDesc& desc) {
            if (!mResourceRegistry) return ResourceHandle();

            ResourceDescription resourceDesc;
            resourceDesc.size = desc.size;
            resourceDesc.bufferUsage = desc.usage;
            resourceDesc.memoryUsage = desc.memoryUsage;

            // 设置内存属性（根据 VMA 使用情况）
            switch (desc.memoryUsage) {
            case VMA_MEMORY_USAGE_GPU_ONLY:
                resourceDesc.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                break;
            case VMA_MEMORY_USAGE_CPU_ONLY:
                resourceDesc.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                break;
            case VMA_MEMORY_USAGE_CPU_TO_GPU:
                resourceDesc.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                break;
            case VMA_MEMORY_USAGE_GPU_TO_CPU:
                resourceDesc.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                break;
            default:
                resourceDesc.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            }

            return mResourceRegistry->createVirtualResource(name, resourceDesc);
        }

        ResourceHandle UnifiedResourceManager::getSwapchainResource() {
            // 返回交换链资源的特殊句柄
            // 这里需要根据你的设计来实现
            return mSwapchainHandle;
        }

        VkImage UnifiedResourceManager::getImage(ResourceHandle handle) const  {
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

        VkBuffer UnifiedResourceManager::getBuffer(ResourceHandle handle) const {
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

        VkImageView UnifiedResourceManager::getImageView(ResourceHandle handle) const {
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

        void* UnifiedResourceManager::getBufferMappedPointer(ResourceHandle handle) const {
            if (!mResourceRegistry) return nullptr;

            const auto& actualResource = mResourceRegistry->getActualResource(handle, 0);
            return actualResource.allocationInfo.pMappedData;
        }

        void UnifiedResourceManager::onSwapchainRecreated(WindowContext* window) {
            // 处理交换链重建事件
            // 这里需要更新交换链资源
            if (mResourceRegistry) {
                // 重新分配实际资源
                mResourceRegistry->allocateActualResources(MAX_FRAMES_IN_FLIGHT);
            }
        }

} // namespace StarryEngine