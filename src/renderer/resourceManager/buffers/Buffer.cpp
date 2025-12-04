#include"Buffer.hpp"
namespace StarryEngine {

    Buffer::Ptr Buffer::create(const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        const void* initialData) {
        auto buffer = std::make_shared<Buffer>(logicalDevice, commandPool);
        if (size > 0) {
            buffer->createBuffer(size, usage, properties, initialData);
        }
        return buffer;
    }

    Buffer::Buffer(const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool)
        : mLogicalDevice(logicalDevice)
        , mCommandPool(commandPool) {
    }

    void Buffer::createBuffer(VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        const void* initialData) {
        // 清理现有资源
        cleanup();

        mBufferSize = size;
        mUsage = usage;
        mProperties = properties;

        // 创建缓冲区
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(mLogicalDevice->getHandle(), &bufferInfo,
            nullptr, &mBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer");
        }

        // 分配内存
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(mLogicalDevice->getHandle(), mBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(mLogicalDevice->getHandle(), &allocInfo,
            nullptr, &mBufferMemory) != VK_SUCCESS) {
            vkDestroyBuffer(mLogicalDevice->getHandle(), mBuffer, nullptr);
            throw std::runtime_error("Failed to allocate buffer memory");
        }

        vkBindBufferMemory(mLogicalDevice->getHandle(), mBuffer, mBufferMemory, 0);

        // 如果有初始数据，并且内存是主机可见的，直接映射并复制
        if (initialData && (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            void* mapped = nullptr;
            vkMapMemory(mLogicalDevice->getHandle(), mBufferMemory, 0, size, 0, &mapped);
            memcpy(mapped, initialData, size);

            // 如果需要，刷新内存范围（对于非一致性的主机可见内存）
            if (!(properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                VkMappedMemoryRange memoryRange{};
                memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
                memoryRange.memory = mBufferMemory;
                memoryRange.offset = 0;
                memoryRange.size = size;
                vkFlushMappedMemoryRanges(mLogicalDevice->getHandle(), 1, &memoryRange);
            }

            vkUnmapMemory(mLogicalDevice->getHandle(), mBufferMemory);
        }
        // 如果有初始数据，但内存不是主机可见的（如设备本地内存），使用暂存缓冲区
        else if (initialData && !(properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            // 创建主机可见的暂存缓冲区
            Buffer stagingBuffer(mLogicalDevice, mCommandPool);

            // 手动创建暂存缓冲区，避免递归
            VkBuffer stagingBufferHandle;
            VkDeviceMemory stagingBufferMemory;

            VkBufferCreateInfo stagingInfo{};
            stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stagingInfo.size = size;
            stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(mLogicalDevice->getHandle(), &stagingInfo,
                nullptr, &stagingBufferHandle) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create staging buffer");
            }

            VkMemoryRequirements stagingMemReq;
            vkGetBufferMemoryRequirements(mLogicalDevice->getHandle(), stagingBufferHandle, &stagingMemReq);

            VkMemoryAllocateInfo stagingAllocInfo{};
            stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            stagingAllocInfo.allocationSize = stagingMemReq.size;
            stagingAllocInfo.memoryTypeIndex = findMemoryType(stagingMemReq.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            if (vkAllocateMemory(mLogicalDevice->getHandle(), &stagingAllocInfo,
                nullptr, &stagingBufferMemory) != VK_SUCCESS) {
                vkDestroyBuffer(mLogicalDevice->getHandle(), stagingBufferHandle, nullptr);
                throw std::runtime_error("Failed to allocate staging buffer memory");
            }

            vkBindBufferMemory(mLogicalDevice->getHandle(), stagingBufferHandle, stagingBufferMemory, 0);

            // 映射并复制数据到暂存缓冲区
            void* stagingMapped = nullptr;
            vkMapMemory(mLogicalDevice->getHandle(), stagingBufferMemory, 0, size, 0, &stagingMapped);
            memcpy(stagingMapped, initialData, size);
            vkUnmapMemory(mLogicalDevice->getHandle(), stagingBufferMemory);

            // 将数据从暂存缓冲区复制到设备本地缓冲区
            copyBuffer(stagingBufferHandle, mBuffer, size);

            // 清理暂存缓冲区
            vkDestroyBuffer(mLogicalDevice->getHandle(), stagingBufferHandle, nullptr);
            vkFreeMemory(mLogicalDevice->getHandle(), stagingBufferMemory, nullptr);
        }
    }

    // 内存类型查找
    uint32_t Buffer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(mLogicalDevice->getPhysicalDevice()->getHandle(), &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("Failed to find suitable memory type!");
    }

    // 缓冲区拷贝
    void Buffer::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = mCommandPool->getHandle();
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmdBuffer;
        vkAllocateCommandBuffers(mLogicalDevice->getHandle(), &allocInfo, &cmdBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmdBuffer, &beginInfo);
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(cmdBuffer, src, dst, 1, &copyRegion);
        vkEndCommandBuffer(cmdBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        vkQueueSubmit(mLogicalDevice->getQueueHandles().graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(mLogicalDevice->getQueueHandles().graphicsQueue);

        vkFreeCommandBuffers(mLogicalDevice->getHandle(), mCommandPool->getHandle(), 1, &cmdBuffer);
    }
    void Buffer::uploadData(const void* data, VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties) {
        createBuffer(size, usage, properties, data);
    }

    void Buffer::updateData(const void* data, VkDeviceSize size, VkDeviceSize offset) {
        if (!(mProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            throw std::runtime_error("Buffer memory is not host visible");
        }

        void* mapped = map(offset, size);
        memcpy(mapped, data, size);
        unmap();
    }


    void Buffer::cleanup() noexcept {
        if (mBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(mLogicalDevice->getHandle(), mBuffer, nullptr);
            mBuffer = VK_NULL_HANDLE;
        }
        if (mBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(mLogicalDevice->getHandle(), mBufferMemory, nullptr);
            mBufferMemory = VK_NULL_HANDLE;
        }
        mBufferSize = 0;
    }

    void* Buffer::map(VkDeviceSize offset, VkDeviceSize size) {
        if (mMapped) {
            return static_cast<char*>(mMapped) + offset;
        }

        VkDeviceSize mapSize = (size == VK_WHOLE_SIZE) ? mBufferSize : size;
        VkResult result = vkMapMemory(mLogicalDevice->getHandle(), mBufferMemory,
            offset, mapSize, 0, &mMapped);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to map buffer memory");
        }

        return mMapped;
    }

    void Buffer::unmap() {
        if (mMapped) {
            vkUnmapMemory(mLogicalDevice->getHandle(), mBufferMemory);
            mMapped = nullptr;
        }
    }
}