#include "RenderContext.hpp"
#include <stdexcept>

namespace StarryEngine {

    RenderContext::RenderContext(std::shared_ptr<LogicalDevice> device, VkCommandBuffer cmd, uint32_t frameIndex)
        : mDevice(device), mCommandBuffer(cmd), mFrameIndex(frameIndex) {
    }

    // ==================== 渲染通道管理 ====================

    void RenderContext::beginRenderPass(const VkRenderPassBeginInfo* renderPassBeginInfo, VkSubpassContents subpassContents) {
        if (!renderPassBeginInfo) {
            throw std::invalid_argument("RenderPassBeginInfo cannot be null");
        }
        vkCmdBeginRenderPass(mCommandBuffer, renderPassBeginInfo, subpassContents);
        mInRenderPass = true;
    }

    void RenderContext::endRenderPass() {
        if (!mInRenderPass) {
            throw std::runtime_error("Cannot end render pass: no render pass is active");
        }
        vkCmdEndRenderPass(mCommandBuffer);
        mInRenderPass = false;
    }

    // ==================== 管线状态管理 ====================

    void RenderContext::bindGraphicsPipeline(VkPipeline pipeline) {
        if (pipeline == VK_NULL_HANDLE) {
            throw std::invalid_argument("Graphics pipeline cannot be null");
        }
        vkCmdBindPipeline(mCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        mBoundPipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    }

    void RenderContext::bindComputePipeline(VkPipeline pipeline) {
        if (pipeline == VK_NULL_HANDLE) {
            throw std::invalid_argument("Compute pipeline cannot be null");
        }
        vkCmdBindPipeline(mCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        mBoundPipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    }

    void RenderContext::setViewport(const VkViewport& viewport) {
        vkCmdSetViewport(mCommandBuffer, 0, 1, &viewport);
    }

    void RenderContext::setScissor(const VkRect2D& scissor) {
        vkCmdSetScissor(mCommandBuffer, 0, 1, &scissor);
    }

    void RenderContext::setBlendConstants(const float constants[4]) {
        vkCmdSetBlendConstants(mCommandBuffer, constants);
    }

    // ==================== 资源绑定 ====================

    void RenderContext::bindVertexBuffer(VkBuffer buffer, uint32_t binding, VkDeviceSize offset) {
        if (buffer == VK_NULL_HANDLE) {
            throw std::invalid_argument("Vertex buffer cannot be null");
        }
        vkCmdBindVertexBuffers(mCommandBuffer, binding, 1, &buffer, &offset);
    }

    void RenderContext::bindVertexBuffers(const std::vector<VkBuffer>& buffers) {
        if (buffers.empty()) {
            return;
        }

        // 检查是否有null buffer
        for (const auto& buffer : buffers) {
            if (buffer == VK_NULL_HANDLE) {
                throw std::invalid_argument("Vertex buffer in vector cannot be null");
            }
        }

        std::vector<VkDeviceSize> offsets(buffers.size(), 0);
        vkCmdBindVertexBuffers(mCommandBuffer, 0, static_cast<uint32_t>(buffers.size()),
            buffers.data(), offsets.data());
    }

    void RenderContext::bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
        if (buffer == VK_NULL_HANDLE) {
            throw std::invalid_argument("Index buffer cannot be null");
        }
        vkCmdBindIndexBuffer(mCommandBuffer, buffer, offset, indexType);
    }

    void RenderContext::bindDescriptorSet(VkPipelineBindPoint bindPoint, VkDescriptorSet descriptorSet,
        uint32_t firstSet, VkPipelineLayout layout) {
        if (descriptorSet == VK_NULL_HANDLE) {
            throw std::invalid_argument("Descriptor set cannot be null");
        }
        if (layout == VK_NULL_HANDLE) {
            throw std::invalid_argument("Pipeline layout cannot be null");
        }

        vkCmdBindDescriptorSets(mCommandBuffer, bindPoint, layout, firstSet, 1, &descriptorSet, 0, nullptr);
    }

    void RenderContext::bindDescriptorSets(VkPipelineBindPoint bindPoint, VkPipelineLayout layout,
        uint32_t firstSet, const std::vector<VkDescriptorSet>& descriptorSets) {
        if (layout == VK_NULL_HANDLE) {
            throw std::invalid_argument("Pipeline layout cannot be null");
        }
        if (descriptorSets.empty()) {
            return;
        }

        // 检查是否有null descriptor set
        for (const auto& set : descriptorSets) {
            if (set == VK_NULL_HANDLE) {
                throw std::invalid_argument("Descriptor set in vector cannot be null");
            }
        }

        vkCmdBindDescriptorSets(mCommandBuffer, bindPoint, layout, firstSet,
            static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(),
            0, nullptr);
    }

    // ==================== 绘制和分发命令 ====================

    void RenderContext::draw(uint32_t vertexCount, uint32_t instanceCount,
        uint32_t firstVertex, uint32_t firstInstance) {
        if (vertexCount == 0) {
            throw std::invalid_argument("Vertex count cannot be zero");
        }
        if (instanceCount == 0) {
            throw std::invalid_argument("Instance count cannot be zero");
        }

        vkCmdDraw(mCommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void RenderContext::drawIndexed(uint32_t indexCount, uint32_t instanceCount,
        uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
        if (indexCount == 0) {
            throw std::invalid_argument("Index count cannot be zero");
        }
        if (instanceCount == 0) {
            throw std::invalid_argument("Instance count cannot be zero");
        }

        vkCmdDrawIndexed(mCommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void RenderContext::drawIndirect(VkBuffer buffer, VkDeviceSize offset,
        uint32_t drawCount, uint32_t stride) {
        if (buffer == VK_NULL_HANDLE) {
            throw std::invalid_argument("Indirect buffer cannot be null");
        }
        if (drawCount == 0) {
            throw std::invalid_argument("Draw count cannot be zero");
        }

        vkCmdDrawIndirect(mCommandBuffer, buffer, offset, drawCount, stride);
    }

    void RenderContext::drawIndexedIndirect(VkBuffer buffer, VkDeviceSize offset,
        uint32_t drawCount, uint32_t stride) {
        if (buffer == VK_NULL_HANDLE) {
            throw std::invalid_argument("Indirect buffer cannot be null");
        }
        if (drawCount == 0) {
            throw std::invalid_argument("Draw count cannot be zero");
        }

        vkCmdDrawIndexedIndirect(mCommandBuffer, buffer, offset, drawCount, stride);
    }

    void RenderContext::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
        if (groupCountX == 0 || groupCountY == 0 || groupCountZ == 0) {
            throw std::invalid_argument("Dispatch group counts cannot be zero");
        }

        vkCmdDispatch(mCommandBuffer, groupCountX, groupCountY, groupCountZ);
    }

    void RenderContext::dispatchIndirect(VkBuffer buffer, VkDeviceSize offset) {
        if (buffer == VK_NULL_HANDLE) {
            throw std::invalid_argument("Indirect buffer cannot be null");
        }

        vkCmdDispatchIndirect(mCommandBuffer, buffer, offset);
    }

} // namespace StarryEngine