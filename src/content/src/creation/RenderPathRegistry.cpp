#include <content/creation/RenderPathRegistry.hpp>
#include <renderer/renderPaths/RayTracingRenderPath.hpp>
#include <renderer/renderPaths/LayerRegistry.hpp>
#include <renderer/passes/ShadowPass.hpp>
#include <renderer/passes/MeshPass.hpp>
#include <renderer/passes/ParticlePass.hpp>
#include <renderer/passes/GraphicsPass.hpp>
#include <renderer/passExecutor/SceneDrawExecutor.hpp>
#include <logging/Logger.hpp>

namespace StarryEngine::Content {

    namespace {
        // ── 标准延迟管线的默认 pass（层 → pass）──
        void addShadow(PassBuildContext& c) {
            c.setSlot(RenderLayers::Background,
                std::make_shared<ShadowPass>("ShadowPass"), "ShadowPass");
        }
        void addForward(PassBuildContext& c) {
            auto pass = std::make_shared<MeshPass>("ForwardPass", "Forward_Opaque", RHI::Color::Transparent());
            pass->addReadTextureByName("ShadowMap");
            c.setSlot(RenderLayers::Opaque, pass, "Forward_Opaque");
        }
        void addSkybox(PassBuildContext& c) {
            auto post = c.getGroupPass ? c.getGroupPass("PostProcessPass") : nullptr;
            if (!post) return;
            SubpassDesc sky;
            sky.tag = "PostProcess_Skybox"; sky.layer = RenderLayers::Surface; sky.queue = 0;
            sky.executor = std::make_shared<SceneDrawExecutor>();
            sky.colorAttachments.push_back({ "SceneColor" });
            sky.depthAttachment = { "Depth" };
            post->addSubpass(sky);
            c.setSlot(RenderLayers::Surface, post, "PostProcess");
        }
        void addGrid(PassBuildContext& c) {
            auto post = c.getGroupPass ? c.getGroupPass("PostProcessPass") : nullptr;
            if (!post) return;
            SubpassDesc grid;
            grid.tag = "PostProcess_Grid"; grid.layer = RenderLayers::Surface; grid.queue = 1;
            grid.executor = std::make_shared<SceneDrawExecutor>();
            grid.colorAttachments.push_back({ "SceneColor" });
            grid.depthAttachment = { "Depth" };
            post->addSubpass(grid);
            c.setSlot(RenderLayers::Surface, post, "PostProcess");
        }
        void addStencil(PassBuildContext& c) {
            auto pass = std::make_shared<GraphicsPass>("StencilPass");
            RenderGraph::AttachmentParams ds;
            ds.clearDepth = 1.0f;
            ds.clearStencil = 0;

            SubpassDesc sw;
            sw.tag = "StencilWrite"; sw.layer = RenderLayers::Outline; sw.queue = 0;
            sw.executor = std::make_shared<SceneDrawExecutor>();
            sw.colorAttachments.push_back({ "SceneColor", RenderGraph::AttachmentParams{} });
            sw.depthAttachment = { "Depth", ds };
            pass->addSubpass(sw);

            SubpassDesc st;
            st.tag = "StencilTest"; st.layer = RenderLayers::Outline; st.queue = 1;
            st.executor = std::make_shared<SceneDrawExecutor>();
            st.colorAttachments.push_back({ "SceneColor", RenderGraph::AttachmentParams{} });
            st.depthAttachment = { "Depth", ds };
            pass->addSubpass(st);

            c.setSlot(RenderLayers::Outline, pass, "Stencil");
        }
        void addParticles(PassBuildContext& c) {
            c.setSlot(RenderLayers::Particles,
                std::make_shared<ParticlePass>("Particles", "Particles"), "Particles");
        }
    } // namespace

    void registerStandardPasses() {
        auto& layers = RenderLayers::instance();
        if (layers.passOf(RenderLayers::Background)) return;   // 已注册(幂等)
        layers.setPass(RenderLayers::Background, &addShadow);
        layers.setPass(RenderLayers::Opaque,     &addForward);
        layers.setPass(RenderLayers::Surface,    [](PassBuildContext& c) { addSkybox(c); addGrid(c); });
        layers.setPass(RenderLayers::Outline,    &addStencil);
        layers.setPass(RenderLayers::Particles,  &addParticles);
        LOG_INFO("[path] 标准 pass 已注册到系统层(0-4)");
    }

    RenderPathRegistry& RenderPathRegistry::instance() {
        static RenderPathRegistry registry;
        return registry;
    }

    RenderPathRegistry::RenderPathRegistry() {
        // 内置登记（不创建）：延迟
        m_factories["deferred"] =
            [](std::shared_ptr<RHI::IRHI> rhi, uint32_t w, uint32_t h, const PathParams& p) {
                registerStandardPasses();   // 默认 pass 创建在内容层
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
