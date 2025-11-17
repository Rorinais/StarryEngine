#include"Buffer.hpp"
namespace StarryEngine {

    Buffer::Ptr Buffer::create(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool) {
        return std::make_shared<Buffer>(logicalDevice, commandPool);
    }

    void Buffer::createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties
    ) {
        cleanup();

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(mLogicalDevice->getHandle(), &bufferInfo, nullptr, &mBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer");
        }

        // 分配内存
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(mLogicalDevice->getHandle(), mBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(mLogicalDevice->getHandle(), &allocInfo, nullptr, &mBufferMemory) != VK_SUCCESS) {
            vkDestroyBuffer(mLogicalDevice->getHandle(), mBuffer, nullptr);
            throw std::runtime_error("Failed to allocate buffer memory");
        }

        vkBindBufferMemory(mLogicalDevice->getHandle(), mBuffer, mBufferMemory, 0);
        mBufferSize = size;
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
    void Buffer::uploadData(const void* data, VkDeviceSize size, VkBufferUsageFlags usage) {
        // 创建暂存缓冲区
        Buffer stagingBuffer(mLogicalDevice, mCommandPool);
        stagingBuffer.createBuffer(
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        // 映射并拷贝数据
        void* stagingMapped = stagingBuffer.map();
        memcpy(stagingMapped, data, size);
        stagingBuffer.unmap();

        // 创建目标缓冲区
        createBuffer(
            size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        // 执行拷贝命令
        copyBuffer(stagingBuffer.getBuffer(), mBuffer, size);
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

    void* Buffer::map() {
        if (!mMapped) {
            VkResult result = vkMapMemory(
                mLogicalDevice->getHandle(),
                mBufferMemory,
                0,
                mBufferSize,
                0,
                &mMapped
            );
            if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to map buffer memory");
            }
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