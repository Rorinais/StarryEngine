#include "RenderPassSystem.hpp"
#include "./RenderContext.hpp"
#include "../VulkanCore/VulkanCore.hpp"
#include "../../../renderer/pipeline/pipeline.hpp"
#include "PipelineBuilder.hpp"
#include <iostream>

namespace StarryEngine {
    void RenderPass::createPipeline(VkRenderPass renderPass,VkDevice device) {
        if (mPipelineCreationCallback && !mPipeline) {
            PipelineBuilder builder(renderPass, device);
            mPipeline = mPipelineCreationCallback(builder);
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
        dumpDebugInfo();

        return createVulkanRenderPass() && createFramebuffer();
    }

    void RenderPass::execute(CommandBuffer* cmdBuffer, RenderContext& context) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = mRenderPass;
        renderPassInfo.framebuffer = mFramebuffers[context.getFrameIndex()];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = mPipeline->getViewportExtent();

        renderPassInfo.clearValueCount = static_cast<uint32_t>(mClearValues.size());
        renderPassInfo.pClearValues = mClearValues.data();

        context.beginRenderPass(&renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        context.bindGraphicsPipeline(mPipeline->getHandle());

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(mPipeline->getViewportExtent().width);
        viewport.height = static_cast<float>(mPipeline->getViewportExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        context.setViewport(viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = mPipeline->getViewportExtent();
        context.setScissor(scissor);

        if (mExecuteCallback) {
            mExecuteCallback(cmdBuffer, context);
        }
        else {
            std::cout << "[RenderPass] WARNING: No execution callback set for " << mName << std::endl;
        }

		context.endRenderPass();
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