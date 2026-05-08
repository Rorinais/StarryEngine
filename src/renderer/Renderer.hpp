#pragma once
#include "../assets/Assets.hpp"
#include "../event/Events.hpp"
#include "../logging/Logger.hpp"
#include "../scene/Scene.hpp"
#include "graph/RenderGraph.hpp"
#include "backend/RHIFactory.hpp"
#include "renderPaths/DeferredRenderPath.hpp"
#include "renderPaths/ForwardRenderPath.hpp"

namespace StarryEngine {
    class Renderer {
    public:
        Renderer(std::shared_ptr<RHI::IRHI> rhi,
            RHI::DescriptorPoolHandle globalPool,
            std::shared_ptr<Scene::Scene> scene);
        ~Renderer();

        void destroy();
        void initDefaultMaterials();
        void renderFrame(RHI::RHICommandEncoder* encoder, uint32_t frameIndex, float deltaTime);
        void onResize(uint32_t width, uint32_t height);
        void setRenderPath(std::shared_ptr<IRenderPath> newRenderPath);

        void analysisScene();

        void createGlobalSetLayout();
        void createGlobalUniformBuffer();

        RHI::DescriptorSetLayoutHandle getGlobalSetLayout() { return m_globalSetLayout; }
        RHI::DescriptorSetHandle getGlobalDescriptorSet() { return m_globalDescriptorSet; }

        void reloadAllShaders();
        void reloadShader(const std::string& vertPath, const std::string& fragPath);
        void prepareFrame(float deltaTime); 

        void addOverlayPass(const std::string& tag, std::shared_ptr<ISubpassRecorder> recorder) {
            if (m_renderPath) {
                m_renderPath->addOverlayPass(tag, std::move(recorder));
            }
        }

        void rebuildRenderGraph() {
            if (m_renderPath && m_analysisSceneResult) {
                m_renderPath->initialize();             
                m_renderPath->rebuildResources(*m_analysisSceneResult);
            }
            else {
                LOG_ERROR("rebuildRenderGraph called but no scene result");
            }
        }

        void setNeedRebuildGraph() { m_needRebuildGraph = true; }

        void setImGuiManager(ImGuiManager* mgr, uint32_t imageCount) {
            if (m_renderPath) {
                if (auto* dp = dynamic_cast<DeferredRenderPath*>(m_renderPath.get())) {
                    dp->setImGuiManager(mgr, imageCount);
                }
            }
        }

    private:
        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::shared_ptr<IRenderPath> m_renderPath;

        RHI::DescriptorPoolHandle m_globalPool;
        RHI::DescriptorSetLayoutHandle m_globalSetLayout;
        RHI::DescriptorSetHandle m_globalDescriptorSet;
        RHI::BufferHandle m_globalUniformBuffer;

        std::shared_ptr<Scene::Scene> m_scene;
        std::shared_ptr<Scene::AnalysisSceneResult> m_analysisSceneResult;
        uint32_t m_lastAnalyzedVersion = UINT32_MAX;

        std::shared_ptr<Assets::MaterialInstance> m_defaultMaterial;
        std::shared_ptr<Assets::MaterialInstance> m_errorMaterial;
        bool m_materialsInitialized = false;

        bool m_needRebuildGraph = false;
    };
}