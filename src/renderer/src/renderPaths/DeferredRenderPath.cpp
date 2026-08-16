#include <renderer/renderPaths/DeferredRenderPath.hpp>
#include <renderer/passes/GraphicsPass.hpp>
#include <renderer/passes/ParticlePass.hpp>
#include <renderer/passExecutor/ParticleRenderExecutor.hpp>
#include <logging/Logger.hpp>
#include <ui/ImGuiManager.hpp>
#include <assets/loader/ShaderLoader.hpp>
#include <algorithm>
#include <unordered_set>

namespace StarryEngine {

    // ──── DeferredRenderPath ──────────────────────────────────────

    DeferredRenderPath::DeferredRenderPath(std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height)
        : BaseRenderPath(rhi, width, height) {}

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
        for (auto& pass : m_passes) {
            // 数据源自动注入：黑板（含场景等），pass 里按类型读取（demo 无需手动注入）
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

        // 校验：emitter 的路由 tag 是否命中某个粒子 pass（否则被静默丢弃——给提示）
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

        // draw item 分发 + 场景相关管线全部委托给各 pass（GraphicsPass 自己处理网格）
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

        // 诊断：确保每个 draw item 的 tag 都能命中某个 subpass
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
