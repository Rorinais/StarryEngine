#include "VertexBuffer.hpp"

namespace StarryEngine {

    VertexBuffer::Ptr VertexBuffer::create(const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool) {
        return std::make_shared<VertexBuffer>(logicalDevice, commandPool);
    }

    VertexBuffer::VertexBuffer(const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool)
        : Buffer(logicalDevice, commandPool) {
    }

    void VertexBuffer::uploadData(const void* data, VkDeviceSize size) {
        // 保存顶点信息以便后续使用
        if (mVertexSize > 0) {
            mVertexCount = static_cast<uint32_t>(size / mVertexSize);
        }

        // 使用基类的 uploadData 方法，指定顶点缓冲区用途
        // 修改此行：在 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT 后添加 TRANSFER_DST_BIT
        Buffer::uploadData(data, size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // 关键修改
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
}