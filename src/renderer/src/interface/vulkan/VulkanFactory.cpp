#include <stdexcept>

#include <interface/vulkan/VulkanFactory.hpp>
#include <interface/vulkan/VulkanResources.hpp>
#include <interface/vulkan/VulkanConversion.hpp>


namespace StarryEngine::RHI {
    std::unique_ptr<RHIBuffer> VKResourceFactory::createBuffer(const BufferDesc& desc) {
		return std::make_unique<VulkanBuffer>(mDevice, desc);
    }

    std::unique_ptr<RHITexture> VKResourceFactory::createTexture(const TextureDesc& desc) {
        return std::make_unique<VulkanTexture>(mDevice, desc);
    }

    std::unique_ptr<RHIPipeline> VKResourceFactory::createGraphicPipeline(const GraphicsPipelineDesc& desc,
        RHIPipelineLayout* layoutObj,
        RHIShaderModule* rhiVertexShader,
        RHIShaderModule* rhiFragmentShader,
        RHIRenderPass* renderPassObj) {
        if (!layoutObj || !rhiVertexShader || !rhiFragmentShader || !renderPassObj) return nullptr;

        auto layout = static_cast<VkPipelineLayout>(layoutObj->getNativeHandle());
        auto vertexShader = static_cast<VkShaderModule>(rhiVertexShader->getNativeHandle());
        auto fragmentShader = static_cast<VkShaderModule>(rhiFragmentShader->getNativeHandle());
        auto renderPass = static_cast<VkRenderPass>(renderPassObj->getNativeHandle());

        std::vector<VkPipelineShaderStageCreateInfo> shaderStage{
            mDevice->createShaderStageInfo(vertexShader, func::RHI_TO_VK_ShaderStageFlag(rhiVertexShader->getStage()), rhiVertexShader->getEntryPoint().c_str()),
            mDevice->createShaderStageInfo(fragmentShader, func::RHI_TO_VK_ShaderStageFlag(rhiFragmentShader->getStage()), rhiFragmentShader->getEntryPoint().c_str())
        };

        return std::make_unique<VulkanGraphicPipeline>(mDevice, desc, shaderStage, layout, renderPass);
    }

    std::unique_ptr<RHIPipeline> VKResourceFactory::createComputePipeline(const ComputePipelineDesc& desc,
        RHIShaderModule* rhiShader,
        RHIPipelineLayout* layoutObj) {
        if (!rhiShader || !layoutObj) return nullptr;

        auto shaderModule = static_cast<VkShaderModule>(rhiShader->getNativeHandle());
        auto layout = static_cast<VkPipelineLayout>(layoutObj->getNativeHandle());

        auto shaderStage = mDevice->createShaderStageInfo(shaderModule, func::RHI_TO_VK_ShaderStageFlag(rhiShader->getStage()), rhiShader->getEntryPoint().c_str());

        return std::make_unique<VulkanComputePipeline>(mDevice, desc, shaderStage, layout);
    }

    std::unique_ptr<RHIPipelineLayout> VKResourceFactory::createPipelineLayout(
        const PipelineLayoutDesc& desc,
        const std::vector<RHIDescriptorSetLayout*>& descriptorSetLayouts) {
        std::vector<VkDescriptorSetLayout> vkDescSetlayouts{};
        vkDescSetlayouts.reserve(descriptorSetLayouts.size());
        for (auto* rhiDesSetlayout : descriptorSetLayouts) {
            if (!rhiDesSetlayout) return nullptr;
            vkDescSetlayouts.push_back(static_cast<VkDescriptorSetLayout>(rhiDesSetlayout->getNativeHandle()));
        }
        return std::make_unique<VulkanPipelineLayout>(mDevice, desc, vkDescSetlayouts);
    }

    std::unique_ptr<RHIShaderModule> VKResourceFactory::createShader(const ShaderModuleDesc& desc) {
        return std::make_unique<VulkanShaderModule>(mDevice, desc);
    }

    std::unique_ptr<RHISampler> VKResourceFactory::createSampler(const SamplerDesc& desc) {
        return std::make_unique<VulkanSampler>(mDevice, desc);
    }

    std::unique_ptr<RHIRenderPass> VKResourceFactory::createRenderPass(const RenderPassDesc& desc) {
		return std::make_unique<VulkanRenderPass>(mDevice, desc);
    }

    std::unique_ptr<RHIFramebuffer> VKResourceFactory::createFramebuffer(
        RHIRenderPass* renderPass,
        const std::vector<RHITexture*>& attachments,
        const FramebufferDesc& desc) {
		return std::make_unique<VulkanFramebuffer>(mDevice, renderPass, attachments, desc);
    }

    std::unique_ptr<RHIDescriptorSet> VKResourceFactory::createDescriptorSet(const DescriptorSetDesc& desc,
        RHIDescriptorPool* poolObj,
        RHIDescriptorSetLayout* layoutObj) {
        if (!poolObj || !layoutObj) {
            throw std::runtime_error("Invalid descriptor pool/layout in VKResourceFactory");
        }

        auto* pool = dynamic_cast<VulkanDescriptorPool*>(poolObj);
        auto* layout = dynamic_cast<VulkanDescriptorSetLayout*>(layoutObj);
        if (!pool || !layout) {
            throw std::runtime_error("Descriptor pool/layout is not a Vulkan resource");
        }

        auto sets = pool->allocateDescriptorSets({ layout });
        if (sets.empty()) {
            throw std::runtime_error("Failed to allocate descriptor set");
        }

        return std::move(sets[0]);
    }

    std::unique_ptr<RHIDescriptorPool> VKResourceFactory::createDescriptorPool(const DescriptorPoolDesc& desc) {
        return std::make_unique<VulkanDescriptorPool>(mDevice, desc);
    }

    std::unique_ptr<RHIDescriptorSetLayout> VKResourceFactory::createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) {
        return std::make_unique<VulkanDescriptorSetLayout>(mDevice, desc);
    }

    std::vector<std::unique_ptr<RHIDescriptorSet>> VKResourceFactory::createDescriptorSets(
        uint32_t count,
        const DescriptorSetDesc& desc,
        RHIDescriptorPool* pool,
        RHIDescriptorSetLayout* layout) {

        std::vector<std::unique_ptr<RHIDescriptorSet>> sets;
        sets.reserve(count);

        for (uint32_t i = 0; i < count; ++i) {
            sets.push_back(createDescriptorSet(desc, pool, layout));
        }

        return sets;
    }

} // namespace StarryEngine::RHI