#include "DeferredRenderPath.hpp"
#include "../passes/GraphicsPass.hpp"
#include "../passExecutor/CopyToSwapchainExecutor.hpp"
#include "../passExecutor/ParticleCSExecutor.hpp"
#include "../passExecutor/ParticleRenderExecutor.hpp"
#include "../../logging/Logger.hpp"
#include "../../ui/ImGuiManager.hpp"
#include "../../assets/loader/ShaderLoader.hpp"
#include <algorithm>

namespace StarryEngine {

    // ──── DeferredRenderPath ──────────────────────────────────────

    DeferredRenderPath::DeferredRenderPath(std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height)
        : BaseRenderPath(rhi, width, height) {}

    void DeferredRenderPath::setConfig(const RenderPathConfig&) {}

    void DeferredRenderPath::setDrawItems(const AnalysisSceneResult& sceneData) {
        m_cachedSceneData = std::make_shared<AnalysisSceneResult>(sceneData);
        updateMaterialTextures(sceneData);
        distributeDrawItems(sceneData);
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
            if (!pass->configure(*m_renderGraph, texIdMap, m_width, m_height, m_resMgr, m_globalSetLayout)) {
                LOG_ERROR("Pass '{}' configuration failed — skipping", pass->getName());
                continue;
            }

            for (auto& info : pass->getSubpasses()) {
                m_tagToSubpass[info.tag] = SubpassTarget{{}, info.subpassIndex, info.executor};
                m_tagToPassNode[info.tag] = info.passNode;
            }
        }
        if (m_imguiManager) m_imguiManager->setRenderGraph(nullptr);
    }

    // ──── Rebuild ─────────────────────────────────────────────────

    void DeferredRenderPath::doRebuildResources(const AnalysisSceneResult& sceneData) {
        updateMaterialTextures(sceneData);
        distributeDrawItems(sceneData);
        prepareAllPipelines(sceneData);
    }

    void DeferredRenderPath::onAfterCompile() {
        IPass::CompileContext ctx;
        ctx.resMgr = m_resMgr;
        ctx.rhi = m_rhi;
        ctx.renderGraph = m_renderGraph.get();
        ctx.globalDescSet = m_globalDescSet;
        ctx.globalSetLayout = m_globalSetLayout;

        for (auto& pass : m_passes) {
            pass->onAfterCompile(ctx);

            // 重新收集 subpass 信息（pass 可能在 onAfterCompile 中替换了 executor）
            for (auto& info : pass->getSubpasses()) {
                auto it = m_tagToSubpass.find(info.tag);
                if (it != m_tagToSubpass.end()) {
                    it->second.executor = info.executor;
                }
            }
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

    // ──── Draw Item Distribution ──────────────────────────────────

    void DeferredRenderPath::distributeDrawItems(const AnalysisSceneResult& sceneData) {
        for (auto& [tag, target] : m_tagToSubpass) target.executor->clearDrawItems();

        std::string defaultTag;
        if (!m_passes.empty()) {
            auto subs = m_passes.front()->getSubpasses();
            if (!subs.empty()) defaultTag = subs.front().tag;
        }

        for (auto& item : sceneData.drawItems) {
            std::string tag = item->passTag.empty() ? defaultTag : item->passTag;
            auto it = m_tagToSubpass.find(tag);
            if (it != m_tagToSubpass.end()) it->second.executor->addDrawItem(item);
            else LOG_ERROR("No subpass for tag '{}'", tag);
        }
    }

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

    void DeferredRenderPath::prepareAllPipelines(const AnalysisSceneResult& sceneData) {
        for (auto& [tag, target] : m_tagToSubpass) {
            auto& items = target.executor->getDrawItems();
            if (items.empty()) continue;
            std::unordered_set<uint32_t> usedIndices;
            for (auto& item : items)
                if (item->pipelineIndex < sceneData.PSO.size()) usedIndices.insert(item->pipelineIndex);
            std::unordered_map<uint32_t, RHI::PipelineHandle> mapping;
            for (uint32_t idx : usedIndices) {
                const auto& pso = sceneData.PSO[idx];
                auto pipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
                    m_resMgr.get(), *pso, target.renderPass, target.subpassIndex);
                if (pipeline.isValid()) mapping[idx] = pipeline;
            }
            target.executor->setPipelineMapping(std::move(mapping));
        }
    }

} // namespace StarryEngine
