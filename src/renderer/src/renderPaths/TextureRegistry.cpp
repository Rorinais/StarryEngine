#include <renderer/renderPaths/TextureRegistry.hpp>
#include <logging/Logger.hpp>

namespace StarryEngine {

    RenderTextures& RenderTextures::instance() {
        static RenderTextures textures;
        return textures;
    }

    void RenderTextures::define(const std::string& name, RHI::TextureDesc desc, bool overwrite) {
        if (name.empty()) return;
        auto it = m_textures.find(name);
        if (it != m_textures.end() && !overwrite) return;   // 已存在则保留（幂等）
        m_textures[name] = std::move(desc);
    }

    bool RenderTextures::has(const std::string& name) const {
        return m_textures.count(name) != 0;
    }

    RHI::TextureDesc RenderTextures::get(const std::string& name) const {
        auto it = m_textures.find(name);
        return it == m_textures.end() ? RHI::TextureDesc{} : it->second;
    }

    std::unordered_map<std::string, RHI::TextureDesc> RenderTextures::all() const {
        return m_textures;
    }

    void RenderTextures::clear() { m_textures.clear(); }

} // namespace StarryEngine
