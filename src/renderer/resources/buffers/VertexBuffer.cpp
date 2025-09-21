#include"VertexBuffer.hpp"
namespace StarryEngine {

    VertexBuffer::Ptr VertexBuffer::create(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool) {
        return std::make_shared<VertexBuffer>(logicalDevice, commandPool);
    }

    VertexBuffer::VertexBuffer(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool)
        : Buffer(logicalDevice, commandPool) {
    }

    void VertexBuffer::uploadData(const void* data, VkDeviceSize size) {
        Buffer::uploadData(data, size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }
}
