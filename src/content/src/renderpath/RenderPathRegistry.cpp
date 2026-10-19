#include <content/renderpath/RenderPathRegistry.hpp>
#include <renderer/renderPaths/RayTracingRenderPath.hpp>
#include <renderer/renderPaths/LayerRegistry.hpp>
#include <renderer/passes/ShadowPass.hpp>
#include <renderer/passes/MeshPass.hpp>
#include <renderer/passes/ParticlePass.hpp>
#include <renderer/passes/GraphicsPass.hpp>
#include <renderer/passExecutor/SceneDrawExecutor.hpp>
#include <logging/Logger.hpp>

namespace StarryEngine::Content {

    RenderPathRegistry& RenderPathRegistry::instance() {
        static RenderPathRegistry registry;
        return registry;
    }

    RenderPathRegistry::RenderPathRegistry() {
        // 内置登记（不创建）：延迟
        m_factories["deferred"] =
            [](std::shared_ptr<RHI::IRHI> rhi, uint32_t w, uint32_t h, const PathParams& p) {
                StandardPipeline::declare(w, h);   // 声明标准管线：层→pass(RenderLayers) + 附件(RenderTextures)
                return std::make_shared<DeferredRenderPath>(rhi, w, h, p.deferred);
            };
        // 内置登记（不创建）：光追
        m_factories["raytrace"] =
            [](std::shared_ptr<RHI::IRHI> rhi, uint32_t w, uint32_t h, const PathParams& p) {
                RayTracingRenderPath::Config cfg;
                cfg.spp = p.spp;
                cfg.denoise = p.denoise;
                cfg.offline = p.offline;
                return std::make_shared<RayTracingRenderPath>(rhi, w, h, cfg);
            };
    }

    void RenderPathRegistry::registerPath(const std::string& name, PathFactory factory, bool overwrite) {
        if (name.empty() || !factory) return;
        auto it = m_factories.find(name);
        if (it != m_factories.end() && !overwrite) {
            LOG_WARN("[path] 路径 '{}' 已存在，使用 overwrite=true 才能替换", name);
            return;
        }
        m_factories[name] = std::move(factory);
        LOG_INFO("[path] 已注册渲染路径: {}", name);
    }

    std::shared_ptr<BaseRenderPath> RenderPathRegistry::create(const std::string& name,
        std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height,
        const PathParams& params) {
        auto it = m_factories.find(name);
        if (it == m_factories.end()) {
            LOG_ERROR("[path] 未注册的渲染路径: {}（可用: {}）", name, [this] {
                std::string s; for (auto& kv : m_factories) { if (!s.empty()) s += ", "; s += kv.first; } return s; }());
            return nullptr;
        }
        return it->second(std::move(rhi), width, height, params);
    }

    std::vector<std::string> RenderPathRegistry::names() const {
        std::vector<std::string> out;
        out.reserve(m_factories.size());
        for (const auto& kv : m_factories) out.push_back(kv.first);
        return out;
    }

} // namespace StarryEngine::Content
