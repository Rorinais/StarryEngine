#pragma once
#include "../../../base.hpp"
#include "../../../renderer/backends/vulkan/vulkanCore/VulkanCore.hpp"
#include "../../../renderer/backends/vulkan/renderContext/CommandPool.hpp"
#include <stdexcept>
#include <cstring>
namespace StarryEngine {

    class Buffer {
    public:
        using Ptr = std::shared_ptr<Buffer>;
        static Ptr create(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool);

        Buffer(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool)
            : mLogicalDevice(logicalDevice), mCommandPool(commandPool) {
        }

        virtual ~Buffer() { cleanup(); }

        const VkBuffer& getBuffer() const noexcept { return mBuffer; }
        const VkDeviceMemory& getMemory() const noexcept { return mBufferMemory; }
        VkDeviceSize getSize() const noexcept { return mBufferSize; }

        virtual void cleanup() noexcept;
        void* map();
        void unmap();
        void uploadData(const void* data, VkDeviceSize size, VkBufferUsageFlags usage);

        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
        void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    protected:
        LogicalDevice::Ptr mLogicalDevice;
        CommandPool::Ptr mCommandPool;

        VkBuffer mBuffer = VK_NULL_HANDLE;
        VkDeviceMemory mBufferMemory = VK_NULL_HANDLE;
        VkDeviceSize mBufferSize = 0;
        void* mMapped = nullptr;
    };
}