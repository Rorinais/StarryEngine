#include "Shader.hpp"
#include <shaderc/shaderc.hpp>
#include <fstream>
#include <sstream>
#include "../../../utils/FileUtils.hpp"
#include "../../../logging/Logger.hpp"

namespace StarryEngine::Assets {

    Shader::Shader(std::shared_ptr<RHI::ResourceManager> resMgr)
        : m_resMgr(resMgr) {
    }

    Shader::~Shader() {
        for (auto& [stage, handle] : m_modules) {
            if (handle.isValid()) {
                m_resMgr->destroy(handle);
            }
        }
    }

    Shader& Shader::addShader(const std::string& source, RHI::ShaderStage stage, const std::string& name) {
        auto loader = std::make_unique<ShaderLoader>(m_resMgr);
        auto output = loader->loadFromSource(source, stage, name);
        if (!output) {
            LOG_ERROR("Failed to load shader from source");
            return *this;  
        }

        m_modules[stage] = output->module;

        return *this;
    }

    Shader& Shader::addShader(const std::string& path, RHI::ShaderStage stage) {
        auto loader = std::make_unique<ShaderLoader>(m_resMgr);
        auto output = loader->loadFromFile(path, stage);
        if (!output) {
            LOG_ERROR("Failed to load shader from source");
            return *this;
        }

        m_modules[stage] = output->module;
        return *this;
    }

} // namespace StarryEngine::Assets