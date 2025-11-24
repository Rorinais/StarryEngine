#pragma once
#include "../renderGraph/RenderGraphTypes.hpp"
#include "../backends/vulkan/vulkanCore/VulkanCore.hpp"
#include <vulkan/vulkan.h>
#include <memory>

namespace StarryEngine {

    struct TextureDesc {
        VkFormat format;
        uint32_t width;
        uint32_t height;
        VkImageUsageFlags usage;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
    };

    struct BufferDesc {
        VkDeviceSize size;
        VkBufferUsageFlags usage;
        VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    };

    class IResourceManager {
    public:
        virtual ~IResourceManager() = default;

        // 初始化
        virtual bool initialize(VulkanCore::Ptr core) = 0;
        virtual void shutdown() = 0;

        // 资源创建
        virtual ResourceHandle createTexture(const std::string& name,const TextureDesc& desc) = 0;
        virtual ResourceHandle createBuffer(const std::string& name,const BufferDesc& desc) = 0;
        virtual ResourceHandle getSwapchainResource() = 0;

        // 资源访问
        virtual VkImage getImage(ResourceHandle handle) const = 0;
        virtual VkBuffer getBuffer(ResourceHandle handle) const = 0;
        virtual VkImageView getImageView(ResourceHandle handle) const = 0;
        virtual void* getBufferMappedPointer(ResourceHandle handle) const = 0;

        // 事件处理
        virtual void onSwapchainRecreated(WindowContext* window) = 0;
    };

} // namespace StarryEngine