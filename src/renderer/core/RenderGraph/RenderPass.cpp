#include"RenderPass.hpp"

namespace StarryEngine
{
    // 资源使用声明
    void RenderPass::reads(ResourceHandle resource, VkPipelineStageFlags stages) {
		PassResourceUsage usage;
		usage.resource = resource;
		usage.stageFlags = stages;
        usage.accessFlags = VK_ACCESS_SHADER_READ_BIT;
		usage.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // TODO: 需要根据资源类型和具体使用情况设置
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
        usage.accessFlags = VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
        usage.layout = VK_IMAGE_LAYOUT_GENERAL;
        usage.isWrite = true;
        mResourceUsages.push_back(usage);
    }

    // 带描述符绑定的资源使用
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
		return createVulkanRenderPass()&&createFramebuffer();
    }

    void RenderPass::execute(CommandBuffer::Ptr cmdBuffer, RenderContext& context) {
        if(mExecuteCallback) {
            mExecuteCallback(cmdBuffer, context);
		} 
    }

    bool RenderPass::createVulkanRenderPass() {

		return true;
    }

    bool RenderPass::createFramebuffer() {
		return true;
    }
}