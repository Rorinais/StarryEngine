#pragma once
#include "../../../base.hpp"
#include "../../../renderer/backends/vulkan/vulkanCore/VulkanCore.hpp"
#include <shaderc/shaderc.hpp>
namespace StarryEngine {
    class ShaderUtils {
    public:
        using Ptr = std::shared_ptr<ShaderUtils>;
        static Ptr create(const LogicalDevice::Ptr& logicalDevice) {
            return std::make_shared<ShaderUtils>(logicalDevice);
        }

        ShaderUtils(const LogicalDevice::Ptr& logicalDevice);

        // 编译GLSL为SPIR-V（支持动态宏定义）
        VkShaderModule loadFromGLSL(
            const std::string& filename,
            VkShaderStageFlagBits stage,  // 改用Vulkan原生类型
            const std::vector<std::pair<std::string, std::string>>& macros = {},
            const std::string& debugName = ""
        );

        // 直接加载SPIR-V文件
        VkShaderModule loadFromSPV(
            const std::string& filename,
            const std::string& debugName = ""
        );

        VkShaderModule loadFromGLSLString(
            const std::string& sourceCode,
            VkShaderStageFlagBits stage,
            const std::vector<std::pair<std::string, std::string>>& macros = {},
            const std::string& debugName = ""
        );

        // 文件读取工具
        static std::string readTextFile(const std::string& filename);
        static std::vector<uint32_t> readBinaryFile(const std::string& filename);

    private:
        // 编译GLSL核心逻辑（每次编译使用独立的CompileOptions）
        std::vector<uint32_t> compileGLSL(
            const std::string& source,
            shaderc_shader_kind kind,
            const std::vector<std::pair<std::string, std::string>>& macros,
            const std::string& debugName
        );

        // 创建ShaderModule
        VkShaderModule createShaderModule(
            const std::vector<uint32_t>& code,
            const std::string& debugName
        );

        // SPIR-V验证
        static void validateSPIRV(const std::vector<uint32_t>& code);

    private:
        LogicalDevice::Ptr mLogicalDevice;
        shaderc::Compiler mCompiler;
    };
}
