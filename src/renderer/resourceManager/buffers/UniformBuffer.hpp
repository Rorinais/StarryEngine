#pragma once
#include "Buffer.hpp"
namespace StarryEngine {
    class UniformBuffer : public Buffer {
    public:
        using Ptr = std::shared_ptr<UniformBuffer>;
        static Ptr create(
            const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool,
            VkDeviceSize size
        );

        UniformBuffer(
            const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool,
            VkDeviceSize size
        );

        void uploadData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

        void loadData(const std::vector<uint8_t>& data) {
            uploadData(data.data(), data.size());
        }
    };
}
