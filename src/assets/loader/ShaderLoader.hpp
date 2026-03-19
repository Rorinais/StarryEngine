#pragma once
#include <memory>
#include <string>
#include <vector>
#include <mutex> 
#include <unordered_map>
#include "../../renderer/interface/RHI_RESOURCE_MANAGER.hpp"
#include "../AssetType.hpp"
#include <shaderc/shaderc.hpp>
#include "spirv_cross/spirv_glsl.hpp"

namespace StarryEngine::Assets {

    class ShaderLoader {
    public:
        ShaderLoader(std::shared_ptr<RHI::ResourceManager> resMgr);
        ~ShaderLoader() = default;

        // 从文件加载单个阶段，返回创建信息
        std::optional<ShaderCreateInfo> loadFromFile(const std::string& path, RHI::ShaderStage stage);

        // 从源码加载单个阶段
        std::optional<ShaderCreateInfo> loadFromSource(const std::string& source,
            RHI::ShaderStage stage,
            const std::string& name = "");

        void clearCache();

    private:
        std::shared_ptr<RHI::ResourceManager> m_resMgr;

        // 编译 GLSL 到 SPIR-V
        std::vector<uint32_t> compileToSpirv(const std::string& source,
            RHI::ShaderStage stage,
            const std::string& name);

        // 反射 SPIR-V，提取资源绑定和输入布局，并创建描述符集布局
        bool reflectAndCreateLayouts(const std::vector<uint32_t>& spirv,
            ShaderCreateInfo& outInfo);

        RHI::ShaderStage getShaderStageFromSpirv(const spirv_cross::Compiler& compiler) const;
        RHI::Format spirvTypeToFormat(const spirv_cross::SPIRType& type) const;

        size_t computeHash(const std::string& source, RHI::ShaderStage stage) const;
        mutable std::mutex m_cacheMutex;                         
        std::unordered_map<size_t, ShaderCreateInfo> m_cache;
    };

} // namespace StarryEngine::Assets