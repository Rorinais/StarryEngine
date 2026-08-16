#pragma once
#include <memory>
#include <string>
#include <vector>
#include <mutex> 
#include <unordered_map>
#include <renderer/interface/RHIManager.hpp>
#include <assets/AssetType.hpp>
#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_glsl.hpp>

namespace StarryEngine::Assets {

    class ShaderLoader {
    public:
        ShaderLoader(std::shared_ptr<RHI::ResourceManager> resMgr);
        ~ShaderLoader() = default;

        std::optional<ShaderCreateInfo> loadFromFile(const std::string& path, RHI::ShaderStage stage,const std::unordered_map<std::string, std::string>& macros = {});

        std::optional<ShaderCreateInfo> loadFromSource(const std::string& source,RHI::ShaderStage stage,const std::string& name = "",const std::unordered_map<std::string, std::string>& macros = {});

        void clearCache();

    private:
        std::shared_ptr<RHI::ResourceManager> m_resMgr;

        std::vector<uint32_t> compileToSpirv(const std::string& source,RHI::ShaderStage stage,const std::string& name,const std::unordered_map<std::string, std::string>& macros = {});

        bool reflectAndCreateLayouts(const std::vector<uint32_t>& spirv,ShaderCreateInfo& outInfo);
        void fillTextureInfo(const spirv_cross::SPIRType& type,RHI::ResourceBinding::TextureInfo& info);

        void flattenUBOMembers(
            const spirv_cross::CompilerGLSL& compiler,
            const spirv_cross::SPIRType& type,
            uint32_t baseOffset,
            const std::string& baseName,
            std::vector<RHI::BufferMember>& flatMembers,bool isStorageBuffer = false);

        RHI::Format spirvImageFormatToRHI(spv::ImageFormat fmt);

        RHI::ShaderStage getShaderStageFromSpirv(const spirv_cross::Compiler& compiler) const;
        RHI::Format spirvTypeToFormat(const spirv_cross::SPIRType& type) const;

        static uint32_t getTypeSize(const spirv_cross::SPIRType& type);

        size_t computeHash(const std::string& source, RHI::ShaderStage stage) const;
        mutable std::mutex m_cacheMutex;                         
        std::unordered_map<size_t, ShaderCreateInfo> m_cache;
    };

} // namespace StarryEngine::Assets