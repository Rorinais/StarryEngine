#include"UniformBuffer.hpp"
namespace StarryEngine {
    UniformBuffer::Ptr UniformBuffer::create(
        const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool,
        VkDeviceSize size
    ) {
        return std::make_shared<UniformBuffer>(logicalDevice, commandPool, size);
    }

    UniformBuffer::UniformBuffer(
        const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool,
        VkDeviceSize size
    ) : Buffer(logicalDevice, commandPool) {
        createBuffer(
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
    }

    void UniformBuffer::uploadData(const void* data, VkDeviceSize size, VkDeviceSize offset) {
        void* mapped = map();
        memcpy(static_cast<char*>(mapped) + offset, data, size);
        unmap();
    }
}