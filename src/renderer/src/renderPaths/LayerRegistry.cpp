#include <renderer/renderPaths/LayerRegistry.hpp>
#include <logging/Logger.hpp>

namespace StarryEngine {

    RenderLayers& RenderLayers::instance() {
        static RenderLayers layers;
        return layers;
    }

    const char* RenderLayers::builtinName(int layer) {
        switch (layer) {
            case Background: return "Background";
            case Opaque:     return "Opaque";
            case Surface:    return "Surface";
            case Outline:    return "Outline";
            case Particles:  return "Particles";
            default:         return "";
        }
    }

    RenderLayers::RenderLayers() {
        // 层名系统固定；pass 工厂由内容层注册（见 content/creation/RenderPathRegistry.cpp）
        for (int i = 0; i < LayerCount; ++i) m_names[i] = builtinName(i);
    }

    bool RenderLayers::setUserLayer(int index, const std::string& name) {
        if (index < FirstUser || index >= LayerCount || name.empty()) {
            LOG_WARN("[layer] 非法自定义层: index={}（用户层范围 {}-{}），name='{}'",
                     index, static_cast<int>(FirstUser), static_cast<int>(LayerCount) - 1, name);
            return false;
        }
        m_names[index] = name;
        LOG_INFO("[layer] 自定义层已设置: {} = '{}'", index, name);
        return true;
    }

    int RenderLayers::addUserLayer(const std::string& name) {
        if (name.empty()) return -1;
        int existing = find(name);
        if (existing >= 0) return existing;
        for (int i = FirstUser; i < LayerCount; ++i) {
            if (m_names[i].empty()) {
                m_names[i] = name;
                LOG_INFO("[layer] 自定义层已注册: {} = '{}'", i, name);
                return i;
            }
        }
        LOG_WARN("[layer] 用户层已满（{} 个），无法注册 '{}'", LayerCount - FirstUser, name);
        return -1;
    }

    int RenderLayers::find(const std::string& name) const {
        for (int i = 0; i < LayerCount; ++i) {
            if (m_names[i] == name) return i;
        }
        return -1;
    }

    std::string RenderLayers::nameOf(int index) const {
        if (index < 0 || index >= LayerCount) return {};
        return m_names[index];
    }

    std::vector<std::pair<int, std::string>> RenderLayers::all() const {
        std::vector<std::pair<int, std::string>> out;
        for (int i = 0; i < LayerCount; ++i) {
            if (!m_names[i].empty()) out.emplace_back(i, m_names[i]);
        }
        return out;
    }

    void RenderLayers::setPass(int layer, PassFactory factory, bool overwrite) {
        if (layer < 0 || layer >= LayerCount || !factory) return;
        if (m_passes[layer] && !overwrite) {
            LOG_WARN("[layer] 层 {} 已有 pass 工厂，使用 overwrite=true 才能替换", layer);
            return;
        }
        m_passes[layer] = std::move(factory);
        LOG_INFO("[layer] 层 {} 已注册 pass 工厂", layer);
    }

    const PassFactory* RenderLayers::passOf(int layer) const {
        if (layer < 0 || layer >= LayerCount || !m_passes[layer]) return nullptr;
        return &m_passes[layer];
    }

    std::vector<int> RenderLayers::layersWithPass() const {
        std::vector<int> out;
        for (int i = 0; i < LayerCount; ++i) {
            if (m_passes[i]) out.push_back(i);
        }
        return out;
    }

    void RenderLayers::build(PassBuildContext& ctx) {
        for (int i = 0; i < LayerCount; ++i) {
            if (!m_passes[i]) continue;
            m_passes[i](ctx);
        }
    }

} // namespace StarryEngine
