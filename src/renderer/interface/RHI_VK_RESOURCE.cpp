#include"RHI_VK_RESOURCE.hpp"

namespace StarryEngine::RHI {

    RHI_VK_ShaderModule::RHI_VK_ShaderModule(Device::Ptr device, ShaderModuleDesc desc)
        : mDevice(device), mDesc(desc), mShaderModule(VK_NULL_HANDLE) {

        try {
            shaderc_shader_kind kind;
            switch (mDesc.stage) {
            case ShaderStage::Vertex:   kind = shaderc_vertex_shader; break;
            case ShaderStage::Fragment: kind = shaderc_fragment_shader; break;
            case ShaderStage::Compute:  kind = shaderc_compute_shader; break;
            case ShaderStage::Geometry: kind = shaderc_geometry_shader; break;
            case ShaderStage::TessellationControl: kind = shaderc_tess_control_shader; break;
            case ShaderStage::TessellationEvaluation: kind = shaderc_tess_evaluation_shader; break;
            default:
                throw std::runtime_error("Unsupported shader stage: " + std::to_string(static_cast<int>(mDesc.stage)));
            }

            auto spirv = compileGLSL(mDesc.sourcecode, kind, mDesc.defines, mDesc.debugName);

            mShaderModule = mDevice->createShaderModule(spirv, mDesc.debugName);
            std::cout << "[RHI_VK_ShaderModule] Created shader module: "<< mShaderModule << " for " << mDesc.debugName << std::endl;

        }
        catch (const std::exception& e) {
            std::cerr << "[RHI_VK_ShaderModule] ERROR: Failed to create shader: "<< mDesc.debugName << " - " << e.what() << std::endl;

            mShaderModule = VK_NULL_HANDLE;
            throw;
        }
    }

    void RHI_VK_ShaderModule::release() {
        mDevice->destroyShaderModule(mShaderModule);
    }

    std::vector<uint32_t> RHI_VK_ShaderModule::compileGLSL(
        const std::string& source,
        shaderc_shader_kind kind,
        const std::vector<std::pair<std::string, std::string>>& macros,
        const std::string& debugName) {
        shaderc::CompileOptions options;
        options.SetTargetEnvironment(
            shaderc_target_env_vulkan,
            shaderc_env_version_vulkan_1_2
        );
        options.SetOptimizationLevel(shaderc_optimization_level_performance);

        // 添加用户定义的宏
        for (const auto& [name, value] : macros) {
            if (value.empty()) {
                options.AddMacroDefinition(name);
            }
            else {
                options.AddMacroDefinition(name, value);
            }
        }

        shaderc::SpvCompilationResult result = mCompiler.CompileGlslToSpv(
            source, kind, debugName.c_str(), options
        );

        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            throw std::runtime_error("Shader compile error: " + debugName + "\n" + result.GetErrorMessage());
        }

        return { result.cbegin(), result.cend() };
    }
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
        //if (set < mDesc.descriptorSets.size()) {
            //return mDesc.descriptorSets[set];
        //}
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
    }

    void RHI_VK_Pipeline::release(){
        mDevice->destroyPipeline(mPipeline);
        mLayout.reset();
    }

    void RHI_VK_Pipeline::createGraphicsPipeline() {
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

}