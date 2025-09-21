#include"pipeline.hpp"

namespace StarryEngine {

    Pipeline::Pipeline(const LogicalDevice::Ptr& logicalDevice, Pipeline::Config config)
        :mLogicalDevice(logicalDevice), mPipelineStageConfig(config) {

        auto colorBlend = ColorBlend();
        ColorBlend::Config colorBlendConfig{};
        colorBlendConfig.blendEnable = VK_FALSE;
        colorBlendConfig.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendConfig.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendConfig.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendConfig.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendConfig.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendConfig.alphaBlendOp = VK_BLEND_OP_ADD;
        colorBlendConfig.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;

        colorBlend.addAttachment(colorBlendConfig);
        setColorBlendState(colorBlend);

        auto depthStencil = DepthStencil();
        depthStencil.enableDepthTest(VK_FALSE);
        depthStencil.enableDepthWrite(VK_FALSE);
        depthStencil.setDepthCompareOp(VK_COMPARE_OP_LESS);
        setDepthStencilState(depthStencil);

        //auto inputAssembly = InputAssembly();
        //inputAssembly.setTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST); 
        //inputAssembly.enablePrimitiveRestart(VK_FALSE); 
        //setInputAssemblyState(inputAssembly);

        auto multiSample = MultiSample();
        multiSample.enableSampleShading(VK_FALSE);
        multiSample.setSampleCount(VK_SAMPLE_COUNT_1_BIT);
        setMultiSampleState(multiSample);

        auto rasterization = Rasterization();
        rasterization.setCullMode(VK_CULL_MODE_NONE)
            .setFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE);
        setRasterizationState(rasterization);

        auto dynamic = Dynamic();
        dynamic.setEnableDynamic(true);
        setDynamicStates(dynamic);

        const std::vector<VkDescriptorSetLayout> descriptorSetLayout;
        auto pipelineLayout = PipelineLayout::create(mLogicalDevice, descriptorSetLayout);
        setPipelineLayout(pipelineLayout);

        setSubPass(0);

    }

    Pipeline::~Pipeline() {
        cleanup();
    }

    void Pipeline::cleanup() {
        if (mLogicalDevice->getHandle() != VK_NULL_HANDLE) {
            mPipelineLayout.reset();
            if (mGraphicsPipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(mLogicalDevice->getHandle(), mGraphicsPipeline, nullptr);
                mGraphicsPipeline = VK_NULL_HANDLE;
            }
        }
    }

    void Pipeline::setShaderStage(ShaderProgram::Ptr shaderStates) {
        mShaderStages = shaderStates;
    }
    void Pipeline::setVertexInputState(VertexInput vertexInput) {
        mPipelineStageConfig.vertexInputState = vertexInput;
    }
    void Pipeline::setInputAssemblyState(InputAssembly inputAssembly) {
        mPipelineStageConfig.inputAssemblyState = inputAssembly;
    }
    void Pipeline::setViewportState(Viewport viewport) {
        mPipelineStageConfig.viewportState = viewport;
    }
    void Pipeline::setRasterizationState(Rasterization rasterization) {
        mPipelineStageConfig.rasterizationState = rasterization;
    }
    void Pipeline::setMultiSampleState(MultiSample multiSample) {
        mPipelineStageConfig.multisampleState = multiSample;
    }
    void Pipeline::setColorBlendState(ColorBlend colorBlend) {
        mPipelineStageConfig.colorBlendState = colorBlend;
    }
    void Pipeline::setDepthStencilState(DepthStencil depthStencil) {
        mPipelineStageConfig.depthStencilState = depthStencil;
    }

    void Pipeline::setDynamicStates(Dynamic dynamic) {
        mPipelineStageConfig.dynamicState = dynamic;
    }

    void Pipeline::setPipelineLayout(PipelineLayout::Ptr pipelineLayout) {
        mPipelineLayout = pipelineLayout;
    }
    void Pipeline::setRenderPass(RenderPass* renderPass) {
        mRenderPass = renderPass;
    }
    void Pipeline::setSubPass(uint32_t count) {
        mSubpass = count;
    }
    void Pipeline::setBasePipelineHandle(VkPipeline basePipeline) {
        mBasePipelineHandle = basePipeline;
    }
    void Pipeline::setBasePipelineIndex(uint32_t index) {
        mBasePipelineIndex = index;
    }

    void Pipeline::createGraphicsPipeline() {
        VkGraphicsPipelineCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        createInfo.stageCount = static_cast<uint32_t>(mShaderStages->getStages().size());
        createInfo.pStages = mShaderStages->getStages().data();
        createInfo.pVertexInputState = &mPipelineStageConfig.vertexInputState.getCreateInfo();
        createInfo.pInputAssemblyState = &mPipelineStageConfig.inputAssemblyState.getCreateInfo();
        createInfo.pViewportState = &mPipelineStageConfig.viewportState.getCreateInfo();
        createInfo.pRasterizationState = &mPipelineStageConfig.rasterizationState.getCreateInfo();
        createInfo.pMultisampleState = &mPipelineStageConfig.multisampleState.getCreateInfo();
        createInfo.pColorBlendState = &mPipelineStageConfig.colorBlendState.getCreateInfo();
        createInfo.pDynamicState = &mPipelineStageConfig.dynamicState.getCreateInfo();
        createInfo.layout = mPipelineLayout->getHandle();
        createInfo.renderPass = mRenderPass->getHandle();
        createInfo.subpass = mSubpass;
        createInfo.basePipelineHandle = mBasePipelineHandle;
        createInfo.basePipelineIndex = mBasePipelineIndex;
        createInfo.pDepthStencilState = &mPipelineStageConfig.depthStencilState.getCreateInfo();

        if (vkCreateGraphicsPipelines(mLogicalDevice->getHandle(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &mGraphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline");
        }
    }
}
