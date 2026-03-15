#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include"../../AssetType.hpp"
#include"../../loader/ShaderLoader.hpp"

namespace StarryEngine::Assets {

    class Shader {
    public:
        Shader(std::shared_ptr<RHI::ResourceManager> resMgr);
        ~Shader();

        Shader& addShader(const std::string& source,RHI::ShaderStage stage,const std::string& name = "");
        Shader& addShader(const std::string& path,RHI::ShaderStage stage);

        RHI::ShaderHandle getShader(RHI::ShaderStage stage) { return m_modules[stage];  }

    private:
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::unordered_map<RHI::ShaderStage, RHI::ShaderHandle> m_modules;
    };

} // namespace StarryEngine::Assets