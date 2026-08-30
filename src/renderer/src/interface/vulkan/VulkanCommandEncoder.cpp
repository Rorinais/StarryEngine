#include <interface/vulkan/VulkanResources.hpp>
#include <interface/vulkan/VulkanCommandEncoder.hpp>


namespace StarryEngine::RHI {
    void VulkanCommandEncoder::setViewport(const Viewport& viewport) {
        VkViewport vkViewport{};
        vkViewport.x = viewport.x;
        vkViewport.y = viewport.y;
        vkViewport.width = viewport.width;
        vkViewport.height = viewport.height;
        vkViewport.minDepth = viewport.minDepth;
        vkViewport.maxDepth = viewport.maxDepth;
        vkCmdSetViewport(getVkCommandBuffer(), 0, 1, &vkViewport);
    }

    void VulkanCommandEncoder::setViewports(const std::vector<Viewport>& viewports) {
        std::vector<VkViewport> vkViewports;
        for (const auto& vp : viewports) {
            VkViewport vkVp{};
            vkVp.x = vp.x;
            vkVp.y = vp.y;
            vkVp.width = vp.width;
            vkVp.height = vp.height;
            vkVp.minDepth = vp.minDepth;
            vkVp.maxDepth = vp.maxDepth;
            vkViewports.push_back(vkVp);
        }
        vkCmdSetViewport(getVkCommandBuffer(), 0, static_cast<uint32_t>(vkViewports.size()), vkViewports.data());
    }

    void VulkanCommandEncoder::setScissor(const Rect2D& scissor) {
        VkRect2D vkScissor{};
        vkScissor.offset.x = scissor.offset.x;
        vkScissor.offset.y = scissor.offset.y;
        vkScissor.extent.width = scissor.extent.width;
        vkScissor.extent.height = scissor.extent.height;
        vkCmdSetScissor(getVkCommandBuffer(), 0, 1, &vkScissor);
    }

    void VulkanCommandEncoder::setScissors(const std::vector<Rect2D>& scissors) {
        std::vector<VkRect2D> vkScissors;
        for (const auto& sc : scissors) {
            VkRect2D vkSc{};
            vkSc.offset.x = sc.offset.x;
            vkSc.offset.y = sc.offset.y;
            vkSc.extent.width = sc.extent.width;
            vkSc.extent.height = sc.extent.height;
            vkScissors.push_back(vkSc);
        }
        vkCmdSetScissor(getVkCommandBuffer(), 0, static_cast<uint32_t>(vkScissors.size()), vkScissors.data());
    }

    void VulkanCommandEncoder::setLineWidth(float width) {
        vkCmdSetLineWidth(getVkCommandBuffer(), width);
    }

    void VulkanCommandEncoder::setDepthBias(float constantFactor, float clamp, float slopeFactor) {
        vkCmdSetDepthBias(getVkCommandBuffer(), constantFactor, clamp, slopeFactor);
    }

    void VulkanCommandEncoder::setBlendConstants(const float constants[4]) {
        vkCmdSetBlendConstants(getVkCommandBuffer(), constants);
    }

    void VulkanCommandEncoder::setDepthBounds(float minDepth, float maxDepth) {
        vkCmdSetDepthBounds(getVkCommandBuffer(), minDepth, maxDepth);
    }

    void VulkanCommandEncoder::setStencilCompareMask(StencilFace face, uint32_t compareMask) {
        VkStencilFaceFlags vkFaceFlags = 0;
        switch (face) {
        case StencilFace::Front:
            vkFaceFlags = VK_STENCIL_FACE_FRONT_BIT;
            break;
        case StencilFace::Back:
            vkFaceFlags = VK_STENCIL_FACE_BACK_BIT;
            break;
        case StencilFace::FrontAndBack:
            vkFaceFlags = VK_STENCIL_FACE_FRONT_AND_BACK;
            break;
        default:
            throw std::runtime_error("Invalid stencil face specified");
        }

        vkCmdSetStencilCompareMask(getVkCommandBuffer(), vkFaceFlags, compareMask);
    }

    void VulkanCommandEncoder::setStencilWriteMask(StencilFace face, uint32_t writeMask) {
        VkStencilFaceFlags vkFaceFlags = 0;
        switch (face) {
        case StencilFace::Front:
            vkFaceFlags = VK_STENCIL_FACE_FRONT_BIT;
            break;
        case StencilFace::Back:
            vkFaceFlags = VK_STENCIL_FACE_BACK_BIT;
            break;
        case StencilFace::FrontAndBack:
            vkFaceFlags = VK_STENCIL_FACE_FRONT_AND_BACK;
            break;
        default:
            throw std::runtime_error("Invalid stencil face specified");
        }
        vkCmdSetStencilWriteMask(getVkCommandBuffer(), vkFaceFlags, writeMask);
    }

    void VulkanCommandEncoder::setStencilReference(StencilFace face, uint32_t reference) {
        VkStencilFaceFlags vkFaceFlags = 0;
        switch (face) {
        case StencilFace::Front:
            vkFaceFlags = VK_STENCIL_FACE_FRONT_BIT;
            break;
        case StencilFace::Back:
            vkFaceFlags = VK_STENCIL_FACE_BACK_BIT;
            break;
        case StencilFace::FrontAndBack:
            vkFaceFlags = VK_STENCIL_FACE_FRONT_AND_BACK;
            break;
        default:
            throw std::runtime_error("Invalid stencil face specified");
        }
        vkCmdSetStencilReference(getVkCommandBuffer(), vkFaceFlags, reference);
    }

    // 管线绑定
    void VulkanCommandEncoder::bindGraphicPipeline(PipelineHandle pipeline) {
        auto* rhiPipeline = mResourceManager->getPipeline(pipeline);
        auto vkPipeline = static_cast<VkPipeline>(rhiPipeline->getNativeHandle());
        vkCmdBindPipeline(getVkCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);
    }

    void VulkanCommandEncoder::bindComputePipeline(PipelineHandle pipeline) {
        auto* rhiPipeline = mResourceManager->getPipeline(pipeline);
        auto vkPipeline = static_cast<VkPipeline>(rhiPipeline->getNativeHandle());
        vkCmdBindPipeline(getVkCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline);
    }

    void VulkanCommandEncoder::bindVertexBuffers(
        uint32_t firstBinding,
        const std::vector<BufferHandle>& buffers,
        const std::vector<uint64_t>& offsets) {
        std::vector<VkBuffer> vkBuffers;
        for (const auto& buf : buffers) {
            auto* rhiBuf = mResourceManager->getBuffer(buf);
            vkBuffers.push_back(static_cast<VkBuffer>(rhiBuf->getNativeHandle()));
        }
        vkCmdBindVertexBuffers(getVkCommandBuffer(), firstBinding, static_cast<uint32_t>(vkBuffers.size()), vkBuffers.data(), offsets.data());
    }

    void VulkanCommandEncoder::bindIndexBuffer(
        BufferHandle buffer,
        uint64_t offset,
        IndexType indexType) {
        auto* rhiBuf = mResourceManager->getBuffer(buffer);
        VkBuffer vkBuffer = static_cast<VkBuffer>(rhiBuf->getNativeHandle());
        VkIndexType vkIndexType = (indexType == IndexType::UInt16) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
        vkCmdBindIndexBuffer(getVkCommandBuffer(), vkBuffer, offset, vkIndexType);
    }

    void VulkanCommandEncoder::bindDescriptorSets(
        PipelineBindPoint bindPoint,
        PipelineLayoutHandle layout,
        uint32_t firstSet,
        const std::vector<DescriptorSetHandle>& descriptorSets,
        const std::vector<uint32_t>& dynamicOffsets) {

        if (!layout || descriptorSets.empty()) return;

        auto* rhiLayout = mResourceManager->getPipelineLayout(layout);
        auto vkLayout = static_cast<VkPipelineLayout>(rhiLayout->getNativeHandle());
        std::vector<VkDescriptorSet> vkSets;
        for (auto handle : descriptorSets) {
            auto set = mResourceManager->getDescriptorSet(handle);
            if (set) {
                vkSets.push_back(static_cast<VkDescriptorSet>(set->getNativeHandle()));
            }
        }
        if (vkSets.empty()) return;

        VkPipelineBindPoint vkBindPoint = (bindPoint == PipelineBindPoint::Graphics)
            ? VK_PIPELINE_BIND_POINT_GRAPHICS
            : VK_PIPELINE_BIND_POINT_COMPUTE;

        vkCmdBindDescriptorSets(
            getVkCommandBuffer(),
            vkBindPoint,
            vkLayout,
            firstSet,
            static_cast<uint32_t>(vkSets.size()),
            vkSets.data(),
            static_cast<uint32_t>(dynamicOffsets.size()),
            dynamicOffsets.data()
        );
    }

    // 推送常量
    void VulkanCommandEncoder::pushConstants(
        PipelineLayoutHandle layout,
        ShaderStage stage,
        uint32_t offset,
        uint32_t size,
        const void* values) {
        auto* rhiLayout = static_cast<VulkanPipelineLayout*>(mResourceManager->getPipelineLayout(layout));
        auto vkLayout = static_cast<VkPipelineLayout>(rhiLayout->getNativeHandle());
        vkCmdPushConstants(getVkCommandBuffer(), vkLayout, static_cast<VkShaderStageFlags>(stage), offset, size, values);
    }

    // 绘图命令
    void VulkanCommandEncoder::draw(
        uint32_t vertexCount,
        uint32_t instanceCount,
        uint32_t firstVertex,
        uint32_t firstInstance) {
		vkCmdDraw(getVkCommandBuffer(), vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void VulkanCommandEncoder::drawIndexed(
        uint32_t indexCount,
        uint32_t instanceCount,
        uint32_t firstIndex,
        int32_t vertexOff,
        uint32_t firstInstance) {
		vkCmdDrawIndexed(getVkCommandBuffer(), indexCount, instanceCount, firstIndex, vertexOff, firstInstance);
    }

    void VulkanCommandEncoder::drawIndirect(
        BufferHandle buffer,
        uint64_t offset,
        uint32_t drawCount,
        uint32_t stride) {
       
    }

    void VulkanCommandEncoder::drawIndexedIndirect(
        BufferHandle buffer,
        uint64_t offset,
        uint32_t drawCount,
        uint32_t stride) {
    }

    void VulkanCommandEncoder::drawIndirectCount(
        BufferHandle buffer,
        uint64_t offset,
        BufferHandle countBuffer,
        uint64_t countOffset,
        uint32_t maxDrawCount,
        uint32_t stride) {
    }

    void VulkanCommandEncoder::drawIndexedIndirectCount(
        BufferHandle buffer,
        uint64_t offset,
        BufferHandle countBuffer,
        uint64_t countOffset,
        uint32_t maxDrawCount,
        uint32_t stride) {
    }

    // 计算命令
    void VulkanCommandEncoder::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
        vkCmdDispatch(getVkCommandBuffer(), groupCountX, groupCountY, groupCountZ);
    }

    void VulkanCommandEncoder::dispatchIndirect(
        BufferHandle buffer,
        uint64_t offset) {
    }

    // 光线追踪命令
    void VulkanCommandEncoder::traceRays(
        BufferHandle raygenTable,
        BufferHandle missTable,
        BufferHandle hitTable,
        BufferHandle callableTable,
        uint32_t width,
        uint32_t height,
        uint32_t depth) {
    }

    void VulkanCommandEncoder::beginRenderPass(
        const RenderPassBeginInfo& beginInfo,
        SubpassContents contents) {

        std::vector<VkClearValue> vkClearValues;
        vkClearValues.reserve(beginInfo.clearValues.size());

        for (size_t i = 0; i < beginInfo.clearValues.size(); ++i) {
            const auto& cv = beginInfo.clearValues[i];
            VkClearValue vkCv{};

            if (cv.isDepth) {
                vkCv.depthStencil.depth = cv.depth;
                vkCv.depthStencil.stencil = cv.stencil;
            }else {
                vkCv.color.float32[0] = cv.color.r;
                vkCv.color.float32[1] = cv.color.g;
                vkCv.color.float32[2] = cv.color.b;
                vkCv.color.float32[3] = cv.color.a;
            }

            vkClearValues.push_back(vkCv);
        }

        VkRenderPassBeginInfo vkBeginInfo{};
        vkBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        vkBeginInfo.renderPass = static_cast<VkRenderPass>(
            mResourceManager->getRenderPass(beginInfo.renderPass)->getNativeHandle());
        vkBeginInfo.framebuffer = static_cast<VkFramebuffer>(
            mResourceManager->getFramebuffer(beginInfo.framebuffer)->getNativeHandle());
        vkBeginInfo.renderArea.offset.x = beginInfo.renderArea.offset.x;
        vkBeginInfo.renderArea.offset.y = beginInfo.renderArea.offset.y;
        vkBeginInfo.renderArea.extent.width = beginInfo.renderArea.extent.width;
        vkBeginInfo.renderArea.extent.height = beginInfo.renderArea.extent.height;
        vkBeginInfo.clearValueCount = static_cast<uint32_t>(vkClearValues.size());
        vkBeginInfo.pClearValues = vkClearValues.data();

        VkSubpassContents vkContents = (contents == SubpassContents::Inline)
            ? VK_SUBPASS_CONTENTS_INLINE
            : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;

        vkCmdBeginRenderPass(getVkCommandBuffer(), &vkBeginInfo, vkContents);
    }

    void VulkanCommandEncoder::nextSubpass(SubpassContents contents) {
		vkCmdNextSubpass(getVkCommandBuffer(), (contents == SubpassContents::Inline) ? VK_SUBPASS_CONTENTS_INLINE : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    }
    void VulkanCommandEncoder::endRenderPass() {
		vkCmdEndRenderPass(getVkCommandBuffer());
    }

    // ── 动态渲染（VK_KHR_dynamic_rendering）：vkCmdBeginRendering / vkCmdEndRendering ──
    void VulkanCommandEncoder::beginRendering(const RenderingInfo& renderingInfo) {
        VkCommandBuffer cmdBuf = getVkCommandBuffer();
        if (cmdBuf == VK_NULL_HANDLE) return;

        VkRenderingInfo vkInfo{};
        vkInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        vkInfo.renderArea.offset.x = renderingInfo.renderArea.offset.x;
        vkInfo.renderArea.offset.y = renderingInfo.renderArea.offset.y;
        vkInfo.renderArea.extent.width = renderingInfo.renderArea.extent.width;
        vkInfo.renderArea.extent.height = renderingInfo.renderArea.extent.height;
        vkInfo.layerCount = renderingInfo.layerCount;
        vkInfo.viewMask = renderingInfo.viewMask;

        std::vector<VkRenderingAttachmentInfo> vkColorAttachments;
        vkColorAttachments.reserve(renderingInfo.colorAttachments.size());
        for (const auto& att : renderingInfo.colorAttachments) {
            VkRenderingAttachmentInfo vkAtt{};
            vkAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            vkAtt.imageView = static_cast<VkImageView>(att.imageView);
            vkAtt.imageLayout = func::RHI_TO_VK_ImageLayout(att.imageLayout);
            vkAtt.loadOp = func::RHI_TO_VK_AttachmentLoadOp(att.loadOp);
            vkAtt.storeOp = func::RHI_TO_VK_AttachmentStoreOp(att.storeOp);
            if (att.loadOp == AttachmentLoadOp::Clear) {
                vkAtt.clearValue.color = { att.clearValue.color.r, att.clearValue.color.g,
                                           att.clearValue.color.b, att.clearValue.color.a };
            }
            vkColorAttachments.push_back(vkAtt);
        }
        vkInfo.colorAttachmentCount = static_cast<uint32_t>(vkColorAttachments.size());
        vkInfo.pColorAttachments = vkColorAttachments.data();

        VkRenderingAttachmentInfo vkDepth{};
        vkDepth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        if (renderingInfo.hasDepth && renderingInfo.depthAttachment.imageView) {
            vkDepth.imageView = static_cast<VkImageView>(renderingInfo.depthAttachment.imageView);
            vkDepth.imageLayout = func::RHI_TO_VK_ImageLayout(renderingInfo.depthAttachment.imageLayout);
            vkDepth.loadOp = func::RHI_TO_VK_AttachmentLoadOp(renderingInfo.depthAttachment.loadOp);
            vkDepth.storeOp = func::RHI_TO_VK_AttachmentStoreOp(renderingInfo.depthAttachment.storeOp);
            // Vulkan 1.3：depth/stencil 共享 loadOp/storeOp（stencil 无独立字段）。
            // StencilPass 依赖此语义：首写（Clear）清 0，后续（Load）保留 StencilWrite 写入值。
            if (renderingInfo.depthAttachment.loadOp == AttachmentLoadOp::Clear) {
                vkDepth.clearValue.depthStencil.depth = renderingInfo.depthAttachment.clearValue.depth;
                vkDepth.clearValue.depthStencil.stencil = renderingInfo.depthAttachment.clearValue.stencil;
            }
            vkInfo.pDepthAttachment = &vkDepth;
        }

        vkCmdBeginRendering(cmdBuf, &vkInfo);
    }

    void VulkanCommandEncoder::endRendering() {
        VkCommandBuffer cmdBuf = getVkCommandBuffer();
        if (cmdBuf == VK_NULL_HANDLE) return;
        vkCmdEndRendering(cmdBuf);
    }

    // secondary 命令缓冲开始录制：RENDER_PASS_CONTINUE_BIT + 继承信息（并行命令录制用）
    void VulkanCommandEncoder::beginSecondary(const SecondaryCommandBufferBeginInfo& beginInfo) {
        VkCommandBuffer cmdBuf = getVkCommandBuffer();
        if (cmdBuf == VK_NULL_HANDLE && mCommandBuffer != nullptr) {
            cmdBuf = reinterpret_cast<VkCommandBuffer>(mCommandBuffer->getNativeHandle());
        }
        if (cmdBuf == VK_NULL_HANDLE) return;

        VkCommandBufferBeginInfo vkBeginInfo{};
        vkBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VkCommandBufferInheritanceInfo vkInheritInfo{};
        vkInheritInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;

        // 动态渲染继承（VK_KHR_dynamic_rendering secondary）
        VkCommandBufferInheritanceRenderingInfo vkInheritRendering{};
        if (beginInfo.useDynamicRendering) {
            vkBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
            vkInheritRendering.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO;
            VkFormat colorFmt = func::RHI_TO_VK_Format(beginInfo.colorFormat);
            vkInheritRendering.colorAttachmentCount = (colorFmt != VK_FORMAT_UNDEFINED) ? 1u : 0u;
            vkInheritRendering.pColorAttachmentFormats = (colorFmt != VK_FORMAT_UNDEFINED) ? &colorFmt : nullptr;
            vkInheritRendering.depthAttachmentFormat = func::RHI_TO_VK_Format(beginInfo.depthFormat);
            vkInheritRendering.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
            vkInheritInfo.pNext = &vkInheritRendering;
        }
        else if (beginInfo.renderPassContinue) {
            vkBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
            vkInheritInfo.renderPass = static_cast<VkRenderPass>(
                mResourceManager->getRenderPass(beginInfo.renderPass)->getNativeHandle());
            vkInheritInfo.subpass = beginInfo.subpass;
            vkInheritInfo.framebuffer = static_cast<VkFramebuffer>(
                mResourceManager->getFramebuffer(beginInfo.framebuffer)->getNativeHandle());
        }

        vkBeginInfo.pInheritanceInfo = &vkInheritInfo;

        vkBeginCommandBuffer(cmdBuf, &vkBeginInfo);
    }

    // 执行次命令缓冲区（native 句柄）
    void VulkanCommandEncoder::executeCommands(const std::vector<void*>& nativeCommandBuffers) {
        if (nativeCommandBuffers.empty()) return;

        VkCommandBuffer cmdBuf = getVkCommandBuffer();
        if (cmdBuf == VK_NULL_HANDLE && mCommandBuffer != nullptr) {
            cmdBuf = reinterpret_cast<VkCommandBuffer>(mCommandBuffer->getNativeHandle());
        }
        if (cmdBuf == VK_NULL_HANDLE) return;

        vkCmdExecuteCommands(cmdBuf,
            static_cast<uint32_t>(nativeCommandBuffers.size()),
            reinterpret_cast<const VkCommandBuffer*>(nativeCommandBuffers.data()));
    }

    // 资源屏障
    void VulkanCommandEncoder::pipelineBarrier(
        PipelineStage srcStage,
        PipelineStage dstStage,
        DependencyFlags flags,
        const std::vector<ImageMemoryBarrier>& memoryBarriers,
        const std::vector<BufferBarrier>& bufferBarriers,
        const std::vector<ImageBarrier>& imageBarriers)
    {
        VkCommandBuffer cmdBuf = getVkCommandBuffer();
        if (cmdBuf == VK_NULL_HANDLE) {
            // 如果没有原生命令缓冲区，可能需要从 mCommandBuffer 获取
            if (mCommandBuffer) {
                cmdBuf = static_cast<VkCommandBuffer>(mCommandBuffer->getNativeHandle());
            }
            if (cmdBuf == VK_NULL_HANDLE) {
                // 日志错误或直接返回
                return;
            }
        }

        // 转换阶段掩码和依赖标志
        VkPipelineStageFlags vkSrcStage = static_cast<VkPipelineStageFlags>(srcStage);
        VkPipelineStageFlags vkDstStage = static_cast<VkPipelineStageFlags>(dstStage);
        VkDependencyFlags vkFlags = static_cast<VkDependencyFlags>(flags);

        // 转换 ImageMemoryBarrier（如果使用）
        std::vector<VkMemoryBarrier> vkMemoryBarriers;
        vkMemoryBarriers.reserve(memoryBarriers.size());
        for (const auto& mb : memoryBarriers) {
            VkMemoryBarrier vkMb{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
            vkMb.srcAccessMask = static_cast<VkAccessFlags>(mb.srcAccessMask);
            vkMb.dstAccessMask = static_cast<VkAccessFlags>(mb.dstAccessMask);
            vkMemoryBarriers.push_back(vkMb);
        }

        // 转换 BufferBarrier
        std::vector<VkBufferMemoryBarrier> vkBufferBarriers;
        vkBufferBarriers.reserve(bufferBarriers.size());
        for (const auto& bb : bufferBarriers) {
            VkBufferMemoryBarrier vkBb{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
            vkBb.srcAccessMask = static_cast<VkAccessFlags>(bb.srcAccessMask);
            vkBb.dstAccessMask = static_cast<VkAccessFlags>(bb.dstAccessMask);
            vkBb.srcQueueFamilyIndex = bb.srcQueueFamilyIndex;
            vkBb.dstQueueFamilyIndex = bb.dstQueueFamilyIndex;
            // 需要从 BufferHandle 获取 VkBuffer
            RHIBuffer* bufObj = mResourceManager->getBuffer(bb.buffer);
            if (bufObj) {
                vkBb.buffer = static_cast<VkBuffer>(bufObj->getNativeHandle());
            }
            vkBb.offset = bb.offset;
            vkBb.size = bb.size;
            vkBufferBarriers.push_back(vkBb);
        }

        // 转换 ImageBarrier
        std::vector<VkImageMemoryBarrier> vkImageBarriers;
        vkImageBarriers.reserve(imageBarriers.size());
        for (const auto& ib : imageBarriers) {
            RHITexture* texObj = mResourceManager->getTexture(ib.image);
            if (!texObj) {
                // 跳过无效纹理
                continue;
            }
            VkImage image = static_cast<VkImage>(texObj->getImageHandle());

            VkImageMemoryBarrier vkIb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            vkIb.srcAccessMask = static_cast<VkAccessFlags>(ib.srcAccessMask);
            vkIb.dstAccessMask = static_cast<VkAccessFlags>(ib.dstAccessMask);
            vkIb.oldLayout = static_cast<VkImageLayout>(ib.oldLayout);
            vkIb.newLayout = static_cast<VkImageLayout>(ib.newLayout);
            vkIb.srcQueueFamilyIndex = ib.srcQueueFamilyIndex;
            vkIb.dstQueueFamilyIndex = ib.dstQueueFamilyIndex;
            vkIb.image = image;
            vkIb.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(ib.aspectMask);
            vkIb.subresourceRange.baseMipLevel = ib.baseMipLevel;
            vkIb.subresourceRange.levelCount = ib.levelCount;
            vkIb.subresourceRange.baseArrayLayer = ib.baseArrayLayer;
            vkIb.subresourceRange.layerCount = ib.layerCount;

            vkImageBarriers.push_back(vkIb);
        }

        // 调用 Vulkan API
        vkCmdPipelineBarrier(
            cmdBuf,
            vkSrcStage,
            vkDstStage,
            vkFlags,
            static_cast<uint32_t>(vkMemoryBarriers.size()),
            vkMemoryBarriers.data(),
            static_cast<uint32_t>(vkBufferBarriers.size()),
            vkBufferBarriers.data(),
            static_cast<uint32_t>(vkImageBarriers.size()),
            vkImageBarriers.data()
        );
    }

    // 拷贝操作
    void VulkanCommandEncoder::copyBuffer(
        BufferHandle src, BufferHandle dst,
        const std::vector<BufferCopyRegion>& regions)
    {
        if (!src || !dst) return;

        std::vector<VkBufferCopy> vkRegions;
        for (auto& r : regions) {
            VkBufferCopy copy{};
            copy.srcOffset = r.srcOffset;
            copy.dstOffset = r.dstOffset;
            copy.size = r.size;
            vkRegions.push_back(copy);
        }

        vkCmdCopyBuffer(
            getVkCommandBuffer(),
            static_cast<VkBuffer>(mResourceManager->getBuffer(src)->getNativeHandle()),
            static_cast<VkBuffer>(mResourceManager->getBuffer(dst)->getNativeHandle()),
            static_cast<uint32_t>(vkRegions.size()),
            vkRegions.data()
        );
    }

    void VulkanCommandEncoder::copyImage(
        TextureHandle src,
        TextureHandle dst,
        const std::vector<ImageCopyRegion>& regions) {
    }

    void VulkanCommandEncoder::copyBufferToImage(
        BufferHandle src,
        TextureHandle dst,
        const std::vector<BufferImageCopyRegion>& regions) {
    }

    void VulkanCommandEncoder::copyImageToBuffer(
        TextureHandle src,
        BufferHandle dst,
        const std::vector<BufferImageCopyRegion>& regions)
    {
        if (!src || !dst || regions.empty()) return;

        // 转换源纹理为 VkImage
        auto* vkTexture = mResourceManager->getTexture(src);
        if (!vkTexture) return;
        VkImage srcImage = static_cast<VkImage>(vkTexture->getImageHandle());

        // 转换目标缓冲区为 VkBuffer
        auto* vkBuffer = mResourceManager->getBuffer(dst);
        if (!vkBuffer) return;
        VkBuffer dstBuffer = static_cast<VkBuffer>(vkBuffer->getNativeHandle());

        // 转换区域
        std::vector<VkBufferImageCopy> vkRegions;
        vkRegions.reserve(regions.size());
        for (const auto& region : regions) {
            VkBufferImageCopy vkRegion{};
            vkRegion.bufferOffset = region.bufferOffset;
            vkRegion.bufferRowLength = region.bufferRowLength;
            vkRegion.bufferImageHeight = region.bufferImageHeight;
            vkRegion.imageSubresource.aspectMask = static_cast<VkImageAspectFlags>(region.imageSubresource.aspectMask);
            vkRegion.imageSubresource.mipLevel = region.imageSubresource.baseMipLevel;
            vkRegion.imageSubresource.baseArrayLayer = region.imageSubresource.baseArrayLayer;
            vkRegion.imageSubresource.layerCount = region.imageSubresource.layerCount;
            vkRegion.imageOffset = { region.imageOffset.x, region.imageOffset.y, region.imageOffset.z };
            vkRegion.imageExtent = { region.imageExtent.width, region.imageExtent.height, region.imageExtent.depth };
            vkRegions.push_back(vkRegion);
        }

        vkCmdCopyImageToBuffer(
            getVkCommandBuffer(),
            srcImage,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   
            dstBuffer,
            static_cast<uint32_t>(vkRegions.size()),
            vkRegions.data()
        );
    }

    void VulkanCommandEncoder::blitImage(
        TextureHandle src,
        ImageLayout srcLayout,
        TextureHandle dst,
        ImageLayout dstLayout,
        const std::vector<ImageBlitRegion>& regions,
        Filter filter)
    {
        // 假设 m_handle 是当前录制的 VkCommandBuffer
        VkCommandBuffer cmdBuf = getVkCommandBuffer();  // 根据实际情况获取

        // 获取底层 VkImage 对象（需要 RHITexture 提供相应方法）
        VkImage srcImage = static_cast<VkImage>(mResourceManager->getTexture(src)->getImageHandle());
        VkImage dstImage = static_cast<VkImage>(mResourceManager->getTexture(dst)->getImageHandle());

        // 转换 RHI 布局枚举到 VkImageLayout（假设一一对应或使用转换函数）
        VkImageLayout vkSrcLayout = func::RHI_TO_VK_ImageLayout(srcLayout);
        VkImageLayout vkDstLayout = func::RHI_TO_VK_ImageLayout(dstLayout);

        // 转换过滤器
        VkFilter vkFilter = (filter == Filter::Linear) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

        // 准备 VkImageBlit 数组
        std::vector<VkImageBlit> vkRegions;
        vkRegions.reserve(regions.size());

        for (const auto& reg : regions) {
            // 转换源子资源范围
            VkImageSubresourceLayers srcSubresource = {};
            srcSubresource.aspectMask = func::RHI_TO_VK_ImageAspect(reg.srcSubresource.aspectMask);
            srcSubresource.mipLevel = reg.srcSubresource.baseMipLevel;
            srcSubresource.baseArrayLayer = reg.srcSubresource.baseArrayLayer;
            srcSubresource.layerCount = reg.srcSubresource.layerCount;

            // 转换目标子资源范围
            VkImageSubresourceLayers dstSubresource = {};
            dstSubresource.aspectMask = func::RHI_TO_VK_ImageAspect(reg.dstSubresource.aspectMask);
            dstSubresource.mipLevel = reg.dstSubresource.baseMipLevel;
            dstSubresource.baseArrayLayer = reg.dstSubresource.baseArrayLayer;
            dstSubresource.layerCount = reg.dstSubresource.layerCount;

            // 构建 VkImageBlit
            VkImageBlit blit = {};
            blit.srcSubresource = srcSubresource;
            blit.srcOffsets[0] = { reg.srcOffsets[0].x, reg.srcOffsets[0].y, reg.srcOffsets[0].z };
            blit.srcOffsets[1] = { reg.srcOffsets[1].x, reg.srcOffsets[1].y, reg.srcOffsets[1].z };
            blit.dstSubresource = dstSubresource;
            blit.dstOffsets[0] = { reg.dstOffsets[0].x, reg.dstOffsets[0].y, reg.dstOffsets[0].z };
            blit.dstOffsets[1] = { reg.dstOffsets[1].x, reg.dstOffsets[1].y, reg.dstOffsets[1].z };

            vkRegions.push_back(blit);
        }

        // 记录 vkCmdBlitImage
        vkCmdBlitImage(cmdBuf,
            srcImage, vkSrcLayout,
            dstImage, vkDstLayout,
            static_cast<uint32_t>(vkRegions.size()), vkRegions.data(),
            vkFilter);
    }

    // 清除操作
    void VulkanCommandEncoder::clearColorImage(
        TextureHandle image,
        ImageLayout layout,
        const Color& color,
        const std::vector<ImageSubresourceRange>& ranges) {
    }

    void VulkanCommandEncoder::clearDepthStencilImage(
        TextureHandle image,
        ImageLayout layout,
        float depth,
        uint32_t stencil,
        const std::vector<ImageSubresourceRange>& ranges) {
    }

    void VulkanCommandEncoder::clearAttachments(
        const std::vector<ClearAttachment>& attachments,
        const std::vector<ClearRect>& rects) {
        VkCommandBuffer cmdBuf = getVkCommandBuffer();
        if (cmdBuf == VK_NULL_HANDLE && mCommandBuffer != nullptr) {
            cmdBuf = reinterpret_cast<VkCommandBuffer>(mCommandBuffer->getNativeHandle());
        }
        if (cmdBuf == VK_NULL_HANDLE || attachments.empty() || rects.empty()) return;

        std::vector<VkClearAttachment> vkAttachments;
        vkAttachments.reserve(attachments.size());
        for (const auto& att : attachments) {
            VkClearAttachment vkAtt{};
            vkAtt.aspectMask = func::RHI_TO_VK_ImageAspect(att.aspectMask);
            vkAtt.colorAttachment = att.colorAttachment;
            if (att.aspectMask == ImageAspect::Color) {
                vkAtt.clearValue.color.float32[0] = att.clearValue.color.r;
                vkAtt.clearValue.color.float32[1] = att.clearValue.color.g;
                vkAtt.clearValue.color.float32[2] = att.clearValue.color.b;
                vkAtt.clearValue.color.float32[3] = att.clearValue.color.a;
            } else {
                vkAtt.clearValue.depthStencil.depth = att.clearValue.depth;
                vkAtt.clearValue.depthStencil.stencil = att.clearValue.stencil;
            }
            vkAttachments.push_back(vkAtt);
        }

        std::vector<VkClearRect> vkRects;
        vkRects.reserve(rects.size());
        for (const auto& rect : rects) {
            VkClearRect vkRect{};
            vkRect.rect.offset = { rect.rect.offset.x, rect.rect.offset.y };
            vkRect.rect.extent = { rect.rect.extent.width, rect.rect.extent.height };
            vkRect.baseArrayLayer = rect.baseArrayLayer;
            vkRect.layerCount = rect.layerCount;
            vkRects.push_back(vkRect);
        }

        vkCmdClearAttachments(cmdBuf,
            static_cast<uint32_t>(vkAttachments.size()), vkAttachments.data(),
            static_cast<uint32_t>(vkRects.size()), vkRects.data());
    }

    // 填充缓冲区
    void VulkanCommandEncoder::fillBuffer(
        BufferHandle buffer,
        uint64_t offset,
        uint64_t size,
        uint32_t data) {
    }

    // 更新缓冲区
    void VulkanCommandEncoder::updateBuffer(
        BufferHandle buffer,
        uint64_t offset,
        uint64_t size,
        const void* data) {
    }

    // 调试标记
    void VulkanCommandEncoder::beginDebugLabel(const char* label, const float color[4]) {}
    void VulkanCommandEncoder::endDebugLabel() {}
    void VulkanCommandEncoder::insertDebugLabel(const char* label, const float color[4]) {}
};