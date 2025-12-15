#include "UniformBuffer.hpp"

namespace StarryEngine {

    UniformBuffer::Ptr UniformBuffer::createAligned(const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool,
        VkDeviceSize size,
        VkDeviceSize minAlignment,
        const void* initialData) {
        // 计算对齐后的大小
        VkDeviceSize alignedSize = (size + minAlignment - 1) & ~(minAlignment - 1);
        auto buffer = std::make_shared<UniformBuffer>(logicalDevice, commandPool,
            alignedSize, initialData);
        buffer->mAlignedSize = alignedSize;
        return buffer;
    }

    UniformBuffer::UniformBuffer(const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool,
        VkDeviceSize size,
        const void* initialData)
        : Buffer(logicalDevice, commandPool) {

        createBuffer(
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            initialData
        );
    }

    VkDescriptorBufferInfo UniformBuffer::getDescriptorInfo(VkDeviceSize offset,
        VkDeviceSize range) const {
        return VkDescriptorBufferInfo{
            .buffer = mBuffer,
            .offset = offset,
            .range = (range == VK_WHOLE_SIZE) ? mBufferSize : range
        };
    }
}