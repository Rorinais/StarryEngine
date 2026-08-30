#include <renderer/renderPaths/DeferredRenderPath.hpp>
#include <renderer/passes/GraphicsPass.hpp>
#include <renderer/passes/MeshPass.hpp>
#include <renderer/passes/ShadowPass.hpp>
#include <renderer/passes/ParticlePass.hpp>
#include <renderer/passes/PassWrapper.hpp>
#include <renderer/passExecutor/SceneDrawExecutor.hpp>
#include <renderer/passExecutor/ParticleRenderExecutor.hpp>
#include <logging/Logger.hpp>
#include <ui/ImGuiManager.hpp>
#include <assets/loader/ShaderLoader.hpp>
#include <algorithm>
#include <unordered_set>

namespace StarryEngine {

    // ──── DeferredRenderPath ──────────────────────────────────────

    DeferredRenderPath::DeferredRenderPath(std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height)
        : DeferredRenderPath(rhi, width, height, Config{}) {}

    DeferredRenderPath::DeferredRenderPath(std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height,
        const Config& cfg)
        : BaseRenderPath(rhi, width, height), m_cfg(cfg) {
        buildDefaultPipeline();
    }

    // ── 内建标准 PBR 管线：填充默认槽位（demo 无需手拼 PassList）──
    void DeferredRenderPath::buildDefaultPipeline() {
        auto colorDesc = PassWrapper::createColorTextureDesc({m_width, m_height, 1}, RHI::Format::RGBA16_Float);
        auto depthDesc = PassWrapper::createDepthTextureDesc({m_width, m_height, 1}, RHI::Format::D24_UNorm_S8_UInt);
        auto swapDesc   = PassWrapper::createColorTextureDesc({m_width, m_height, 1}, RHI::Format::BGRA8_sRGB);
        auto shadowMapDesc = PassWrapper::createDepthTextureDesc({ShadowPass::kShadowMapSize, ShadowPass::kShadowMapSize, 1}, RHI::Format::D24_UNorm_S8_UInt);
        addTextureDesc("SceneColor",  colorDesc);
        addTextureDesc("Depth",       depthDesc);
        addTextureDesc("Swapchain",   swapDesc);
        addTextureDesc("ShadowMap",   shadowMapDesc);

        if (m_cfg.shadows) {
            setDefaultSlot(RenderPassEvent::Shadow, std::make_shared<ShadowPass>("ShadowPass"), "ShadowPass");
        }

        auto forwardPass = std::make_shared<MeshPass>("ForwardPass", "Forward_Opaque", RHI::Color::Transparent());
        forwardPass->addReadTextureByName("ShadowMap");
        setDefaultSlot(RenderPassEvent::Opaque, forwardPass, "Forward_Opaque");

        if (m_cfg.skybox || m_cfg.grid) {
            auto postPass = std::make_shared<GraphicsPass>("PostProcessPass");
            if (m_cfg.skybox) {
                SubpassDesc sky;
                sky.tag = "PostProcess_Skybox";
                sky.executor = std::make_shared<SceneDrawExecutor>();
                sky.colorAttachments.push_back({"SceneColor"});
                sky.depthAttachment = {"Depth"};
                postPass->addSubpass(sky);
            }
            if (m_cfg.grid) {
                SubpassDesc grid;
                grid.tag = "PostProcess_Grid";
                grid.executor = std::make_shared<SceneDrawExecutor>();
                grid.colorAttachments.push_back({"SceneColor"});
                grid.depthAttachment = {"Depth"};
                postPass->addSubpass(grid);
            }
            setDefaultSlot(RenderPassEvent::Post, postPass, "PostProcess");
        }

        if (m_cfg.stencilOutline) {
            auto stencilPass = std::make_shared<GraphicsPass>("StencilPass");
            RenderGraph::AttachmentParams ds;
            ds.clearDepth = 1.0f;
            ds.clearStencil = 0;

            SubpassDesc sw;
            sw.tag = "StencilWrite";
            sw.executor = std::make_shared<SceneDrawExecutor>();
            sw.colorAttachments.push_back({ "SceneColor", RenderGraph::AttachmentParams{} });
            sw.depthAttachment = { "Depth", ds };
            stencilPass->addSubpass(sw);

            SubpassDesc st;
            st.tag = "StencilTest";
            st.executor = std::make_shared<SceneDrawExecutor>();
            st.colorAttachments.push_back({ "SceneColor", RenderGraph::AttachmentParams{} });
            st.depthAttachment = { "Depth", ds };
            stencilPass->addSubpass(st);

            setDefaultSlot(RenderPassEvent::Stencil, stencilPass, "Stencil");
        }

        if (m_cfg.particles) {
            setDefaultSlot(RenderPassEvent::Particles, std::make_shared<ParticlePass>("Particles", "Particles"), "Particles");
        }
    }

    void DeferredRenderPath::setConfig(const RenderPathConfig&) {}

    void DeferredRenderPath::setDrawItems(const AnalysisSceneResult& sceneData) {
        m_cachedSceneData = std::make_shared<AnalysisSceneResult>(sceneData);
    }

    void DeferredRenderPath::setImGuiManager(ImGuiManager* mgr, uint32_t imageCount) {
        m_imguiManager = mgr;
        m_imguiImageCount = imageCount;
    }

    // ──── Config Pass Building ────────────────────────────────────

    void DeferredRenderPath::buildConfigPasses(
        std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap)
    {
        // 槽位 → pass 列表（启用且有序）
        assemblePassList();

        for (auto& pass : m_passes) {
            pass->setDataProvider(&m_blackboard);

            if (!pass->configure(*m_renderGraph, texIdMap, m_width, m_height, m_resMgr, m_globalSetLayout)) {
                LOG_ERROR("Pass '{}' configuration failed — skipping", pass->getName());
                continue;
            }

            for (auto& info : pass->getSubpasses()) {
                m_tagToSubpass[info.tag] = SubpassTarget{{}, info.subpassIndex, info.executor};
                m_tagToPassNode[info.tag] = info.passNode;
            }
        }

        {
            std::unordered_set<std::string> particleTags;
            for (auto& pass : m_passes)
                if (auto* pp = dynamic_cast<ParticlePass*>(pass.get())) particleTags.insert(pp->getPassTag());

            auto* scene = m_blackboard.get<Scene::Scene*>() ? *m_blackboard.get<Scene::Scene*>() : nullptr;
            if (scene && !particleTags.empty()) {
                for (auto& em : scene->getParticleEmitters()) {
                    std::string tag = ParticlePass::resolveEmitterTag(*em);
                    if (!particleTags.count(tag)) {
                        LOG_WARN("[Particle] emitter '{}' 路由 tag '{}' 没匹配任何粒子 pass（现有 {} 个），将被丢弃",
                                 em->name, tag, particleTags.size());
                    }
                }
            }
        }

        if (m_imguiManager) m_imguiManager->setRenderGraph(nullptr);
    }

    // ──── Rebuild ─────────────────────────────────────────────────

    void DeferredRenderPath::doRebuildResources(const AnalysisSceneResult& sceneData) {
        updateMaterialTextures(sceneData);

        IPass::CompileContext ctx;
        ctx.resMgr = m_resMgr;
        ctx.rhi = m_rhi;
        ctx.renderGraph = m_renderGraph.get();
        ctx.globalDescSets = m_globalDescSets;
        ctx.globalSetLayout = m_globalSetLayout;

        std::string defaultTag;
        if (!m_passes.empty()) {
            auto subs = m_passes.front()->getSubpasses();
            if (!subs.empty()) defaultTag = subs.front().tag;
        }

        std::unordered_set<std::string> allTags;
        for (auto& pass : m_passes)
            for (auto& info : pass->getSubpasses())
                allTags.insert(info.tag);
        for (auto& item : sceneData.drawItems) {
            std::string tag = item->passTag.empty() ? defaultTag : item->passTag;
            if (!allTags.count(tag)) LOG_ERROR("No subpass for tag '{}'", tag);
        }

        for (auto& pass : m_passes)
            pass->onSceneData(sceneData, ctx, defaultTag);
    }

    void DeferredRenderPath::onAfterCompile() {
        IPass::CompileContext ctx;
        ctx.resMgr = m_resMgr;
        ctx.rhi = m_rhi;
        ctx.renderGraph = m_renderGraph.get();
        ctx.globalDescSets = m_globalDescSets;
        ctx.globalSetLayout = m_globalSetLayout;

        for (auto& pass : m_passes) {
            pass->onAfterCompile(ctx);
        }
    }

    void DeferredRenderPath::onAfterCompileImGui() {
        if (!m_imguiManager) return;
        m_imguiManager->setRenderGraph(m_renderGraph);
        if (!m_imguiManager->isVulkanReady()) {
            auto it = m_tagToSubpass.find("ImGui");
            if (it != m_tagToSubpass.end()) {
                m_imguiManager->initializeVulkanBackend(m_rhi.get(), m_resMgr.get(),
                    it->second.renderPass, m_imguiImageCount);
            }
        }
        m_imguiManager->setDefaultSampler(m_defaultSampler);
        m_imguiManager->registerSceneTexture(m_resMgr.get());
    }

    // ──── Material Texture Binding ────────────────────────────────
    void DeferredRenderPath::updateMaterialTextures(const AnalysisSceneResult& sceneData) {
        if (!m_renderGraph || m_textureIdMap.empty()) { LOG_WARN("RG not ready for textures"); return; }
        RHI::TextureHandle swapchainPhys;
        auto it = m_textureIdMap.find("Swapchain");
        if (it != m_textureIdMap.end()) swapchainPhys = m_renderGraph->getPhysicalTextureHandle(it->second);

        for (auto& material : sceneData.materials) {
            if (!material) continue;
            for (const auto& [texName, dep] : material->getTextureDependencies()) {
                if (texName == "Swapchain") { LOG_ERROR("Material depends on Swapchain!"); continue; }
                auto texIt = m_textureIdMap.find(texName);
                if (texIt == m_textureIdMap.end()) continue;
                auto phys = m_renderGraph->getPhysicalTextureHandle(texIt->second);
                if (!phys.isValid() || (swapchainPhys.isValid() && phys == swapchainPhys)) continue;
                switch (dep.type) {
                case Assets::ResourceDependencyType::Sampler:
                    material->setTexture(dep.set, dep.binding, phys, m_defaultSampler); break;
                case Assets::ResourceDependencyType::InputAttachment:
                    material->setInputAttachment(dep.set, dep.binding, phys, RHI::ImageLayout::ShaderReadOnly); break;
                }
            }
        }
    }

} // namespace StarryEngine
