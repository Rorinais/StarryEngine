#pragma once
#include "../interface/TypedPipelineComponent.hpp"
#include "../../../../../renderer/resource/shaders/ShaderProgram.hpp"
#include "../../../../../renderer/resource/shaders/ShaderBuilder.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace StarryEngine {
    class ShaderStageComponent : public
        TypedPipelineComponent<ShaderStageComponent, PipelineComponentType::SHADER_STAGE> {
    public:
        ShaderStageComponent(const std::string& name);
        ShaderStageComponent& reset();

        // 设置着色器程序
        ShaderStageComponent& setShaderProgram(std::shared_ptr<ShaderProgram> program);

        // 添加着色器阶段（便捷方法）
        ShaderStageComponent& addVertexShader(const std::string& filename,
            const std::string& entryPoint = "main",
            const std::vector<std::pair<std::string, std::string>>& macros = {},
            const std::string& debugName = "");

        ShaderStageComponent& addFragmentShader(const std::string& filename,
            const std::string& entryPoint = "main",
            const std::vector<std::pair<std::string, std::string>>& macros = {},
            const std::string& debugName = "");

        // 从字符串添加着色器
        ShaderStageComponent& addVertexShaderFromString(const std::string& sourceCode,
            const std::string& entryPoint = "main",
            const std::vector<std::pair<std::string, std::string>>& macros = {},
            const std::string& debugName = "");

        ShaderStageComponent& addFragmentShaderFromString(const std::string& sourceCode,
            const std::string& entryPoint = "main",
            const std::vector<std::pair<std::string, std::string>>& macros = {},
            const std::string& debugName = "");

        // 使用ShaderBuilder构建着色器
        ShaderStageComponent& addShaderFromBuilder(std::shared_ptr<ShaderBuilder> builder,
            VkShaderStageFlagBits stage,
            const std::string& entryPoint = "main",
            const std::vector<std::pair<std::string, std::string>>& macros = {},
            const std::string& debugName = "");

        // 应用组件到管线创建信息
        void apply(VkGraphicsPipelineCreateInfo& pipelineInfo) override {
            if (mShaderProgram && !mShaderProgram->getStages().empty()) {
                pipelineInfo.stageCount = static_cast<uint32_t>(mShaderProgram->getStages().size());
                pipelineInfo.pStages = mShaderProgram->getStages().data();
            }
            else {
                pipelineInfo.stageCount = 0;
                pipelineInfo.pStages = nullptr;
            }
        }

        std::string getDescription() const override;
        bool isValid() const override;

        // 获取着色器程序
        std::shared_ptr<ShaderProgram> getShaderProgram() const { return mShaderProgram; }

        // 检查是否包含特定阶段的着色器
        bool hasStage(VkShaderStageFlagBits stage) const;

    private:
        std::shared_ptr<ShaderProgram> mShaderProgram;
        std::unordered_map<VkShaderStageFlagBits, std::string> mStageNames;

        void updateCreateInfo();
    };
} // namespace StarryEngine