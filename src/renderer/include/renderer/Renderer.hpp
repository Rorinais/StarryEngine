#pragma once
#include <assets/Assets.hpp>
#include <event/Events.hpp>
#include <logging/Logger.hpp>
#include <scene/Scene.hpp>
#include <core/Clock.hpp>
#include <renderer/RenderTypes.hpp>
#include <renderer/SceneAnalyzer.hpp>
#include <renderer/graph/RenderGraph.hpp>
#include <renderer/backend/RHIFactory.hpp>
#include <renderer/renderPaths/DeferredRenderPath.hpp>
#include <renderer/renderPaths/ForwardRenderPath.hpp>

namespace StarryEngine {
    class Renderer {
    public:
        Renderer(std::shared_ptr<RHI::IRHI> rhi,RHI::DescriptorPoolHandle globalPool,std::shared_ptr<Scene::Scene> scene);
        ~Renderer() { destroy(); }
        void destroy();

        void initDefaultMaterials();
        void createGlobalSetLayout();
        void createGlobalUniformBuffer();
        void onResize(uint32_t width, uint32_t height);

        void analysisScene();
        void reloadAllShaders();
        void rebuildRenderGraph();
        void reloadShader(const std::string& vertPath, const std::string& fragPath);
        void renderFrame(RHI::RHICommandEncoder* encoder, uint32_t frameIndex, const Clock& clock);

        void setNeedRebuildGraph() { m_needRebuildGraph = true; }
        void setImGuiManager(ImGuiManager* mgr, uint32_t imageCount);
        void setRenderPath(std::shared_ptr<IRenderPath> newRenderPath);

        void addOverlayPass(const std::string& tag, std::shared_ptr<IPassExecutor> executor);
        void addOverlayPass(const OverlayPassDesc& desc);
        void removeOverlayPass(const std::string& tag);
        void clearOverlayPasses();

        RHI::DescriptorSetLayoutHandle getGlobalSetLayout() { return m_globalSetLayout; }
        RHI::DescriptorSetHandle getGlobalDescriptorSet() { return m_globalDescriptorSet; }

        void setLightViewProj(const glm::mat4& lightVP) { m_lightVP = lightVP; }

    private:
        void buildSceneResources();
        void updateDynamicBuffers(const Clock& clock);
        void updateInstanceBuffers(const std::vector<std::shared_ptr<Scene::RenderObject>>& objects);

    private:
        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::shared_ptr<IRenderPath> m_renderPath;

        RHI::DescriptorPoolHandle m_globalPool;
        RHI::DescriptorSetLayoutHandle m_globalSetLayout;
        RHI::DescriptorSetHandle m_globalDescriptorSet;
        RHI::BufferHandle m_globalUniformBuffer;

        std::shared_ptr<Scene::Scene> m_scene;
        std::unique_ptr<SceneAnalyzer> m_sceneAnalyzer;
        std::shared_ptr<AnalysisSceneResult> m_analysisSceneResult;
        uint32_t m_lastAnalyzedVersion = UINT32_MAX;

        std::shared_ptr<Assets::MaterialInstance> m_defaultMaterial;
        std::shared_ptr<Assets::MaterialInstance> m_errorMaterial;
        bool m_materialsInitialized = false;

        bool m_needRebuildGraph = false;

        glm::mat4 m_lightVP = glm::mat4(1.0f);   // 未设置时为单位阵（阴影贴图退化为无意义采样，不会崩溃）
    };
}