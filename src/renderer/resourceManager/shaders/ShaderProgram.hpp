#pragma once
#include "shaderUtils.hpp"
namespace StarryEngine {
    class ShaderProgram {
    public:
        using Ptr = std::shared_ptr<ShaderProgram>;
        static Ptr create(const LogicalDevice::Ptr& logicalDevice) {
            return std::make_shared<ShaderProgram>(logicalDevice);
        }

        ShaderProgram(const LogicalDevice::Ptr& logicalDevice);

        ~ShaderProgram();

        const std::vector<VkPipelineShaderStageCreateInfo>& getStages() const { return mStages; }

        void addGLSLStage(
            const std::string& filename,
            VkShaderStageFlagBits stage,
            const char* entryPoint,
            const std::vector<std::pair<std::string, std::string>>& macros = {},
            const std::string& debugName = ""
        );

        void addGLSLStringStage(
            const std::string& sourceCode,
            VkShaderStageFlagBits stage,
            const char* entryPoint,
            const std::vector<std::pair<std::string, std::string>>& macros = {},
            const std::string& debugName = ""
        );

        void addSPVStage(
            const std::string& filename,
            VkShaderStageFlagBits stage,
            const char* entryPoint,
            const std::string& debugName = ""
        );

    private:
        VkPipelineShaderStageCreateInfo createStageInfo(
            VkShaderModule module,
            VkShaderStageFlagBits stage,
            const char* entryPoint
        );

    private:
        LogicalDevice::Ptr mLogicalDevice;
        ShaderUtils::Ptr mShaderUtils;
        std::vector<VkShaderModule> mShaderModules;
        std::vector<VkPipelineShaderStageCreateInfo> mStages;
    };
}