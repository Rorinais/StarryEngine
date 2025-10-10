#include "RenderPassSystem.hpp"

namespace StarryEngine {

    void RenderPass::reads(ResourceHandle resource, VkPipelineStageFlags stages) {
        PassResourceUsage usage;
        usage.resource = resource;
        usage.stageFlags = stages;
        usage.accessFlags = VK_ACCESS_SHADER_READ_BIT;
        usage.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        usage.isWrite = false;
        mResourceUsages.push_back(usage);
    }

    void RenderPass::writes(ResourceHandle resource, VkPipelineStageFlags stages) {
        PassResourceUsage usage;
        usage.resource = resource;
        usage.stageFlags = stages;
        usage.accessFlags = VK_ACCESS_SHADER_WRITE_BIT;
        usage.layout = VK_IMAGE_LAYOUT_GENERAL;
        usage.isWrite = true;
        mResourceUsages.push_back(usage);
    }

    void RenderPass::readWrite(ResourceHandle resource, VkPipelineStageFlags stages) {
        PassResourceUsage usage;
        usage.resource = resource;
        usage.stageFlags = stages;
        usage.accessFlags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        usage.layout = VK_IMAGE_LAYOUT_GENERAL;
        usage.isWrite = true;
        mResourceUsages.push_back(usage);
    }

    void RenderPass::reads(ResourceHandle resource, uint32_t binding, VkDescriptorType type, VkPipelineStageFlags stages) {
        PassResourceUsage usage;
        usage.resource = resource;
        usage.stageFlags = stages;
        usage.accessFlags = VK_ACCESS_SHADER_READ_BIT;
        usage.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        usage.isWrite = false;
        usage.binding = binding;
        usage.descriptorType = type;
        mResourceUsages.push_back(usage);
    }

    bool RenderPass::compile() {
        return createVulkanRenderPass() && createFramebuffer();
    }

    void RenderPass::execute(CommandBuffer* cmdBuffer, RenderContext& context) {
        if (mExecuteCallback) {
            mExecuteCallback(cmdBuffer, context);
        }
    }

    bool RenderPass::createVulkanRenderPass() {
        // 简化的实现，实际中需要根据资源使用创建Vulkan渲染通道
        return true;
    }

    bool RenderPass::createFramebuffer() {
        // 简化的实现，实际中需要根据资源创建帧缓冲区
        return true;
    }

} // namespace StarryEngine