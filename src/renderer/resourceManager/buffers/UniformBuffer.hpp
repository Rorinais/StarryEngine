#pragma once
#include "Buffer.hpp"
#include <vector>

namespace StarryEngine {

    class UniformBuffer : public Buffer {
    public:
        using Ptr = std::shared_ptr<UniformBuffer>;

        // 创建UniformBuffer
        static Ptr create(const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool,
            VkDeviceSize size,
            const void* initialData = nullptr);

        // 创建对齐的UniformBuffer（考虑最小对齐要求）
        static Ptr createAligned(const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool,
            VkDeviceSize size,
            VkDeviceSize minAlignment = 256,  // Vulkan通常要求256字节对齐
            const void* initialData = nullptr);

        UniformBuffer(const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool,
            VkDeviceSize size,
            const void* initialData = nullptr);

        // 获取描述符信息
        VkDescriptorBufferInfo getDescriptorInfo(VkDeviceSize offset = 0,
            VkDeviceSize range = VK_WHOLE_SIZE) const;

        // 便捷的模板方法
        template<typename T>
        void upload(const T& data, VkDeviceSize offset = 0) {
            updateData(&data, sizeof(T), offset);
        }

        template<typename T>
        void upload(const std::vector<T>& data, VkDeviceSize offset = 0) {
            updateData(data.data(), data.size() * sizeof(T), offset);
        }

        // 直接映射为指定类型
        template<typename T>
        T* mapAs(VkDeviceSize offset = 0) {
            return reinterpret_cast<T*>(map(offset, sizeof(T)));
        }

    private:
        VkDeviceSize mAlignedSize = 0;
    };
}