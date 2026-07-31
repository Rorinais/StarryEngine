#pragma once
#include "BaseRenderPath.hpp"
#include "../passExecutor/ParticleParams.hpp"
#include "../graph/RenderGraph.hpp"
#include "../../scene/Scene.hpp"
#include "../passes/Subpass.hpp"
#include <vector>
#include <memory>
#include <unordered_map>

namespace StarryEngine {
    class ImGuiManager;

    class DeferredRenderPath : public BaseRenderPath {
    public:
        DeferredRenderPath(std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height);
        ~DeferredRenderPath() = default;

        // IRenderPath — deferred-specific overrides
        void setConfig(const RenderPathConfig& config) override;
        void setDrawItems(const Scene::AnalysisSceneResult& sceneData) override;

        void setImGuiManager(ImGuiManager* mgr, uint32_t imageCount);
        void setParticleParams(const ParticleParams& p) { m_particleParams = p; }

    protected:
        void buildConfigPasses(
            std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap) override;
        void doRebuildResources(const Scene::AnalysisSceneResult& sceneData) override;
        void onAfterCompile() override;
        void onAfterCompileImGui() override;

    private:
        void buildTestComputePass();
        void prepareParticlePipeline();

        void distributeDrawItems(const Scene::AnalysisSceneResult& sceneData);
        void updateMaterialTextures(const Scene::AnalysisSceneResult& sceneData);
        void prepareAllPipelines(const Scene::AnalysisSceneResult& sceneData);

        uint32_t m_imguiImageCount = 2;
        ImGuiManager* m_imguiManager = nullptr;

        // ── 粒子系统 ──
        RHI::ShaderHandle             m_particleVS, m_particleFS, m_particleCS;
        RHI::PipelineLayoutHandle     m_particleRenderLayout;
        RenderGraph::BufferId         m_particleBufferId;
        uint32_t                      m_particleCount = 0;
        RHI::DescriptorSetLayoutHandle m_particleCSDescLayout;
        RHI::DescriptorSetLayoutHandle m_particleRenderDescLayout;
        RHI::DescriptorSetLayoutHandle m_particleRenderSet1Layout;  // prepareParticlePipeline 中创建
        RHI::PipelineLayoutHandle      m_particleCSLayout;
        RHI::PipelineHandle            m_particleCSPipeline;
        RHI::DescriptorPoolHandle      m_particleCSPool;
        RHI::DescriptorPoolHandle      m_particleRenderPool;
        ParticleParams                 m_particleParams;
    };

} // namespace StarryEngine
