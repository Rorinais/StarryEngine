#include "RHI_RESOURCE_FACTORY.hpp"
#include "RHI_VK_RESOURCE.hpp"
#include "RHI_RESOURCE_MANAGER.hpp"
#include <stdexcept>

namespace StarryEngine::RHI {
    std::unique_ptr<RHIBuffer> VKResourceFactory::createBuffer(const BufferDesc& desc) {
		return std::make_unique<RHI_VK_Buffer>(mDevice, desc);
    }

    std::unique_ptr<RHITexture> VKResourceFactory::createTexture(const TextureDesc& desc) {
        return std::make_unique<RHI_VK_Texture>(mDevice, desc);
    }

    std::unique_ptr<RHIPipeline> VKResourceFactory::createPipeline(const GraphicsPipelineDesc& desc) {
        // 检查 PipelineLayout
        auto* layoutObj = mResourceManager->getPipelineLayout(desc.pipelineLayoutHandle);
        if (!layoutObj) {
            std::cerr << "[VKResourceFactory] PipelineLayout is null for handle: " << desc.pipelineLayoutHandle.toString() << std::endl;
            return nullptr;
        }
        auto layout = static_cast<VkPipelineLayout>(layoutObj->getNativeHandle());

        // 检查 VertexShader
        auto* rhiVertexShader = mResourceManager->getShader(desc.vertexShader);
        if (!rhiVertexShader) {
            std::cerr << "[VKResourceFactory] VertexShader is null for handle: " << desc.vertexShader.toString() << std::endl;
            return nullptr;
        }
        auto vertexShader = static_cast<VkShaderModule>(rhiVertexShader->getNativeHandle());

        // 检查 FragmentShader
        auto* rhiFragmentShader = mResourceManager->getShader(desc.fragmentShader);
        if (!rhiFragmentShader) {
            std::cerr << "[VKResourceFactory] FragmentShader is null for handle: " << desc.fragmentShader.toString() << std::endl;
            return nullptr;
        }
        auto fragmentShader = static_cast<VkShaderModule>(rhiFragmentShader->getNativeHandle());

        // 检查 RenderPass
        auto* renderPassObj = mResourceManager->getRenderPass(desc.renderPass);
        if (!renderPassObj) {
            std::cerr << "[VKResourceFactory] RenderPass is null for handle: " << desc.renderPass.toString() << std::endl;
            return nullptr;
        }
        auto renderPass = static_cast<VkRenderPass>(renderPassObj->getNativeHandle());

        std::vector<VkPipelineShaderStageCreateInfo> shaderStage{
            mDevice->createShaderStageInfo(vertexShader, FUNC::RHI_TO_VK_ShaderStageFlag(rhiVertexShader->getStage()), rhiVertexShader->getEntryPoint().c_str()),
            mDevice->createShaderStageInfo(fragmentShader, FUNC::RHI_TO_VK_ShaderStageFlag(rhiFragmentShader->getStage()), rhiFragmentShader->getEntryPoint().c_str())
        };

        return std::make_unique<RHI_VK_Pipeline>(mDevice, desc, shaderStage, layout, renderPass);
    }

    std::unique_ptr<RHIPipeline> VKResourceFactory::createComputePipeline(const ComputePipelineDesc& desc) {
        // TODO: 实现计算管线创建
        throw std::runtime_error("Not implemented: createComputePipeline");
    }

    std::unique_ptr<RHIPipelineLayout> VKResourceFactory::createPipelineLayout(const PipelineLayoutDesc& desc) {
        std::vector<VkDescriptorSetLayout> vkDescSetlayouts{};
        for (auto desSet : desc.descriptorSetLayouts) {
			 auto rhiDesSetlayout = mResourceManager->getDescriptorSetLayout(desSet);
             auto desSetlayout = static_cast<VkDescriptorSetLayout>(rhiDesSetlayout->getNativeHandle());
			 vkDescSetlayouts.push_back(desSetlayout);
        }
        return std::make_unique<RHI_VK_PipelineLayout>(mDevice, desc, vkDescSetlayouts);
    }

    std::unique_ptr<RHIShaderModule> VKResourceFactory::createShader(const ShaderModuleDesc& desc) {
        return std::make_unique<RHI_VK_ShaderModule>(mDevice, desc);
    }

    std::unique_ptr<RHISampler> VKResourceFactory::createSampler(const SamplerDesc& desc) {
        return std::make_unique<RHI_VK_Sampler>(mDevice, desc);
    }

    std::unique_ptr<RHIRenderPass> VKResourceFactory::createRenderPass(const RenderPassDesc& desc) {
		return std::make_unique<RHI_VK_RenderPass>(mDevice, desc);
    }

    std::unique_ptr<RHIFramebuffer> VKResourceFactory::createFramebuffer(const FramebufferDesc& desc) {
		return std::make_unique<RHI_VK_Framebuffer>(mDevice, desc);
    }

    std::unique_ptr<RHIDescriptorSet> VKResourceFactory::createDescriptorSet(const DescriptorSetDesc& desc) {
        if (!mResourceManager) {
            throw std::runtime_error("ResourceManager not set in VKResourceFactory");
        }

        // 1. 获取描述符池
        auto* pool = dynamic_cast<RHI_VK_DescriptorPool*>(mResourceManager->getDescriptorPool(desc.descriptorPool));
        if (!pool) {
            throw std::runtime_error("Invalid descriptor pool handle");
        }

        RHI_VK_DescriptorSetLayout* layout = nullptr;

        // 2. 优先使用显式传入的 descriptorSetLayout
        if (desc.descriptorSetLayout.isValid()) {
            layout = dynamic_cast<RHI_VK_DescriptorSetLayout*>(mResourceManager->getDescriptorSetLayout(desc.descriptorSetLayout));
            if (!layout) {
                throw std::runtime_error("Invalid descriptor set layout handle");
            }
        }
        else {
            // 回退到通过 pipelineLayout 获取
            auto* pipelineLayout = dynamic_cast<RHI_VK_PipelineLayout*>(mResourceManager->getPipelineLayout(desc.pipelineLayout));
            if (!pipelineLayout) {
                throw std::runtime_error("Invalid pipeline layout handle");
            }
            auto layoutHandle = pipelineLayout->getLayoutHandle(desc.setIndex);
            if (!layoutHandle.isValid()) {
                throw std::runtime_error("No descriptor set layout at set index " + std::to_string(desc.setIndex));
            }
            layout = dynamic_cast<RHI_VK_DescriptorSetLayout*>(mResourceManager->getDescriptorSetLayout(layoutHandle));
            if (!layout) {
                throw std::runtime_error("Invalid descriptor set layout handle");
            }
        }

        // 3. 分配描述符集
        auto sets = pool->allocateDescriptorSets({ layout });
        if (sets.empty()) {
            throw std::runtime_error("Failed to allocate descriptor set");
        }

        return std::move(sets[0]);
    }

    std::unique_ptr<RHIDescriptorPool> VKResourceFactory::createDescriptorPool(const DescriptorPoolDesc& desc) {
        // TODO: 实现描述符池创建
        return std::make_unique<RHI_VK_DescriptorPool>(mDevice, desc);
    }

    std::unique_ptr<RHIDescriptorSetLayout> VKResourceFactory::createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) {
        return std::make_unique<RHI_VK_DescriptorSetLayout>(mDevice, desc);
    }

  //  std::unique_ptr<RHICommandBuffer> VKResourceFactory::createCommandBuffer(const CommandBufferDesc& desc) {
  //      auto rhicmdPool = static_cast<RHI_VK_CommandPool*>(mResourceManager->getCommandPool(desc.commandPool));
  //      return std::make_unique<RHI_VK_CommandBuffer>(mDevice, desc, rhicmdPool);
  //  }

  //  std::unique_ptr<RHICommandPool> VKResourceFactory::createCommandPool(const CommandPoolDesc& desc) {
  //      return std::make_unique<RHI_VK_CommandPool>(mDevice, desc);
  //  }

  //  std::unique_ptr<RHIFence> VKResourceFactory::createFence(const FenceDesc& desc) {
  //      // TODO: 实现栅栏创建
  //      throw std::runtime_error("Not implemented: createFence");
  //  }

  //  std::unique_ptr<RHISemaphore> VKResourceFactory::createSemaphore(const SemaphoreDesc& desc) {
  //      // TODO: 实现信号量创建
  //      throw std::runtime_error("Not implemented: createSemaphore");
  //  }

  //  std::unique_ptr<RHIEvent> VKResourceFactory::createEvent(const EventDesc& desc) {
  //      // TODO: 实现事件创建
  //      throw std::runtime_error("Not implemented: createEvent");
  //  }

  //  std::unique_ptr<RHIQueryPool> VKResourceFactory::createQueryPool(const QueryPoolDesc& desc) {
  //      // TODO: 实现查询池创建
  //      throw std::runtime_error("Not implemented: createQueryPool");
  //  }

  //  std::unique_ptr<RHIAccelerationStructure> VKResourceFactory::createAccelerationStructure(const AccelerationStructureDesc& desc) {
  //      // TODO: 实现加速结构创建
  //      throw std::runtime_error("Not implemented: createAccelerationStructure");
  //  }

  //  std::unique_ptr<RHISwapChain> VKResourceFactory::createSwapChain(const SwapChainDesc& desc) {
  //      // TODO: 实现交换链创建
  //      throw std::runtime_error("Not implemented: createSwapChain");
  //  }

  //  std::unique_ptr<RHIQueue> VKResourceFactory::createQueue(const QueueDesc& desc) {
  //      // TODO: 实现队列创建
  //      throw std::runtime_error("Not implemented: createQueue");
  //  }

  //  std::vector<std::unique_ptr<RHICommandBuffer>> VKResourceFactory::createCommandBuffers(
  //      uint32_t count,
  //      const CommandBufferDesc& desc) {
  //      auto rhicmdPool = static_cast<RHI_VK_CommandPool*>(mResourceManager->getCommandPool(desc.commandPool));
  //      std::vector<VkCommandBuffer> cmdBuffers = rhicmdPool->allocateCommandBuffers(count, desc.level);

		//std::vector<std::unique_ptr<RHICommandBuffer>> buffers;
  //      for (auto cmdBuffer: cmdBuffers) {
		//	buffers.push_back(std::make_unique<RHI_VK_CommandBuffer>(mDevice, desc, rhicmdPool));
  //      }
  //      return buffers;
  //  }

    std::vector<std::unique_ptr<RHIDescriptorSet>> VKResourceFactory::createDescriptorSets(
        uint32_t count,
        const DescriptorSetDesc& desc) {

        std::vector<std::unique_ptr<RHIDescriptorSet>> sets;
        sets.reserve(count);

        for (uint32_t i = 0; i < count; ++i) {
            sets.push_back(createDescriptorSet(desc));
        }

        return sets;
    }

    void VKResourceFactory::setResourceManager(ResourceManager* ptr) {
        mResourceManager = ptr;
    }

    // 状态设置
    void RHI_VK_CommandEncoder::setViewport(const Viewport& viewport) {
        VkViewport vkViewport{};
        vkViewport.x = viewport.x;
        vkViewport.y = viewport.y;
        vkViewport.width = viewport.width;
        vkViewport.height = viewport.height;
        vkViewport.minDepth = viewport.minDepth;
        vkViewport.maxDepth = viewport.maxDepth;
        vkCmdSetViewport(getVkCommandBuffer(), 0, 1, &vkViewport);
    }

    void RHI_VK_CommandEncoder::setViewports(const std::vector<Viewport>& viewports) {
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

    void RHI_VK_CommandEncoder::setScissor(const Rect2D& scissor) {
        VkRect2D vkScissor{};
        vkScissor.offset.x = scissor.offset.x;
        vkScissor.offset.y = scissor.offset.y;
        vkScissor.extent.width = scissor.extent.width;
        vkScissor.extent.height = scissor.extent.height;
        vkCmdSetScissor(getVkCommandBuffer(), 0, 1, &vkScissor);
    }

    void RHI_VK_CommandEncoder::setScissors(const std::vector<Rect2D>& scissors) {
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

    void RHI_VK_CommandEncoder::setLineWidth(float width) {
        vkCmdSetLineWidth(getVkCommandBuffer(), width);
    }

    void RHI_VK_CommandEncoder::setDepthBias(float constantFactor, float clamp, float slopeFactor) {
        vkCmdSetDepthBias(getVkCommandBuffer(), constantFactor, clamp, slopeFactor);
    }

    void RHI_VK_CommandEncoder::setBlendConstants(const float constants[4]) {
        vkCmdSetBlendConstants(getVkCommandBuffer(), constants);
    }

    void RHI_VK_CommandEncoder::setDepthBounds(float minDepth, float maxDepth) {
        vkCmdSetDepthBounds(getVkCommandBuffer(), minDepth, maxDepth);
    }

    void RHI_VK_CommandEncoder::setStencilCompareMask(StencilFace face, uint32_t compareMask) {
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

    void RHI_VK_CommandEncoder::setStencilWriteMask(StencilFace face, uint32_t writeMask) {
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

    void RHI_VK_CommandEncoder::setStencilReference(StencilFace face, uint32_t reference) {
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
    void RHI_VK_CommandEncoder::bindPipeline(RHIPipeline* pipeline) {
        auto vkPipeline = static_cast<VkPipeline>(static_cast<RHI_VK_Pipeline*>(pipeline)->getNativeHandle());
        vkCmdBindPipeline(getVkCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);
    }

    void RHI_VK_CommandEncoder::bindVertexBuffers(
        uint32_t firstBinding,
        const std::vector<RHIBuffer*>& buffers,
        const std::vector<uint64_t>& offsets) {
        std::vector<VkBuffer> vkBuffers;
        for (const auto& buf : buffers) {
            vkBuffers.push_back(static_cast<VkBuffer>(static_cast<RHI_VK_Buffer*>(buf)->getNativeHandle()));
        }
        vkCmdBindVertexBuffers(getVkCommandBuffer(), firstBinding, static_cast<uint32_t>(vkBuffers.size()), vkBuffers.data(), offsets.data());
    }

    void RHI_VK_CommandEncoder::bindIndexBuffer(
        RHIBuffer* buffer,
        uint64_t offset,
        IndexType indexType) {
        VkBuffer vkBuffer = static_cast<VkBuffer>(static_cast<RHI_VK_Buffer*>(buffer)->getNativeHandle());
        VkIndexType vkIndexType = (indexType == IndexType::UInt16) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
        vkCmdBindIndexBuffer(getVkCommandBuffer(), vkBuffer, offset, vkIndexType);
    }

    void RHI_VK_CommandEncoder::bindDescriptorSets(
        PipelineBindPoint bindPoint,
        RHIPipelineLayout* layout,
        uint32_t firstSet,
        const std::vector<DescriptorSetHandle>& descriptorSets,
        const std::vector<uint32_t>& dynamicOffsets) {

        if (!layout || descriptorSets.empty()) return;

        auto vkLayout = static_cast<VkPipelineLayout>(layout->getNativeHandle());
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
    void RHI_VK_CommandEncoder::pushConstants(
        RHIPipelineLayout* layout,
        ShaderStage stage,
        uint32_t offset,
        uint32_t size,
        const void* values) {
        RHI_VK_PipelineLayout* rhiLayout = static_cast<RHI_VK_PipelineLayout*>(layout);
        auto vkLayout = static_cast<VkPipelineLayout>(rhiLayout->getNativeHandle());
        vkCmdPushConstants(getVkCommandBuffer(), vkLayout, static_cast<VkShaderStageFlags>(stage), offset, size, values);
    }

    // 绘图命令
    void RHI_VK_CommandEncoder::draw(
        uint32_t vertexCount,
        uint32_t instanceCount,
        uint32_t firstVertex,
        uint32_t firstInstance) {
		vkCmdDraw(getVkCommandBuffer(), vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void RHI_VK_CommandEncoder::drawIndexed(
        uint32_t indexCount,
        uint32_t instanceCount,
        uint32_t firstIndex,
        int32_t vertexOff,
        uint32_t firstInstance) {
		vkCmdDrawIndexed(getVkCommandBuffer(), indexCount, instanceCount, firstIndex, vertexOff, firstInstance);
    }

    void RHI_VK_CommandEncoder::drawIndirect(
        RHIBuffer* buffer,
        uint64_t offset,
        uint32_t drawCount,
        uint32_t stride) {
       
    }

    void RHI_VK_CommandEncoder::drawIndexedIndirect(
        RHIBuffer* buffer,
        uint64_t offset,
        uint32_t drawCount,
        uint32_t stride) {
    }

    void RHI_VK_CommandEncoder::drawIndirectCount(
        RHIBuffer* buffer,
        uint64_t offset,
        RHIBuffer* countBuffer,
        uint64_t countOffset,
        uint32_t maxDrawCount,
        uint32_t stride) {
    }

    void RHI_VK_CommandEncoder::drawIndexedIndirectCount(
        RHIBuffer* buffer,
        uint64_t offset,
        RHIBuffer* countBuffer,
        uint64_t countOffset,
        uint32_t maxDrawCount,
        uint32_t stride) {
    }

    // 计算命令
    void RHI_VK_CommandEncoder::dispatch(
        uint32_t groupCountX,
        uint32_t groupCountY,
        uint32_t groupCountZ) {
    }

    void RHI_VK_CommandEncoder::dispatchIndirect(
        RHIBuffer* buffer,
        uint64_t offset) {
    }

    // 光线追踪命令
    void RHI_VK_CommandEncoder::traceRays(
        RHIBuffer* raygenTable,
        RHIBuffer* missTable,
        RHIBuffer* hitTable,
        RHIBuffer* callableTable,
        uint32_t width,
        uint32_t height,
        uint32_t depth) {
    }

    void RHI_VK_CommandEncoder::buildAccelerationStructure(
        const AccelerationStructureBuildInfo& buildInfo,
        RHIBuffer* scratchBuffer,
        uint64_t scratchOffset) {
    }

    void RHI_VK_CommandEncoder::copyAccelerationStructure(
        RHIBuffer* src,
        RHIBuffer* dst,
        CopyAccelerationStructureMode mode) {
    }

    void RHI_VK_CommandEncoder::beginRenderPass(
        const RenderPassBeginInfo& beginInfo,
        SubpassContents contents) {

        std::vector<VkClearValue> vkClearValues;
        vkClearValues.reserve(beginInfo.clearValues.size());

        for (size_t i = 0; i < beginInfo.clearValues.size(); ++i) {
            const auto& cv = beginInfo.clearValues[i];
            VkClearValue vkCv{};

            if (i == 0) {
                // 颜色清除
                vkCv.color.float32[0] = cv.color.r;
                vkCv.color.float32[1] = cv.color.g;
                vkCv.color.float32[2] = cv.color.b;
                vkCv.color.float32[3] = cv.color.a;
            }
            else {
                // 深度/模板清除
                vkCv.depthStencil.depth = cv.depth;
                vkCv.depthStencil.stencil = cv.stencil;
            }

            vkClearValues.push_back(vkCv);
        }

        VkRenderPassBeginInfo vkBeginInfo{};
        vkBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        vkBeginInfo.renderPass = static_cast<VkRenderPass>(beginInfo.renderPass);
        vkBeginInfo.framebuffer = static_cast<VkFramebuffer>(beginInfo.framebuffer);
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

    void RHI_VK_CommandEncoder::nextSubpass(SubpassContents contents) {
		vkCmdNextSubpass(getVkCommandBuffer(), (contents == SubpassContents::Inline) ? VK_SUBPASS_CONTENTS_INLINE : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    }
    void RHI_VK_CommandEncoder::endRenderPass() {
		vkCmdEndRenderPass(getVkCommandBuffer());
    }

    // 执行次命令缓冲区
    void RHI_VK_CommandEncoder::executeCommands(const std::vector<RHICommandBuffer*>& commandBuffers) {}

    // 资源屏障
    void RHI_VK_CommandEncoder::pipelineBarrier(
        PipelineStageFlags srcStage,
        PipelineStageFlags dstStage,
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
    void RHI_VK_CommandEncoder::copyBuffer(
        RHIBuffer* src,
        RHIBuffer* dst,
        const std::vector<BufferCopyRegion>& regions) {
    }

    void RHI_VK_CommandEncoder::copyImage(
        RHITexture* src,
        RHITexture* dst,
        const std::vector<ImageCopyRegion>& regions) {
    }

    void RHI_VK_CommandEncoder::copyBufferToImage(
        RHIBuffer* src,
        RHITexture* dst,
        const std::vector<BufferImageCopyRegion>& regions) {
    }

    void RHI_VK_CommandEncoder::copyImageToBuffer(
        RHITexture* src,
        RHIBuffer* dst,
        const std::vector<BufferImageCopyRegion>& regions) {
    }

    void RHI_VK_CommandEncoder::blitImage(
        RHITexture* src,
        ImageLayout srcLayout,
        RHITexture* dst,
        ImageLayout dstLayout,
        const std::vector<ImageBlitRegion>& regions,
        Filter filter) {
    }

    // 清除操作
    void RHI_VK_CommandEncoder::clearColorImage(
        RHITexture* image,
        ImageLayout layout,
        const Color& color,
        const std::vector<ImageSubresourceRange>& ranges) {
    }

    void RHI_VK_CommandEncoder::clearDepthStencilImage(
        RHITexture* image,
        ImageLayout layout,
        float depth,
        uint32_t stencil,
        const std::vector<ImageSubresourceRange>& ranges) {
    }

    void RHI_VK_CommandEncoder::clearAttachments(
        const std::vector<ClearAttachment>& attachments,
        const std::vector<ClearRect>& rects) {
    }

    // 填充缓冲区
    void RHI_VK_CommandEncoder::fillBuffer(
        RHIBuffer* buffer,
        uint64_t offset,
        uint64_t size,
        uint32_t data) {
    }

    // 更新缓冲区
    void RHI_VK_CommandEncoder::updateBuffer(
        RHIBuffer* buffer,
        uint64_t offset,
        uint64_t size,
        const void* data) {
    }

    // 查询操作
    //void RHI_VK_CommandEncoder::beginQuery(
    //    QueryPoolHandle queryPool,
    //    uint32_t query,
    //    QueryControlFlags flags) {
    //}

    //void RHI_VK_CommandEncoder::endQuery(
    //    QueryPoolHandle queryPool,
    //    uint32_t query) {
    //}

    //void RHI_VK_CommandEncoder::writeTimestamp(
    //    PipelineStage stage,
    //    QueryPoolHandle queryPool,
    //    uint32_t query) {
    //}

    //void RHI_VK_CommandEncoder::resetQueryPool(
    //    QueryPoolHandle queryPool,
    //    uint32_t firstQuery,
    //    uint32_t queryCount) {
    //}

    //void RHI_VK_CommandEncoder::copyQueryPoolResults(
    //    QueryPoolHandle queryPool,
    //    uint32_t firstQuery,
    //    uint32_t queryCount,
    //    RHIBuffer* dstBuffer,
    //    uint64_t dstOffset,
    //    uint64_t stride,
    //    QueryResultFlags flags) {
    //}

    // 调试标记
    void RHI_VK_CommandEncoder::beginDebugLabel(const char* label, const float color[4]) {}
    void RHI_VK_CommandEncoder::endDebugLabel() {}
    void RHI_VK_CommandEncoder::insertDebugLabel(const char* label, const float color[4]) {}


} // namespace StarryEngine::RHI