#include"IndexBuffer.hpp"
namespace StarryEngine {
    IndexBuffer::Ptr IndexBuffer::create(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool) {
        return std::make_shared<IndexBuffer>(logicalDevice, commandPool);
    }

    IndexBuffer::IndexBuffer(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool)
        : Buffer(logicalDevice, commandPool), mIndexCount(0) {
    }

    void IndexBuffer::loadData(const std::vector<uint32_t>& indices) {
        if (indices.empty()) {
            throw std::runtime_error("Index data is empty!");
        }

        const VkDeviceSize dataSize = sizeof(uint32_t) * indices.size();
        mIndexCount = static_cast<uint32_t>(indices.size());

        Buffer::uploadData(
            indices.data(),
            dataSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        );
    }
}