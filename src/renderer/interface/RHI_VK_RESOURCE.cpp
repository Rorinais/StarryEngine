#include"RHI_VK_RESOURCE.hpp"

namespace StarryEngine::RHI {
    //RHI_VK_PipelineLayout
    void RHI_VK_PipelineLayout::destroy() {
        if (mDevice && mDevice->getLogicalDevice() != VK_NULL_HANDLE) {
            if (mPipelineLayout != VK_NULL_HANDLE) {
                mDevice->destroyPipelineLayout(mPipelineLayout);
                mPipelineLayout = VK_NULL_HANDLE;
            }

            // 销毁描述符集布局
            for (auto layout : mVkDescriptorSetLayouts) {
                vkDestroyDescriptorSetLayout(mDevice->getLogicalDevice(), layout, nullptr);
            }
            mVkDescriptorSetLayouts.clear();
        }
    }

    const std::vector<RHI::DescriptorSetLayoutBinding>& RHI_VK_PipelineLayout::getDescriptorSetLayout(uint32_t set) const  {
        if (set < mDesc.descriptorSets.size()) {
            return mDesc.descriptorSets[set];
        }
        static const std::vector<RHI::DescriptorSetLayoutBinding> empty;
        return empty;
    }

    const RHI::PushConstantRange& RHI_VK_PipelineLayout::getPushConstantRange(uint32_t index) const  {
        if (index < mDesc.pushConstants.size()) {
            return mDesc.pushConstants[index];
        }
        throw std::out_of_range("Push constant range index out of range");
    }

    uint32_t RHI_VK_PipelineLayout::getBindingPoint(uint32_t set, uint32_t binding) const  {
        // 简化实现：返回绑定索引本身
        // 实际实现中可能需要从Vulkan反射数据中获取
        return binding;
    }

    size_t RHI_VK_PipelineLayout::getMemoryUsage() const  {
        size_t size = sizeof(*this);
        // 计算描述符集布局的内存使用
        for (const auto& set : mDesc.descriptorSets) {
            size += set.size() * sizeof(RHI::DescriptorSetLayoutBinding);
        }
        size += mDesc.pushConstants.size() * sizeof(RHI::PushConstantRange);
        return size;
    }

    VkDescriptorSetLayout RHI_VK_PipelineLayout::getVkDescriptorSetLayout(uint32_t set) const {
        if (set < mVkDescriptorSetLayouts.size()) {
            return mVkDescriptorSetLayouts[set];
        }
        return VK_NULL_HANDLE;
    }

    void RHI_VK_PipelineLayout::createPipelineLayout() {
        // 创建Vulkan描述符集布局
        mVkDescriptorSetLayouts.reserve(mDesc.descriptorSets.size());

        for (const auto& descriptorSet : mDesc.descriptorSets) {
            std::vector<VkDescriptorSetLayoutBinding> vkBindings;
            vkBindings.reserve(descriptorSet.size());

            for (const auto& binding : descriptorSet) {
                VkDescriptorSetLayoutBinding vkBinding{};
                vkBinding.binding = binding.binding;
                vkBinding.descriptorType = static_cast<VkDescriptorType>(binding.type);//这个应该不是这样写
                vkBinding.descriptorCount = binding.count;
                vkBinding.stageFlags = static_cast<VkShaderStageFlags>(binding.stageFlags);
                vkBinding.pImmutableSamplers = nullptr;

                vkBindings.push_back(vkBinding);
            }

            VkDescriptorSetLayout vkLayout = mDevice->createDescriptorSetLayout(vkBindings);

            mVkDescriptorSetLayouts.push_back(vkLayout);
        }
        std::cout << "descriptorSet" << std::endl;
        // 创建Vulkan推送常量范围
        std::vector<VkPushConstantRange> vkPushConstants;
        vkPushConstants.reserve(mDesc.pushConstants.size());

        for (const auto& range : mDesc.pushConstants) {
            VkPushConstantRange vkRange{};
            vkRange.stageFlags = static_cast<VkShaderStageFlags>(range.stage);
            vkRange.offset = range.offset;
            vkRange.size = range.size;
            vkPushConstants.push_back(vkRange);
        }

        mDevice->createPipelineLayout(mVkDescriptorSetLayouts, vkPushConstants);


    }

    //RHI_VK_Pipeline
    RHI_VK_Pipeline::RHI_VK_Pipeline(
        Device::Ptr device,
        const GraphicsPipelineDesc& pipelineDesc,
        PipelineType type,
        std::unique_ptr<RHI_VK_PipelineLayout> layout 
    ) : mDevice(device), mType(type), mDesc(pipelineDesc), mLayout(std::move(layout)) {

        if (!mLayout) {
            // 如果没有提供布局，使用描述符中的布局描述
            mLayout = std::make_unique<RHI_VK_PipelineLayout>(mDevice, pipelineDesc.layoutDesc);
        }

        createGraphicsPipeline();
    }

    void RHI_VK_Pipeline::destroy() {
        if (mDevice && mDevice->getLogicalDevice() != VK_NULL_HANDLE && mPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(mDevice->getLogicalDevice(), mPipeline, nullptr);
            mPipeline = VK_NULL_HANDLE;
        }
        mLayout.reset();
    }

    void RHI_VK_Pipeline::createGraphicsPipeline() {
        // 创建着色器模块
        std::vector<VkShaderModule> shaderModules;
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

        // 顶点着色器
        if (!mDesc.vertexShader.bytecode.empty()) {
            auto shaderModule = createShaderModule(mDesc.vertexShader);
            if (shaderModule != VK_NULL_HANDLE) {
                shaderModules.push_back(shaderModule);

                VkPipelineShaderStageCreateInfo stageInfo{};
                stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
                stageInfo.module = shaderModule;
                stageInfo.pName = mDesc.vertexShader.entryPoint.c_str();
                shaderStages.push_back(stageInfo);
            }
        }

        // 片段着色器
        if (!mDesc.fragmentShader.bytecode.empty()) {
            auto shaderModule = createShaderModule(mDesc.fragmentShader);
            if (shaderModule != VK_NULL_HANDLE) {
                shaderModules.push_back(shaderModule);

                VkPipelineShaderStageCreateInfo stageInfo{};
                stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                stageInfo.module = shaderModule;
                stageInfo.pName = mDesc.fragmentShader.entryPoint.c_str();
                shaderStages.push_back(stageInfo);
            }
        }

        if (shaderStages.empty()) {
            throw std::runtime_error("No valid shaders provided for graphics pipeline");
        }

        // 顶点输入状态
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        // 根据 VertexLayout 创建单个顶点绑定
        std::vector<VkVertexInputBindingDescription> bindingDescriptions;

        // 注意：VertexLayout 中没有 bindings 成员，只有一个 binding 的概念
        // 我们假设绑定索引为 0
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0; // 使用默认绑定索引 0
        bindingDescription.stride = mDesc.vertexLayout.stride;
        bindingDescription.inputRate = static_cast<VkVertexInputRate>(mDesc.vertexLayout.inputRate);

        // 注意：这里我们忽略 instanceStepRate，因为 Vulkan 中没有直接对应的字段
        // instanceStepRate 通常在顶点着色器中处理
        bindingDescriptions.push_back(bindingDescription);

        // 顶点属性描述
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        attributeDescriptions.reserve(mDesc.vertexLayout.attributes.size());

        for (const auto& attribute : mDesc.vertexLayout.attributes) {
            VkVertexInputAttributeDescription vkAttribute{};
            vkAttribute.location = attribute.location;
            vkAttribute.binding = 0; // 所有属性都绑定到同一个绑定点（索引 0）
            vkAttribute.format = static_cast<VkFormat>(attribute.format);
            vkAttribute.offset = attribute.offset;
            attributeDescriptions.push_back(vkAttribute);
        }

        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        // 输入装配状态
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = static_cast<VkPrimitiveTopology>(mDesc.topology);
        inputAssembly.primitiveRestartEnable = mDesc.primitiveRestartEnable;

        // 视口和裁剪状态（使用动态状态）
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        // 光栅化状态
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

        // 从 mDesc.rasterizer 复制所有字段
        rasterizer.depthClampEnable = mDesc.rasterizer.depthClampEnable ? VK_TRUE : VK_FALSE;
        rasterizer.rasterizerDiscardEnable = mDesc.rasterizer.discardEnable ? VK_TRUE : VK_FALSE;
        rasterizer.polygonMode = static_cast<VkPolygonMode>(mDesc.rasterizer.polygonMode);
        rasterizer.lineWidth = mDesc.rasterizer.lineWidth;
        rasterizer.cullMode = static_cast<VkCullModeFlags>(mDesc.rasterizer.cullMode);
        rasterizer.frontFace = static_cast<VkFrontFace>(mDesc.rasterizer.frontFace);
        rasterizer.depthBiasEnable = mDesc.rasterizer.depthBiasEnable ? VK_TRUE : VK_FALSE;
        rasterizer.depthBiasConstantFactor = mDesc.rasterizer.depthBiasConstantFactor;
        rasterizer.depthBiasClamp = mDesc.rasterizer.depthBiasClamp;
        rasterizer.depthBiasSlopeFactor = mDesc.rasterizer.depthBiasSlopeFactor;

        // 多重采样状态
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE; // 暂时禁用
        multisampling.rasterizationSamples = static_cast<VkSampleCountFlagBits>(mDesc.sampleCount);
        multisampling.minSampleShading = 1.0f;
        multisampling.pSampleMask = &mDesc.sampleMask;
        multisampling.alphaToCoverageEnable = mDesc.alphaToCoverageEnable ? VK_TRUE : VK_FALSE;
        multisampling.alphaToOneEnable = mDesc.alphaToOneEnable ? VK_TRUE : VK_FALSE;

        // 深度模板状态
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = mDesc.depthStencil.depthTestEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthWriteEnable = mDesc.depthStencil.depthWriteEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = static_cast<VkCompareOp>(mDesc.depthStencil.depthCompareOp);
        depthStencil.depthBoundsTestEnable = mDesc.depthStencil.depthBoundsTestEnable ? VK_TRUE : VK_FALSE;
        depthStencil.stencilTestEnable = mDesc.depthStencil.stencilTestEnable ? VK_TRUE : VK_FALSE;

        // 设置深度边界
        depthStencil.minDepthBounds = mDesc.depthStencil.minDepthBounds;
        depthStencil.maxDepthBounds = mDesc.depthStencil.maxDepthBounds;

        // 设置模板操作（简化版）
        depthStencil.front = convertStencilOpState(mDesc.depthStencil.front);
        depthStencil.back = convertStencilOpState(mDesc.depthStencil.back);

        // 颜色混合状态
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = mDesc.colorBlend.logicOpEnable ? VK_TRUE : VK_FALSE;
        colorBlending.logicOp = static_cast<VkLogicOp>(mDesc.colorBlend.logicOp);

        // 颜色混合附件状态
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
        colorBlendAttachments.reserve(mDesc.colorBlend.attachments.size());

        for (const auto& attachment : mDesc.colorBlend.attachments) {
            VkPipelineColorBlendAttachmentState vkAttachment{};
            vkAttachment.blendEnable = attachment.blendEnable ? VK_TRUE : VK_FALSE;
            vkAttachment.srcColorBlendFactor = static_cast<VkBlendFactor>(attachment.srcColorBlendFactor);
            vkAttachment.dstColorBlendFactor = static_cast<VkBlendFactor>(attachment.dstColorBlendFactor);
            vkAttachment.colorBlendOp = static_cast<VkBlendOp>(attachment.colorBlendOp);
            vkAttachment.srcAlphaBlendFactor = static_cast<VkBlendFactor>(attachment.srcAlphaBlendFactor);
            vkAttachment.dstAlphaBlendFactor = static_cast<VkBlendFactor>(attachment.dstAlphaBlendFactor);
            vkAttachment.alphaBlendOp = static_cast<VkBlendOp>(attachment.alphaBlendOp);
            vkAttachment.colorWriteMask = static_cast<VkColorComponentFlags>(attachment.colorWriteMask);
            colorBlendAttachments.push_back(vkAttachment);
        }

        colorBlending.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
        colorBlending.pAttachments = colorBlendAttachments.data();

        // 设置混合常量
        for (int i = 0; i < 4; ++i) {
            colorBlending.blendConstants[i] = mDesc.colorBlend.blendConstants[i];
        }

        // 动态状态
        std::vector<VkDynamicState> dynamicStates;

        // 检查是否需要动态视口
        bool hasDynamicViewport = false;
        bool hasDynamicScissor = false;
        for (const auto& state : mDesc.dynamicStates) {
            if (state == "Viewport") {
                hasDynamicViewport = true;
                dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
            }
            else if (state == "Scissor") {
                hasDynamicScissor = true;
                dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
            }
            else if (state == "LineWidth") {
                dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
            }
            // 可以添加更多动态状态的检查
        }

        // 如果视口或裁剪不是动态的，我们需要在管线创建时设置它们
        if (!hasDynamicViewport || !hasDynamicScissor) {
            // 注意：这里需要设置视口和裁剪，但在简化版中我们先使用动态状态
        }

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // 创建图形管线
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = dynamicStates.empty() ? nullptr : &dynamicState;
        pipelineInfo.layout = mLayout->getVkPipelineLayout();
        pipelineInfo.renderPass = reinterpret_cast<VkRenderPass>(mDesc.renderPass); // 假设这是 VkRenderPass
        pipelineInfo.subpass = mDesc.subpass;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        VkResult result = vkCreateGraphicsPipelines(
            mDevice->getLogicalDevice(),
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &mPipeline
        );

        // 清理着色器模块
        for (auto shaderModule : shaderModules) {
            vkDestroyShaderModule(mDevice->getLogicalDevice(), shaderModule, nullptr);
        }

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline");
        }
    }

    // 辅助函数：转换模板操作状态
    VkStencilOpState RHI_VK_Pipeline::convertStencilOpState(const StencilOpState& state) {
        VkStencilOpState vkState{};
        vkState.failOp = static_cast<VkStencilOp>(state.failOp);
        vkState.passOp = static_cast<VkStencilOp>(state.passOp);
        vkState.depthFailOp = static_cast<VkStencilOp>(state.depthFailOp);
        vkState.compareOp = static_cast<VkCompareOp>(state.compareOp);
        vkState.compareMask = state.compareMask;
        vkState.writeMask = state.writeMask;
        vkState.reference = state.reference;
        return vkState;
    }

    VkShaderModule RHI_VK_Pipeline::createShaderModule(const ShaderModuleDesc& desc) {
        if (desc.bytecode.empty()) {
            return VK_NULL_HANDLE;
        }

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = desc.bytecode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(desc.bytecode.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(mDevice->getLogicalDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }
        return shaderModule;
    }

}