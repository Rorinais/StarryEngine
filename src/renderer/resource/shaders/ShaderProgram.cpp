#include"ShaderProgram.hpp"
namespace StarryEngine {
    ShaderProgram::ShaderProgram(const LogicalDevice::Ptr& logicalDevice) : mLogicalDevice(logicalDevice) {
        mShaderUtils = ShaderUtils::create(logicalDevice);
    }

    ShaderProgram::~ShaderProgram() {
        for (auto module : mShaderModules) {
            vkDestroyShaderModule(mLogicalDevice->getHandle(), module, nullptr);
        }
    }

    // 添加GLSL着色器阶段（支持宏）
    void ShaderProgram::addGLSLStage(
        const std::string& filename,
        VkShaderStageFlagBits stage,
        const char* entryPoint,
        const std::vector<std::pair<std::string, std::string>>& macros,
        const std::string& debugName
    ) {
        VkShaderModule module = mShaderUtils->loadFromGLSL(
            filename, stage, macros, debugName
        );
        mShaderModules.push_back(module);
        mStages.push_back(createStageInfo(module, stage, entryPoint));
    }

    // 添加预编译的SPIR-V着色器阶段
    void ShaderProgram::addSPVStage(
        const std::string& filename,
        VkShaderStageFlagBits stage,
        const char* entryPoint,
        const std::string& debugName
    ) {
        VkShaderModule module = mShaderUtils->loadFromSPV(filename, debugName);
        mShaderModules.push_back(module);
        mStages.push_back(createStageInfo(module, stage, entryPoint));
    }

    void ShaderProgram::addGLSLStringStage(
        const std::string& sourceCode,
        VkShaderStageFlagBits stage,
        const char* entryPoint,
        const std::vector<std::pair<std::string, std::string>>& macros,
        const std::string& debugName
    ) {
        VkShaderModule module = mShaderUtils->loadFromGLSLString(
            sourceCode, stage, macros, debugName
        );
        mShaderModules.push_back(module);
        mStages.push_back(createStageInfo(module, stage, entryPoint));
    }

    VkPipelineShaderStageCreateInfo ShaderProgram::createStageInfo(
        VkShaderModule module,
        VkShaderStageFlagBits stage,
        const char* entryPoint
    ) {
        return VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = stage,
            .module = module,
            .pName = entryPoint,
            .pSpecializationInfo = nullptr
        };
    }
}