#include "ShaderStageComponent.hpp"

namespace StarryEngine {

    ShaderStageComponent::ShaderStageComponent(const std::string& name) {
        setName(name);
        reset();
    }

    ShaderStageComponent& ShaderStageComponent::reset() {
        mShaderProgram.reset();
        mStageNames.clear();
        return *this;
    }

    ShaderStageComponent& ShaderStageComponent::setShaderProgram(std::shared_ptr<ShaderProgram> program) {
        mShaderProgram = program;
        return *this;
    }

    ShaderStageComponent& ShaderStageComponent::addVertexShader(
        const std::string& filename,
        const std::string& entryPoint,
        const std::vector<std::pair<std::string, std::string>>& macros,
        const std::string& debugName) {

        if (!mShaderProgram) {
            // 需要延迟创建，在构建时才需要LogicalDevice
            throw std::runtime_error("Shader program not initialized. Call setShaderProgram() first.");
        }

        mShaderProgram->addGLSLStage(filename, VK_SHADER_STAGE_VERTEX_BIT,
            entryPoint.c_str(), macros, debugName);
        mStageNames[VK_SHADER_STAGE_VERTEX_BIT] = debugName.empty() ? filename : debugName;
        return *this;
    }

    ShaderStageComponent& ShaderStageComponent::addFragmentShader(
        const std::string& filename,
        const std::string& entryPoint,
        const std::vector<std::pair<std::string, std::string>>& macros,
        const std::string& debugName) {

        if (!mShaderProgram) {
            throw std::runtime_error("Shader program not initialized. Call setShaderProgram() first.");
        }

        mShaderProgram->addGLSLStage(filename, VK_SHADER_STAGE_FRAGMENT_BIT,
            entryPoint.c_str(), macros, debugName);
        mStageNames[VK_SHADER_STAGE_FRAGMENT_BIT] = debugName.empty() ? filename : debugName;
        return *this;
    }

    ShaderStageComponent& ShaderStageComponent::addVertexShaderFromString(
        const std::string& sourceCode,
        const std::string& entryPoint,
        const std::vector<std::pair<std::string, std::string>>& macros,
        const std::string& debugName) {

        if (!mShaderProgram) {
            throw std::runtime_error("Shader program not initialized. Call setShaderProgram() first.");
        }

        mShaderProgram->addGLSLStringStage(sourceCode, VK_SHADER_STAGE_VERTEX_BIT,
            entryPoint.c_str(), macros, debugName);
        mStageNames[VK_SHADER_STAGE_VERTEX_BIT] = debugName.empty() ? "VertexShader" : debugName;
        return *this;
    }

    ShaderStageComponent& ShaderStageComponent::addFragmentShaderFromString(
        const std::string& sourceCode,
        const std::string& entryPoint,
        const std::vector<std::pair<std::string, std::string>>& macros,
        const std::string& debugName) {

        if (!mShaderProgram) {
            throw std::runtime_error("Shader program not initialized. Call setShaderProgram() first.");
        }

        mShaderProgram->addGLSLStringStage(sourceCode, VK_SHADER_STAGE_FRAGMENT_BIT,
            entryPoint.c_str(), macros, debugName);
        mStageNames[VK_SHADER_STAGE_FRAGMENT_BIT] = debugName.empty() ? "FragmentShader" : debugName;
        return *this;
    }

    ShaderStageComponent& ShaderStageComponent::addShaderFromBuilder(
        std::shared_ptr<ShaderBuilder> builder,
        VkShaderStageFlagBits stage,
        const std::string& entryPoint,
        const std::vector<std::pair<std::string, std::string>>& macros,
        const std::string& debugName) {

        if (!mShaderProgram) {
            throw std::runtime_error("Shader program not initialized. Call setShaderProgram() first.");
        }

        std::string source = builder->getSource();
        mShaderProgram->addGLSLStringStage(source, stage, entryPoint.c_str(), macros, debugName);

        std::string stageName;
        switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT: stageName = "Vertex"; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: stageName = "Fragment"; break;
        case VK_SHADER_STAGE_GEOMETRY_BIT: stageName = "Geometry"; break;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: stageName = "TessControl"; break;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: stageName = "TessEval"; break;
        case VK_SHADER_STAGE_COMPUTE_BIT: stageName = "Compute"; break;
        default: stageName = "Unknown"; break;
        }

        mStageNames[stage] = debugName.empty() ? stageName + "Shader" : debugName;
        return *this;
    }

    std::string ShaderStageComponent::getDescription() const {
        if (!mShaderProgram || mShaderProgram->getStages().empty()) {
            return "Shader Stage: No shaders";
        }

        std::string desc = "Shader Stages: ";
        const auto& stages = mShaderProgram->getStages();

        for (size_t i = 0; i < stages.size(); ++i) {
            if (i > 0) desc += ", ";

            std::string stageName;
            switch (stages[i].stage) {
            case VK_SHADER_STAGE_VERTEX_BIT: stageName = "Vertex"; break;
            case VK_SHADER_STAGE_FRAGMENT_BIT: stageName = "Fragment"; break;
            case VK_SHADER_STAGE_GEOMETRY_BIT: stageName = "Geometry"; break;
            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: stageName = "TessControl"; break;
            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: stageName = "TessEval"; break;
            case VK_SHADER_STAGE_COMPUTE_BIT: stageName = "Compute"; break;
            default: stageName = "Unknown"; break;
            }

            auto it = mStageNames.find(stages[i].stage);
            if (it != mStageNames.end()) {
                desc += stageName + "[" + it->second + "]";
            }
            else {
                desc += stageName;
            }
        }

        return desc;
    }

    bool ShaderStageComponent::isValid() const {
        if (!mShaderProgram) {
            return false;
        }

        const auto& stages = mShaderProgram->getStages();
        if (stages.empty()) {
            return false;
        }

        // 检查是否至少有一个顶点着色器
        bool hasVertexShader = false;
        bool hasFragmentShader = false;

        for (const auto& stage : stages) {
            if (stage.stage == VK_SHADER_STAGE_VERTEX_BIT) {
                hasVertexShader = true;
            }
            else if (stage.stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
                hasFragmentShader = true;
            }

            // 检查着色器模块是否有效
            if (stage.module == VK_NULL_HANDLE) {
                return false;
            }
        }

        // 对于图形管线，顶点着色器是必须的
        if (!hasVertexShader) {
            return false;
        }

        return true;
    }

    bool ShaderStageComponent::hasStage(VkShaderStageFlagBits stage) const {
        if (!mShaderProgram) {
            return false;
        }

        const auto& stages = mShaderProgram->getStages();
        for (const auto& s : stages) {
            if (s.stage == stage) {
                return true;
            }
        }
        return false;
    }

    void ShaderStageComponent::updateCreateInfo() {
        // 这个组件不需要更新创建信息，因为直接使用ShaderProgram
    }

} // namespace StarryEngine