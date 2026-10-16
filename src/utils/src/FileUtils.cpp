#include <utils/FileUtils.hpp>
#include <shaderc/shaderc.hpp>
#include <cstring>

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

    // ── shaderc #include 解析器 ──
    // 相对 include（#include "x.glsl"）按"包含者所在目录"解析；找不到再退回工作目录。
    // shaderc 只在 shader 声明 GL_GOOGLE_include_directive 时调用本解析器。
    class GlslIncluder : public shaderc::CompileOptions::IncluderInterface {
        // 生命周期 owner：source_name / content 字符串须存活到 ReleaseInclude
        struct Owner {
            std::string sourceName;
            std::string content;
        };

    public:
        shaderc_include_result* GetInclude(const char* requested,
                                           shaderc_include_type /*type*/,
                                           const char* requesting,
                                           size_t /*include_depth*/) override {
            auto* owner = new Owner();
            auto* result = new shaderc_include_result{};

            // 解析包含者目录（requesting 形如 "assets/shaders/npr_toon/hair.frag"）
            std::string reqStr = requesting ? requesting : "";
            std::string reqDir;
            auto slash = reqStr.find_last_of("/\\");
            if (slash != std::string::npos) reqDir = reqStr.substr(0, slash + 1);

            // 优先：包含者目录下的相对路径
            std::string path = reqDir + requested;
            owner->content = readFileSafe(path);
            if (owner->content.empty()) {
                owner->content = readFileSafe(requested);   // 回退工作目录
                owner->sourceName = requested;
            } else {
                owner->sourceName = path;
            }

            result->source_name = owner->sourceName.c_str();
            result->source_name_length = owner->sourceName.size();
            result->content = owner->content.c_str();
            result->content_length = owner->content.size();
            result->user_data = owner;
            return result;
        }

        void ReleaseInclude(shaderc_include_result* data) override {
            if (data) {
                delete static_cast<Owner*>(data->user_data);
                delete data;
            }
        }

    private:
        static std::string readFileSafe(const std::string& path) {
            std::ifstream f(path);
            if (!f.is_open()) return {};
            std::stringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }
    };

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
        // 启用 #include（按包含者目录解析相对路径）
        options.SetIncluder(std::make_unique<GlslIncluder>());

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

} // namespace StarryEngine::Utils
