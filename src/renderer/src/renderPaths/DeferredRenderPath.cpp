#include <renderer/renderPaths/DeferredRenderPath.hpp>
#include <renderer/renderPaths/LayerRegistry.hpp>
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

        // 标准 PBR 管线的各段由 PassRegistry 映射表构建（默认表：shadow/forward/skybox/grid/stencil/particles）
        std::unordered_map<std::string, std::shared_ptr<GraphicsPass>> groups;
        auto slotTags = std::make_shared<std::unordered_set<std::string>>();

        PassBuildContext ctx;
        ctx.width = m_width;
        ctx.height = m_height;
        ctx.setSlot = [this, slotTags](int layer, std::shared_ptr<IPass> pass, const std::string& tag) {
            if (tag.empty() || slotTags->count(tag)) return;
            slotTags->insert(tag);
            setDefaultSlot(layer, std::move(pass), tag);
        };
        ctx.getGroupPass = [&groups](const std::string& name) -> std::shared_ptr<GraphicsPass> {
            auto it = groups.find(name);
            if (it != groups.end()) return it->second;
            auto pass = std::make_shared<GraphicsPass>(name);
            groups[name] = pass;
            return pass;
        };

        // 层 = pass 注册表：按层索引升序执行各层工厂
        auto& layers = RenderLayers::instance();
        if (m_cfg.layers.empty()) {
            layers.build(ctx);
        } else {
            for (int l : m_cfg.layers) {
                if (auto* f = layers.passOf(l)) (*f)(ctx);
            }
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

                // (layer,queue) 索引：同一 (layer,queue) 只允许一个 subpass
                if (info.layer >= 0) {
                    const long long key = (static_cast<long long>(info.layer) << 32) | (info.queue & 0xFFFFFFFFLL);
                    auto it = m_layerQueueToTag.find(key);
                    if (it != m_layerQueueToTag.end() && it->second != info.tag) {
                        LOG_ERROR("[path] (layer={}, queue={}) 已被 subpass '{}' 占用，'{}' 请换 queue",
                                  info.layer, info.queue, it->second, info.tag);
                    } else {
                        m_layerQueueToTag[key] = info.tag;
                    }
                }
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
            // 优先 (renderLayer, renderQueue) 路由；未声明层则沿用旧 tag
            if (item->renderLayer >= 0) {
                const long long key = (static_cast<long long>(item->renderLayer) << 32) | (item->renderQueue & 0xFFFFFFFFLL);
                auto it = m_layerQueueToTag.find(key);
                if (it != m_layerQueueToTag.end()) {
                    item->passTag = it->second;
                } else {
                    LOG_ERROR("材质 (layer={}, queue={}) 没有对应 subpass，退回 tag '{}'",
                              item->renderLayer, item->renderQueue, item->passTag);
                }
            }
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

    void DeferredRenderPath::onBeforeGraphDestroy() {
        if (m_imguiManager) {
            m_imguiManager->invalidateSceneTexture();
        }
    }

    void DeferredRenderPath::onAfterCompileImGui() {
        if (!m_imguiManager) return;
        m_imguiManager->setRenderGraph(m_renderGraph);
        if (!m_imguiManager->isVulkanReady()) {
            auto it = m_tagToSubpass.find("ImGui");
            if (it != m_tagToSubpass.end()) {
                // 动态渲染时 renderPass 为空（RenderNode）；ImGuiManager 会改走 dynamic rendering
                if (!m_imguiManager->initializeVulkanBackend(m_rhi.get(), m_resMgr.get(),
                    it->second.renderPass, m_imguiImageCount)) {
                    LOG_ERROR("Failed to initialize ImGui Vulkan backend");
                }
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
