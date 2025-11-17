#include"commandBuffer.hpp"
namespace StarryEngine {
    CommandBuffer::CommandBuffer(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool, bool asSecondary
    ) :mLogicalDevice(logicalDevice), mCommandPool(commandPool) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandBufferCount = 1;
        allocInfo.commandPool = mCommandPool->getHandle();
        allocInfo.level = asSecondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;


        if (vkAllocateCommandBuffers(mLogicalDevice->getHandle(), &allocInfo, &mCommandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers!");
        }
    }
    CommandBuffer::~CommandBuffer() {
        if (mCommandBuffer != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(mLogicalDevice->getHandle(), mCommandPool->getHandle(), 1, &mCommandBuffer);
        }
    }

    void CommandBuffer::reset(VkCommandBufferResetFlags flags) {
        if (vkResetCommandBuffer(mCommandBuffer, flags) != VK_SUCCESS) {
            throw std::runtime_error("Failed to reset command buffer!");
        }
    }

    void CommandBuffer::begin(VkCommandBufferUsageFlags flag, const VkCommandBufferInheritanceInfo& inheritance) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = flag;
        beginInfo.pInheritanceInfo = &inheritance;

        if (vkBeginCommandBuffer(mCommandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }
    }

    void CommandBuffer::end() {
        if (vkEndCommandBuffer(mCommandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }

}