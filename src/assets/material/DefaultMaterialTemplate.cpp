#include "DefaultMaterialTemplate.hpp"
#include "../../assets/loader/ShaderLoader.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine::Assets {
    DefaultMaterialTemplate::DefaultMaterialTemplate(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        const std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle>& layouts,
        const std::vector<RHI::PushConstantRange>& pushConstants)
        : m_resMgr(resMgr), m_layouts(layouts), m_pushConstants(pushConstants) {}

    bool DefaultMaterialTemplate::loadShaders(const std::string& vsPath, const std::string& fsPath) {
        Assets::ShaderLoader loader(m_resMgr);
        auto vert = loader.loadFromFile(vsPath, RHI::ShaderStage::Vertex);
        auto frag = loader.loadFromFile(fsPath, RHI::ShaderStage::Fragment);

        if (vert) {
            m_vertexShader = vert->module;
        }else{
            return false;
        }
        if (frag) {
            m_fragmentShader = frag->module;
        }
        else {
            return false;
        }
        return true;
    }
}