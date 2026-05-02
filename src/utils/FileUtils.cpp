#include"FileUtils.hpp"

namespace StarryEngine::Utils {
    std::string FileUtils::readTextFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open GLSL file: " + filename);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::vector<uint32_t> FileUtils::readBinaryFile(const std::string& filename) {
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

    bool FileUtils::fileExists(const std::string& filepath) {
        std::ifstream file(filepath);
        return file.good();
    }

    std::vector<uint32_t> FileUtils::compileGlslToSpirv(
        const std::string& source,
        const std::string& name,
        shaderc_shader_kind kind,
        const std::unordered_map<std::string, std::string>& macros)
    {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        options.SetOptimizationLevel(shaderc_optimization_level_performance);
        options.SetGenerateDebugInfo();

        // 注入变体宏
        for (const auto& [macro, value] : macros) {
            options.AddMacroDefinition(macro, value);
        }

        auto result = compiler.CompileGlslToSpv(source, kind, name.c_str(), options);
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            throw std::runtime_error("Shader compilation failed: " + result.GetErrorMessage());
        }
        return { result.cbegin(), result.cend() };
    }
}