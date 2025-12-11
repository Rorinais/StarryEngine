#pragma once
#include "../../../base.hpp"
#include "../../../renderer/backends/vulkan/vulkanCore/VulkanCore.hpp"
#include "../../../renderer/backends/vulkan/renderContext/CommandPool.hpp"
#include <stdexcept>
#include <cstring>
#include <memory>
#include <vector>

namespace StarryEngine {

    class Buffer {
    public:
        using Ptr = std::shared_ptr<Buffer>;

        // 创建方法（使用工厂模式）
        static Ptr create(const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool,
            VkDeviceSize size = 0,
            VkBufferUsageFlags usage = 0,
            VkMemoryPropertyFlags properties = 0,
            const void* initialData = nullptr);

        Buffer(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool);

        virtual ~Buffer() { cleanup(); }

        // 访问器
        const VkBuffer& getBuffer() const noexcept { return mBuffer; }
        const VkDeviceMemory& getMemory() const noexcept { return mBufferMemory; }
        VkDeviceSize getSize() const noexcept { return mBufferSize; }
        VkBufferUsageFlags getUsage() const noexcept { return mUsage; }
        VkMemoryPropertyFlags getProperties() const noexcept { return mProperties; }

        // 核心功能
        virtual void cleanup() noexcept;

        // 内存映射
        void* map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
        void unmap();

        // 数据上传（会重新创建缓冲区）
        void uploadData(const void* data, VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // 部分更新（需要主机可见内存）
        void updateData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

        // 创建指定属性的缓冲区
        void createBuffer(VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties,
            const void* initialData = nullptr);

    protected:
        LogicalDevice::Ptr mLogicalDevice;
        CommandPool::Ptr mCommandPool;

        VkBuffer mBuffer = VK_NULL_HANDLE;
        VkDeviceMemory mBufferMemory = VK_NULL_HANDLE;
        VkDeviceSize mBufferSize = 0;
        VkBufferUsageFlags mUsage = 0;
        VkMemoryPropertyFlags mProperties = 0;
        void* mMapped = nullptr;

        // 辅助方法
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    private:
        // 私有拷贝构造函数和赋值操作符
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
    };
}