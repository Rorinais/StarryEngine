#pragma once
#include "BaseRenderPath.hpp"
#include "../graph/RenderGraph.hpp"
#include "../../scene/Scene.hpp"
#include "../passes/Subpass.hpp"
#include <vector>
#include <memory>
#include <unordered_map>

namespace StarryEngine {
    class ImGuiManager;

    // 可调粒子参数
    struct ParticleParams {
        float gravity = 0.0f, speedMin = 0.8f, speedMax = 2.5f, lifetime = 3.0f;
        float spreadXZ = 0.8f, swayFreq = 2.7f, swayAmp = 0.8f;
        float emitterY = -3.0f, topDiffuse = 1.5f, topThreshold = 2.0f;
        float colorYoung[4]  = {1.0f, 0.95f, 0.5f, 0.0f};
        float colorMiddle[4] = {1.0f, 0.45f, 0.05f, 0.0f};
        float colorOld[4]    = {0.7f, 0.1f, 0.02f, 0.0f};
        float pointSizeMin = 2.0f, pointSizeMax = 8.0f;
    };

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

    private:
        void buildTestComputePass();
        void prepareParticlePipeline();

        void distributeDrawItems(const Scene::AnalysisSceneResult& sceneData);
        void updateMaterialTextures(const Scene::AnalysisSceneResult& sceneData);
        void prepareAllPipelines(const Scene::AnalysisSceneResult& sceneData);

        uint32_t m_imguiImageCount = 2;
        ImGuiManager* m_imguiManager = nullptr;

        // ── 粒子系统 ──
        RHI::ShaderHandle             m_particleVS, m_particleFS;
        RHI::PipelineLayoutHandle     m_particleRenderLayout;
        RenderGraph::BufferId         m_particleBufferId;
        uint32_t                      m_particleCount = 0;
        RHI::DescriptorSetLayoutHandle m_particleCSDescLayout;
        RHI::PipelineLayoutHandle      m_particleCSLayout;
        RHI::PipelineHandle            m_particleCSPipeline;
        ParticleParams                 m_particleParams;
    };

} // namespace StarryEngine
