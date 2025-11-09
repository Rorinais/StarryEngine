#include "RenderPassSystem.hpp"
#include <iostream>

namespace StarryEngine {
    void RenderPass::createPipeline(VkRenderPass renderPass, const LogicalDevice::Ptr& logicalDevice) {
        if (mPipelineCreationCallback && !mPipeline) {
            PipelineBuilder builder(renderPass, logicalDevice);
            mPipeline = mPipelineCreationCallback(builder);

            if (mPipeline) {
                std::cout << "[RenderPass] Created pipeline for " << mName << std::endl;
            }
            else {
                std::cout << "[RenderPass] WARNING: Failed to create pipeline for " << mName << std::endl;
            }
        }
    }


    void RenderPass::declareRead(ResourceHandle resource, VkPipelineStageFlags stages) {
        ResourceUsage usage;
        usage.resource = resource;
        usage.stageFlags = stages;
        usage.accessFlags = VK_ACCESS_SHADER_READ_BIT;
        usage.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        usage.isWrite = false;
        mResourceUsages.push_back(usage);

        std::cout << "[RenderPass] " << mName << " declares READ of resource " << resource.getId()
            << " at stage " << stages << std::endl;
    }

    void RenderPass::declareWrite(ResourceHandle resource, VkPipelineStageFlags stages) {
        ResourceUsage usage;
        usage.resource = resource;
        usage.stageFlags = stages;
        usage.accessFlags = VK_ACCESS_SHADER_WRITE_BIT;
        usage.layout = VK_IMAGE_LAYOUT_GENERAL;
        usage.isWrite = true;
        mResourceUsages.push_back(usage);

        std::cout << "[RenderPass] " << mName << " declares WRITE of resource " << resource.getId()
            << " at stage " << stages << std::endl;
    }

    void RenderPass::declareReadWrite(ResourceHandle resource, VkPipelineStageFlags stages) {
        ResourceUsage usage;
        usage.resource = resource;
        usage.stageFlags = stages;
        usage.accessFlags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        usage.layout = VK_IMAGE_LAYOUT_GENERAL;
        usage.isWrite = true;
        mResourceUsages.push_back(usage);

        std::cout << "[RenderPass] " << mName << " declares READ-WRITE of resource " << resource.getId()
            << " at stage " << stages << std::endl;
    }

    void RenderPass::declareRead(ResourceHandle resource, uint32_t binding, VkDescriptorType type, VkPipelineStageFlags stages) {
        ResourceUsage usage;
        usage.resource = resource;
        usage.stageFlags = stages;
        usage.accessFlags = VK_ACCESS_SHADER_READ_BIT;
        usage.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        usage.isWrite = false;
        usage.binding = binding;
        usage.descriptorType = type;
        mResourceUsages.push_back(usage);

        std::cout << "[RenderPass] " << mName << " declares READ of resource " << resource.getId()
            << " at binding " << binding << " with type " << type
            << " at stage " << stages << std::endl;
    }

    bool RenderPass::compile() {
        std::cout << "[RenderPass] Compiling " << mName << " with " << mResourceUsages.size()
            << " resource usages" << std::endl;

        // 调试输出资源使用情况
        dumpDebugInfo();

        return createVulkanRenderPass() && createFramebuffer();
    }

    void RenderPass::execute(CommandBuffer* cmdBuffer, RenderContext& context) {
        std::cout << "[RenderPass] Executing " << mName << std::endl;

        if (mExecuteCallback) {
            mExecuteCallback(cmdBuffer, context);
        }
        else {
            std::cout << "[RenderPass] WARNING: No execution callback set for " << mName << std::endl;
        }
    }

    void RenderPass::dumpDebugInfo() const {
        std::cout << "=== RenderPass Debug Info: " << mName << " ===" << std::endl;
        std::cout << "Index: " << mIndex << std::endl;
        std::cout << "Resource Usages: " << mResourceUsages.size() << std::endl;

        for (const auto& usage : mResourceUsages) {
            std::cout << "  - Resource " << usage.resource.getId()
                << " Stage: " << usage.stageFlags
                << " Access: " << usage.accessFlags
                << " Layout: " << usage.layout
                << " IsWrite: " << usage.isWrite << std::endl;
        }

        std::cout << "Has Execute Callback: " << (mExecuteCallback ? "Yes" : "No") << std::endl;
        std::cout << "=================================" << std::endl;
    }

    bool RenderPass::createVulkanRenderPass() {
        // 简化的实现，实际中需要根据资源使用创建Vulkan渲染通道
        std::cout << "[RenderPass] Creating Vulkan render pass for " << mName << std::endl;
        return true;
    }

    bool RenderPass::createFramebuffer() {
        // 简化的实现，实际中需要根据资源创建帧缓冲区
        std::cout << "[RenderPass] Creating framebuffer for " << mName << std::endl;
        return true;
    }

} // namespace StarryEngine