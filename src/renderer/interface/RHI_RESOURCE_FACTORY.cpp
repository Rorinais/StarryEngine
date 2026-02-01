#include "RHI_RESOURCE_FACTORY.hpp"
#include "RHI_VK_RESOURCE.hpp"
#include "RHI_RESOURCE_MANAGER.hpp"
#include <stdexcept>

namespace StarryEngine::RHI {
    std::unique_ptr<RHIBuffer> VKResourceFactory::createBuffer(const BufferDesc& desc) {
        // TODO: 实现Vulkan缓冲区创建
        throw std::runtime_error("Not implemented: createBuffer");
    }

    std::unique_ptr<RHITexture> VKResourceFactory::createTexture(const TextureDesc& desc) {
        // TODO: 实现Vulkan纹理创建
        throw std::runtime_error("Not implemented: createTexture");
    }

    std::unique_ptr<RHIPipeline> VKResourceFactory::createPipeline(const GraphicsPipelineDesc& desc) {
        // 创建管线布局
        auto layout = std::make_unique<RHI_VK_PipelineLayout>(mDevice, desc.layoutDesc);

        auto rhiVertexShader = mResourceManager->getShader(desc.vertexShader);
        auto vertexShader = static_cast<VkShaderModule>(rhiVertexShader->getNativeHandle());

        auto rhiFragmentShader = mResourceManager->getShader(desc.fragmentShader);
        auto fragmentShader = static_cast<VkShaderModule>(rhiFragmentShader->getNativeHandle());
        
        std::vector<VkPipelineShaderStageCreateInfo> shaderStage{
            mDevice->createShaderStageInfo(vertexShader,RHI_TO_VK_SHADERSTAGEFLAG(rhiVertexShader->getStage()),rhiVertexShader->getEntryPoint().c_str()),
            mDevice->createShaderStageInfo(fragmentShader,RHI_TO_VK_SHADERSTAGEFLAG(rhiFragmentShader->getStage()),rhiFragmentShader->getEntryPoint().c_str())
        };

        auto pipeline = std::make_unique<RHI_VK_Pipeline>(
            mDevice,
            desc,
            PipelineType::Graphics,
            std::move(layout)
        );

        return pipeline;
    }

    std::unique_ptr<RHIPipeline> VKResourceFactory::createComputePipeline(const ComputePipelineDesc& desc) {
        // TODO: 实现计算管线创建
        throw std::runtime_error("Not implemented: createComputePipeline");
    }

    std::unique_ptr<RHIPipelineLayout> VKResourceFactory::createPipelineLayout(const PipelineLayoutDesc& desc) {
        return std::make_unique<RHI_VK_PipelineLayout>(mDevice, desc);
    }

    std::unique_ptr<RHIShaderModule> VKResourceFactory::createShader(const ShaderModuleDesc& desc) {
        return std::make_unique<RHI_VK_ShaderModule>(mDevice, desc);
    }

    std::unique_ptr<RHISampler> VKResourceFactory::createSampler(const SamplerDesc& desc) {
        // TODO: 实现采样器创建
        throw std::runtime_error("Not implemented: createSampler");
    }

    std::unique_ptr<RHIRenderPass> VKResourceFactory::createRenderPass(const RenderPassDesc& desc) {
        // TODO: 实现渲染通道创建
        throw std::runtime_error("Not implemented: createRenderPass");
    }

    std::unique_ptr<RHIFramebuffer> VKResourceFactory::createFramebuffer(const FramebufferDesc& desc) {
        // TODO: 实现帧缓冲创建
        throw std::runtime_error("Not implemented: createFramebuffer");
    }

    std::unique_ptr<RHIDescriptorSet> VKResourceFactory::createDescriptorSet(const DescriptorSetDesc& desc) {
        // TODO: 实现描述符集创建
        throw std::runtime_error("Not implemented: createDescriptorSet");
    }

    std::unique_ptr<RHIDescriptorPool> VKResourceFactory::createDescriptorPool(const DescriptorPoolDesc& desc) {
        // TODO: 实现描述符池创建
        throw std::runtime_error("Not implemented: createDescriptorPool");
    }

    std::unique_ptr<RHIDescriptorSetLayout> VKResourceFactory::createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) {
        // TODO: 实现描述符集布局创建
        throw std::runtime_error("Not implemented: createDescriptorSetLayout");
    }

    std::unique_ptr<RHICommandBuffer> VKResourceFactory::createCommandBuffer(const CommandBufferDesc& desc) {
        // TODO: 实现命令缓冲区创建
        throw std::runtime_error("Not implemented: createCommandBuffer");
    }

    std::unique_ptr<RHICommandPool> VKResourceFactory::createCommandPool(const CommandPoolDesc& desc) {
        // TODO: 实现命令池创建
        throw std::runtime_error("Not implemented: createCommandPool");
    }

    std::unique_ptr<RHIFence> VKResourceFactory::createFence(const FenceDesc& desc) {
        // TODO: 实现栅栏创建
        throw std::runtime_error("Not implemented: createFence");
    }

    std::unique_ptr<RHISemaphore> VKResourceFactory::createSemaphore(const SemaphoreDesc& desc) {
        // TODO: 实现信号量创建
        throw std::runtime_error("Not implemented: createSemaphore");
    }

    std::unique_ptr<RHIEvent> VKResourceFactory::createEvent(const EventDesc& desc) {
        // TODO: 实现事件创建
        throw std::runtime_error("Not implemented: createEvent");
    }

    std::unique_ptr<RHIQueryPool> VKResourceFactory::createQueryPool(const QueryPoolDesc& desc) {
        // TODO: 实现查询池创建
        throw std::runtime_error("Not implemented: createQueryPool");
    }

    std::unique_ptr<RHIAccelerationStructure> VKResourceFactory::createAccelerationStructure(const AccelerationStructureDesc& desc) {
        // TODO: 实现加速结构创建
        throw std::runtime_error("Not implemented: createAccelerationStructure");
    }

    std::unique_ptr<RHISwapChain> VKResourceFactory::createSwapChain(const SwapChainDesc& desc) {
        // TODO: 实现交换链创建
        throw std::runtime_error("Not implemented: createSwapChain");
    }

    std::unique_ptr<RHIQueue> VKResourceFactory::createQueue(const QueueDesc& desc) {
        // TODO: 实现队列创建
        throw std::runtime_error("Not implemented: createQueue");
    }

    std::vector<std::unique_ptr<RHICommandBuffer>> VKResourceFactory::createCommandBuffers(
        uint32_t count,
        const CommandBufferDesc& desc) {

        std::vector<std::unique_ptr<RHICommandBuffer>> buffers;
        buffers.reserve(count);

        for (uint32_t i = 0; i < count; ++i) {
            buffers.push_back(createCommandBuffer(desc));
        }

        return buffers;
    }

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

} // namespace StarryEngine::RHI