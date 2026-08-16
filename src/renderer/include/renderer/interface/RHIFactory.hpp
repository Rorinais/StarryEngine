#pragma once

#include<interface/RHIEnums.hpp>
#include<interface/RHIHandles.hpp>
#include<interface/RHIStructs.hpp>
#include<interface/IRHIResources.hpp>

namespace StarryEngine::RHI {
    class IResourceFactory {
    public:
        virtual ~IResourceFactory() = default;

        virtual std::unique_ptr<RHIBuffer> createBuffer(const BufferDesc& desc) = 0;
        virtual std::unique_ptr<RHITexture> createTexture(const TextureDesc& desc) = 0;
        virtual std::unique_ptr<RHIShaderModule> createShader(const ShaderModuleDesc& desc) = 0;
        virtual std::unique_ptr<RHISampler> createSampler(const SamplerDesc& desc) = 0;
        virtual std::unique_ptr<RHIRenderPass> createRenderPass(const RenderPassDesc& desc) = 0;
        // renderPass/attachments 已由调用方（ResourceManager）解析成对象指针
        virtual std::unique_ptr<RHIFramebuffer> createFramebuffer(
            RHIRenderPass* renderPass,
            const std::vector<RHITexture*>& attachments,
            const FramebufferDesc& desc) = 0;
        virtual std::unique_ptr<RHIDescriptorPool> createDescriptorPool(const DescriptorPoolDesc& desc) = 0;
        virtual std::unique_ptr<RHIDescriptorSetLayout> createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) = 0;

        virtual std::unique_ptr<RHIPipelineLayout> createPipelineLayout(
            const PipelineLayoutDesc& desc,
            const std::vector<RHIDescriptorSetLayout*>& descriptorSetLayouts) = 0;

        virtual std::unique_ptr<RHIPipeline> createComputePipeline(
            const ComputePipelineDesc& desc,
            RHIShaderModule* computeShader, 
            RHIPipelineLayout* layout) = 0;

        virtual std::unique_ptr<RHIPipeline> createGraphicPipeline(
            const GraphicsPipelineDesc& desc,
            RHIPipelineLayout* layout,
            RHIShaderModule* vertexShader,
            RHIShaderModule* fragmentShader,
            RHIRenderPass* renderPass) = 0;

        virtual std::unique_ptr<RHIDescriptorSet> createDescriptorSet(
            const DescriptorSetDesc& desc, RHIDescriptorPool* pool, 
            RHIDescriptorSetLayout* layout) = 0;

        virtual std::vector<std::unique_ptr<RHIDescriptorSet>> createDescriptorSets(
            uint32_t count, const DescriptorSetDesc& desc,
            RHIDescriptorPool* pool,
            RHIDescriptorSetLayout* layout) = 0;
    };

    // 注意：RHIFactory（RHI 工厂）定义在 RHIConfig.hpp（含 create(RHIInitConfig)），
    // 本头只提供 IResourceFactory（资源对象工厂）。避免重复定义。
}