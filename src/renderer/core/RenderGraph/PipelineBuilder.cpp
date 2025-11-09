#include "PipelineBuilder.hpp"

namespace StarryEngine {

    PipelineBuilder::PipelineBuilder(VkRenderPass renderPass, const LogicalDevice::Ptr& logicalDevice)
        : mRenderPass(renderPass), mLogicalDevice(logicalDevice) {
    }

    PipelineBuilder& PipelineBuilder::setShaderProgram(ShaderProgram::Ptr program) {
        mShaderProgram = program;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::setVertexInput(const VertexInput& vertexInput) {
        mConfig.vertexInputState = vertexInput;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::setViewport(const Viewport& viewport) {
        mConfig.viewportState = viewport;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::setRasterization(const Rasterization& rasterization) {
        mConfig.rasterizationState = rasterization;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::setDepthStencil(const DepthStencil& depthStencil) {
        mConfig.depthStencilState = depthStencil;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::setColorBlend(const ColorBlend& colorBlend) {
        mConfig.colorBlendState = colorBlend;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::setInputAssembly(const InputAssembly& inputAssembly) {
        mConfig.inputAssemblyState = inputAssembly;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::setMultisample(const MultiSample& multisample) {
        mConfig.multisampleState = multisample;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::setDynamic(const Dynamic& dynamic) {
        mConfig.dynamicState = dynamic;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::setDescriptorSetLayouts(const std::vector<VkDescriptorSetLayout>& layouts) {
        mDescriptorSetLayouts = layouts;
        return *this;
    }

    Pipeline::Ptr PipelineBuilder::build() {
        auto pipeline = Pipeline::create(mLogicalDevice, mConfig);
        pipeline->setRenderPass(mRenderPass);

        if (mShaderProgram) {
            pipeline->setShaderStage(mShaderProgram);
        }

        if (!mDescriptorSetLayouts.empty()) {
            auto pipelineLayout = PipelineLayout::create(mLogicalDevice, mDescriptorSetLayouts);
            pipeline->setPipelineLayout(pipelineLayout);
        }

        pipeline->createGraphicsPipeline();
        return pipeline;
    }

} // namespace StarryEngine