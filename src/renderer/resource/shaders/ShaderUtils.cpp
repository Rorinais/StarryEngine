#include"shaderUtils.hpp"
namespace StarryEngine {
    ShaderUtils::ShaderUtils(const LogicalDevice::Ptr& logicalDevice)
        : mLogicalDevice(logicalDevice) {
    }

    VkShaderModule ShaderUtils::loadFromGLSL(
        const std::string& filename,
        VkShaderStageFlagBits stage,
        const std::vector<std::pair<std::string, std::string>>& macros,
        const std::string& debugName
    ) {
        const std::string source = readTextFile(filename);
        shaderc_shader_kind kind;

        switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:   kind = shaderc_vertex_shader; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: kind = shaderc_fragment_shader; break;
        case VK_SHADER_STAGE_COMPUTE_BIT:  kind = shaderc_compute_shader; break;
        default:
            throw std::runtime_error("Unsupported shader stage");
        }

        auto spirv = compileGLSL(source, kind, macros, debugName);
        return createShaderModule(spirv, debugName);
    }

    VkShaderModule ShaderUtils::loadFromSPV(
        const std::string& filename,
        const std::string& debugName
    ) {
        auto spirv = readBinaryFile(filename);
        validateSPIRV(spirv);
        return createShaderModule(spirv, debugName);
    }

    // 在shaderUtils.cpp中实现

    VkShaderModule ShaderUtils::loadFromGLSLString(
        const std::string& sourceCode,
        VkShaderStageFlagBits stage,
        const std::vector<std::pair<std::string, std::string>>& macros,
        const std::string& debugName
    ) {
        shaderc_shader_kind kind;
        switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:   kind = shaderc_vertex_shader; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: kind = shaderc_fragment_shader; break;
        case VK_SHADER_STAGE_COMPUTE_BIT:  kind = shaderc_compute_shader; break;
        case VK_SHADER_STAGE_GEOMETRY_BIT: kind = shaderc_geometry_shader; break;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: kind = shaderc_tess_control_shader; break;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: kind = shaderc_tess_evaluation_shader; break;
        default:
            throw std::runtime_error("Unsupported shader stage");
        }

        auto spirv = compileGLSL(sourceCode, kind, macros, debugName);
        return createShaderModule(spirv, debugName);
    }

    std::vector<uint32_t> ShaderUtils::compileGLSL(
        const std::string& source,
        shaderc_shader_kind kind,
        const std::vector<std::pair<std::string, std::string>>& macros,
        const std::string& debugName
    ) {
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

    VkShaderModule ShaderUtils::createShaderModule(
        const std::vector<uint32_t>& code,
        const std::string& debugName
    ) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size() * sizeof(uint32_t);
        createInfo.pCode = code.data();

        VkShaderModule module;
        if (vkCreateShaderModule(mLogicalDevice->getHandle(), &createInfo, nullptr, &module) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module: " + debugName);
        }
        return module;
    }
    std::string ShaderUtils::readTextFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open GLSL file: " + filename);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::vector<uint32_t> ShaderUtils::readBinaryFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) {
            throw std::runtime_error("Failed to open SPIR-V file: " + filename);
        }

        const size_t fileSize = static_cast<size_t>(file.tellg());
        if (fileSize % sizeof(uint32_t) != 0) {
            throw std::runtime_error("SPIR-V file size is not 4-byte aligned: " + filename);
        }

        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

        if (!file) {
            throw std::runtime_error("Failed to read entire SPIR-V file: " + filename);
        }

        return buffer;
    }

    void ShaderUtils::validateSPIRV(const std::vector<uint32_t>& code) {
        if (code.empty()) {
            throw std::runtime_error("Empty SPIR-V code");
        }

        constexpr uint32_t SPIRV_MAGIC = 0x07230203;
        if (code[0] != SPIRV_MAGIC) {
            throw std::runtime_error("Invalid SPIR-V magic number");
        }
    }
}