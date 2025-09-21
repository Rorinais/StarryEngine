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

    void CommandBuffer::beginRenderPass(const VkRenderPassBeginInfo& renderPassBeginInfo, const VkSubpassContents& subpassContents) {
        vkCmdBeginRenderPass(mCommandBuffer, &renderPassBeginInfo, subpassContents);
    }

    void CommandBuffer::bindGraphicPipeline(const VkPipeline& pipeline) {
        vkCmdBindPipeline(mCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }

    void CommandBuffer::bindDescriptorSets(const VkPipelineLayout& pipelineLayout, uint32_t firstSet, const std::vector<VkDescriptorSet>& descriptorSets) {
        vkCmdBindDescriptorSets(
            mCommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            firstSet,
            static_cast<uint32_t>(descriptorSets.size()),
            descriptorSets.data(),
            0, nullptr
        );
    }

    void CommandBuffer::bindDescriptorSets(const VkPipelineLayout& pipelineLayout, uint32_t firstSet, VkDescriptorSet descriptorSets) {
        vkCmdBindDescriptorSets(
            mCommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            firstSet,
            1,
            &descriptorSets,
            0, nullptr
        );
    }


    void CommandBuffer::setViewport(const VkExtent2D& extent, bool isOpenglCoord) {
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = isOpenglCoord
            ? static_cast<float>(extent.height) : 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = isOpenglCoord
            ? -static_cast<float>(extent.height)
            : static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(mCommandBuffer, 0, 1, &viewport);
    }

    void CommandBuffer::setScissor(const VkExtent2D& extent) {
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = extent;
        vkCmdSetScissor(mCommandBuffer, 0, 1, &scissor);
    }

    void CommandBuffer::bindVertexBuffers(const std::vector<VkBuffer>& vertexBuffers) {
        std::vector<VkDeviceSize> offsets(vertexBuffers.size(), 0);
        vkCmdBindVertexBuffers(
            mCommandBuffer,
            0,
            static_cast<uint32_t>(vertexBuffers.size()),
            vertexBuffers.data(),
            offsets.data()
        );
    }

    void CommandBuffer::bindIndexBuffer(const VkBuffer& indexBuffers) {
        vkCmdBindIndexBuffer(mCommandBuffer, indexBuffers, 0, VK_INDEX_TYPE_UINT32);
    }

    void CommandBuffer::drawIndexed(const uint32_t indexCount) {
        vkCmdDrawIndexed(mCommandBuffer, indexCount, 1, 0, 0, 0);
    }

    void CommandBuffer::draw(size_t vertexCount) {
        vkCmdDraw(mCommandBuffer, vertexCount, 1, 0, 0);
    }

    void CommandBuffer::endRenderPass() {
        vkCmdEndRenderPass(mCommandBuffer);
    }

    void CommandBuffer::end() {
        if (vkEndCommandBuffer(mCommandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }

    // CommandBuffer.cpp
    void CommandBuffer::executeCommands(const std::vector<VkCommandBuffer>& commandBuffers) {
        vkCmdExecuteCommands(mCommandBuffer, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    }

    bool CommandBuffer::isRecording() const {

        return true; // 简化示例
    }
}