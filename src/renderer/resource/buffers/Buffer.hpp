#pragma once
#include "../../../base.hpp"
#include "../../../renderer/backends/vulkan/vulkanCore/VulkanCore.hpp"
#include "../../../renderer/backends/vulkan/renderContext/CommandPool.hpp"
#include <stdexcept>
#include <cstring>
#include <memory>
#include <vector>

// 前置声明VMA
struct VmaAllocator_T;
typedef VmaAllocator_T* VmaAllocator;
struct VmaAllocation_T;
typedef VmaAllocation_T* VmaAllocation;

namespace StarryEngine {

    class Buffer {
    public:
        using Ptr = std::shared_ptr<Buffer>;

        // 创建方法（保持接口不变）
        static Ptr create(const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool,
            VkDeviceSize size = 0,
            VkBufferUsageFlags usage = 0,
            VkMemoryPropertyFlags properties = 0,
            const void* initialData = nullptr);

        Buffer(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool);
        virtual ~Buffer() { cleanup(); }

        // 访问器（完全不变）
        const VkBuffer& getBuffer() const noexcept { return mBuffer; }
        const VkDeviceMemory& getMemory() const noexcept { return mBufferMemory; }
        VkDeviceSize getSize() const noexcept { return mBufferSize; }
        VkBufferUsageFlags getUsage() const noexcept { return mUsage; }
        VkMemoryPropertyFlags getProperties() const noexcept { return mProperties; }

        // 核心功能（接口不变）
        virtual void cleanup() noexcept;
        void* map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
        void unmap();
        void uploadData(const void* data, VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        void updateData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
        void createBuffer(VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties,
            const void* initialData = nullptr);

        // 新增：静态方法设置VMA分配器（由VulkanBackend调用）
        static void SetVMAAllocator(VmaAllocator allocator);

    protected:
        LogicalDevice::Ptr mLogicalDevice;
        CommandPool::Ptr mCommandPool;

        VkBuffer mBuffer = VK_NULL_HANDLE;
        VkDeviceMemory mBufferMemory = VK_NULL_HANDLE;
        VkDeviceSize mBufferSize = 0;
        VkBufferUsageFlags mUsage = 0;
        VkMemoryPropertyFlags mProperties = 0;
        void* mMapped = nullptr;

        // VMA相关成员
        VmaAllocation mVmaAllocation = VK_NULL_HANDLE;

        // 静态VMA分配器
        static VmaAllocator sVMAAllocator;

        // 辅助方法
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

        // 内部VMA创建方法
        bool createBufferWithVMA(VkDeviceSize size,
                                VkBufferUsageFlags usage,
                                VkMemoryPropertyFlags properties,
                                const void* initialData = nullptr);

    private:
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
    };
}