#pragma once
#include <assets/Assets.hpp>
#include <event/Events.hpp>
#include <logging/Logger.hpp>
#include <scene/Scene.hpp>
#include <core/Clock.hpp>
#include <renderer/RenderTypes.hpp>
#include <renderer/SceneAnalyzer.hpp>
#include <renderer/graph/RenderGraph.hpp>
#include <renderer/graph/ParallelRecording.hpp>
#include <core/JobSystem.hpp>
#include <renderer/interface/RHIFactory.hpp>
#include <renderer/renderPaths/DeferredRenderPath.hpp>
#include <functional>

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
        RHI::DescriptorSetHandle getGlobalDescriptorSet(uint32_t slot) {
            return (slot < RHI::kMaxFramesInFlight) ? m_globalDescriptorSets[slot] : RHI::DescriptorSetHandle::Null();
        }
        const std::array<RHI::DescriptorSetHandle, RHI::kMaxFramesInFlight>& getGlobalDescriptorSets() const {
            return m_globalDescriptorSets;
        }

        void setLightViewProj(const glm::mat4& lightVP) { m_lightVP = lightVP; }

        JobSystem* getJobSystem() const { return m_jobSystem.get(); }

        bool isFrameInFlight() const { return m_frameInFlight; }
        void setFrameDataBoneProvider(std::function<void(uint32_t dataSlot)> provider) {
            m_boneProvider = std::move(provider);
        }

    private:
        void buildSceneResources();
        void updateDynamicBuffers(const Clock& clock, uint32_t slot);
        void updateGlobals(const Clock& clock, uint32_t slot);
        void updateInstanceBuffers(const std::vector<std::shared_ptr<Scene::RenderObject>>& objects, uint32_t slot);
        void submitFrameDataJobs(uint32_t slot);

        void ensureParallelRecording();

    private:
        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::shared_ptr<IRenderPath> m_renderPath;

        RHI::DescriptorPoolHandle m_globalPool;
        RHI::DescriptorSetLayoutHandle m_globalSetLayout;

        std::array<RHI::DescriptorSetHandle, RHI::kMaxFramesInFlight> m_globalDescriptorSets;
        std::array<RHI::BufferHandle, RHI::kMaxFramesInFlight> m_globalUniformBuffers;

        std::shared_ptr<Scene::Scene> m_scene;
        std::unique_ptr<SceneAnalyzer> m_sceneAnalyzer;
        std::shared_ptr<AnalysisSceneResult> m_analysisSceneResult;
        uint32_t m_lastAnalyzedVersion = UINT32_MAX;

        std::shared_ptr<Assets::MaterialInstance> m_defaultMaterial;
        std::shared_ptr<Assets::MaterialInstance> m_errorMaterial;
        bool m_materialsInitialized = false;

        bool m_needRebuildGraph = false;

        std::unique_ptr<JobSystem> m_jobSystem;
        std::unique_ptr<ParallelRecordingContext> m_parallelRecording;

        bool m_frameInFlight = false;
        bool m_fidHasKicked = false;
        std::function<void(uint32_t dataSlot)> m_boneProvider;

        glm::mat4 m_lightVP = glm::mat4(1.0f);  
    };
}