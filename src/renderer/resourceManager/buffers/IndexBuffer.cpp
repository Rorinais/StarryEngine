#include "IndexBuffer.hpp"
#include <type_traits>

namespace StarryEngine {

    IndexBuffer::Ptr IndexBuffer::create(const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool) {
        return std::make_shared<IndexBuffer>(logicalDevice, commandPool);
    }

    IndexBuffer::IndexBuffer(const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool)
        : Buffer(logicalDevice, commandPool) {
    }

    void IndexBuffer::loadData(const std::vector<uint16_t>& indices) {
        mIndexCount = static_cast<uint32_t>(indices.size());
        mIndexType = IndexType::UINT16;

        Buffer::uploadData(indices.data(),
            indices.size() * sizeof(uint16_t),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); // 关键修改
    }

    void IndexBuffer::loadData(const std::vector<uint32_t>& indices) {
        mIndexCount = static_cast<uint32_t>(indices.size());
        mIndexType = IndexType::UINT32;

        Buffer::uploadData(indices.data(),
            indices.size() * sizeof(uint32_t),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); // 关键修改
    }

    void IndexBuffer::bind(VkCommandBuffer commandBuffer) const {
        vkCmdBindIndexBuffer(commandBuffer, getBuffer(), 0, getVkIndexType());
    }

    // 模板函数的实现（必须在头文件中）
    // template<typename T>
    // void IndexBuffer::loadData(const std::vector<T>& indices) {
    //     if constexpr (std::is_same_v<T, uint16_t>) {
    //         loadData(indices);
    //     } else if constexpr (std::is_same_v<T, uint32_t>) {
    //         loadData(indices);
    //     } else {
    //         static_assert(sizeof(T) == 0, "Unsupported index type");
    //     }
    // }
}