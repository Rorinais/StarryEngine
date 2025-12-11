#pragma once
#include "pipeline.hpp"
#include <vulkan/vulkan.h>
#include <memory>

#ifdef  mydebug
namespace StarryEngine {

    class Pipeline;
    class PipelineBuilder {
    public:
        using Ptr = std::shared_ptr<PipelineBuilder>;

        PipelineBuilder(VkRenderPass renderPass, LogicalDevice::Ptr device);

        PipelineBuilder& setShaderProgram(ShaderProgram::Ptr program);
        PipelineBuilder& setVertexInput(const VertexInput& vertexInput);
        PipelineBuilder& setViewport(const Viewport& viewport);
        PipelineBuilder& setRasterization(const Rasterization& rasterization);
        PipelineBuilder& setDepthStencil(const DepthStencil& depthStencil);
        PipelineBuilder& setColorBlend(const ColorBlend& colorBlend);
        PipelineBuilder& setInputAssembly(const InputAssembly& inputAssembly);
        PipelineBuilder& setMultisample(const MultiSample& multisample);
        PipelineBuilder& setDynamic(const Dynamic& dynamic);
        PipelineBuilder& setDescriptorSetLayouts(const std::vector<VkDescriptorSetLayout>& layouts);

        Pipeline::Ptr build();

    private:
        VkRenderPass mRenderPass;
        LogicalDevice::Ptr mLogicalDevice;
        Pipeline::Config mConfig;
        ShaderProgram::Ptr mShaderProgram;
        std::vector<VkDescriptorSetLayout> mDescriptorSetLayouts;
    };
} // namespace StarryEngine
#endif
