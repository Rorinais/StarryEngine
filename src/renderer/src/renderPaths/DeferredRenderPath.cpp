#include <renderer/renderPaths/DeferredRenderPath.hpp>
#include <renderer/renderPaths/LayerRegistry.hpp>
#include <renderer/renderPaths/TextureRegistry.hpp>
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
        buildDefaultPipeline();   // 构造即挂 pass / 声明纹理（建图在 initialize）
    }

    // ── 内建标准 PBR 管线：填充默认槽位（demo 无需手拼 PassList）──
    void DeferredRenderPath::buildDefaultPipeline() {
        auto& textures = RenderTextures::instance();
        for (auto& [name, desc] : textures.all()) {
            if (!m_textureDescs.count(name)) addTextureDesc(name, desc);
        }


        PassBuildContext ctx;
        ctx.width = m_width;
        ctx.height = m_height;
        // 约定：一层 = 一个 render pass（同层重复挂载为替换）；层内多段用 subpass 的 queue 区分
        ctx.setSlot = [this](int layer, std::shared_ptr<IPass> pass) {
            setDefaultSlot(layer, std::move(pass));
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
                if (info.layer >= 0) {
                    const long long key = (static_cast<long long>(info.layer) << 32) | (info.queue & 0xFFFFFFFFLL);
                    auto it = m_keyToSubpass.find(key);
                    if (it != m_keyToSubpass.end()) {
                        LOG_ERROR("[path] (layer={}, queue={}) 已被另一个 subpass 占用，请换 queue",info.layer, info.queue);
                        continue;
                    }
                    m_keyToSubpass[key] = SubpassTarget{{}, info.subpassIndex, info.executor};
                    m_keyToPassNode[key] = info.passNode;
                } else {
                    LOG_DEBUG("[path] subpass (layer={}, queue={}) 未声明 layer，跳过路由注册", info.layer, info.queue);
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

        for (auto& item : sceneData.drawItems) {
            // 未声明层的材质默认放到 (Opaque, 0) —— 不再依赖字符串默认 tag
            if (item->renderLayer < 0) {
                item->renderLayer = RenderLayers::Opaque;
                item->renderQueue = 0;
            }
            const long long key = (static_cast<long long>(item->renderLayer) << 32) | (item->renderQueue & 0xFFFFFFFFLL);
            if (!m_keyToSubpass.count(key)) {
                LOG_ERROR("材质 (layer={}, queue={}) 没有对应 subpass", item->renderLayer, item->renderQueue);
            }
        }

        for (auto& pass : m_passes)
            pass->onSceneData(sceneData, ctx);
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
            auto it = m_keyToSubpass.find(presentKey());
            if (it != m_keyToSubpass.end()) {
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
