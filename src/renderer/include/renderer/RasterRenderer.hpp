#pragma once
#include <renderer/BaseRenderer.hpp>
#include <renderer/SceneAnalyzer.hpp>
#include <assets/material/MaterialInstance.hpp>

namespace StarryEngine {
    // ── 光栅渲染器：延迟/前向技术 ──
    // SceneAnalyzer 场景→draw items/材质/实例缓冲；持有默认材质、PBR 灯光（lightVP）、shader 热重载。
    class RasterRenderer : public BaseRenderer {
    public:
        using BaseRenderer::BaseRenderer;

        // 显式构造：全局布局/UBO 由 BaseRenderer 自初始化，这里补齐默认材质
        explicit RasterRenderer(std::shared_ptr<RHI::IRHI> rhi,
                                RHI::DescriptorPoolHandle globalPool,
                                std::shared_ptr<Scene::Scene> scene);

        void initDefaultMaterials();
        void analysisScene();
        void setLightViewProj(const glm::mat4& lightVP) { m_lightVP = lightVP; }
        const glm::mat4& getLightViewProj() const override { return m_lightVP; }
        void reloadAllShaders() override;
        void reloadShader(const std::string& vertPath, const std::string& fragPath);

        std::shared_ptr<AnalysisSceneResult> getAnalysisSceneResult() { return m_analysisSceneResult; }

    protected:
        void buildSceneResources() override;
        void submitFrameDataJobs(uint32_t slot) override;
        void updateInstanceBuffers(const std::vector<std::shared_ptr<Scene::RenderObject>>& objects, uint32_t slot) override;
        void updateDynamicBuffers(const Clock& clock, uint32_t slot) override;
        void doRebuildRenderGraph() override;

        void onAfterPathResize() override;

    private:
        std::unique_ptr<SceneAnalyzer> m_sceneAnalyzer;
        std::shared_ptr<AnalysisSceneResult> m_analysisSceneResult;
        uint32_t m_lastAnalyzedVersion = UINT32_MAX;

        std::shared_ptr<Assets::MaterialInstance> m_defaultMaterial;
        std::shared_ptr<Assets::MaterialInstance> m_errorMaterial;
        bool m_materialsInitialized = false;

        glm::mat4 m_lightVP = glm::mat4(1.0f);
    };
}
