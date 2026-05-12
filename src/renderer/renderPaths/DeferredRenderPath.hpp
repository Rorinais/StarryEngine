#pragma once
#include "IRenderPath.hpp"
#include "../graph/RenderGraph.hpp"
#include "../../scene/Scene.hpp"
#include "../passes/Subpass.hpp"
#include <vector>
#include <memory>
#include <unordered_map>

namespace StarryEngine {
    class ImGuiManager;

    struct SubpassTarget {
        RHI::RenderPassHandle renderPass;
        uint32_t subpassIndex;
        std::shared_ptr<ISubpassRecorder> recorder;
    };

    class DeferredRenderPath : public IRenderPath {
    public:
        DeferredRenderPath(std::shared_ptr<RHI::IRHI> rhi,uint32_t width, uint32_t height);
        ~DeferredRenderPath() = default;

        bool initialize() override;
        void onResize(uint32_t width, uint32_t height) override;
        void setConfig(const RenderPathConfig& config) override;
        void setDrawItems(const Scene::AnalysisSceneResult& sceneData) override;
        void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) override;
        void rebuildResources(const Scene::AnalysisSceneResult& sceneData) override;

        void addTextureDesc(std::string name, RHI::TextureDesc desc);
        void setTextureDescs(const std::unordered_map<std::string, RHI::TextureDesc>& descs);

        void setImGuiManager(ImGuiManager* mgr, uint32_t imageCount);
        void addOverlayPass(const OverlayPassDesc& desc) override { m_overlayPasses.push_back(desc); }

        std::shared_ptr<RenderGraph::RenderGraph> getRenderGraph() { return m_renderGraph; }

    private:
        bool buildGraph();
        std::unordered_map<std::string, RenderGraph::TextureId> buildTextureIdMap();
        void buildConfigPasses(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);
        void buildOverlayPasses(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);
        bool compileAndFinalize(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);

        void distributeDrawItems(const Scene::AnalysisSceneResult& sceneData);
        void updateMaterialTextures(const Scene::AnalysisSceneResult& sceneData);
        void prepareAllPipelines(const Scene::AnalysisSceneResult& sceneData);
    private:
        uint32_t m_imguiImageCount = 2;
        ImGuiManager* m_imguiManager = nullptr;

        std::vector<OverlayPassDesc> m_overlayPasses;
    private:
        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::shared_ptr<RenderGraph::RenderGraph> m_renderGraph;
        
        RenderPathConfig m_config;

        uint32_t m_width, m_height;
        bool m_resourceStatsPrinted = false;
        RHI::SamplerHandle m_defaultSampler;
        std::string m_swapchainTextureName = "Swapchain";

        std::unordered_map<std::string, SubpassTarget> m_tagToSubpass;   // 标签 → Subpass 物理信息
        std::unordered_map<std::string, RenderGraph::PassNode*> m_tagToPassNode; // 标签 → PassNode

        std::unordered_map<std::string, RHI::TextureDesc> m_textureDescs;
        std::unordered_map<std::string, RenderGraph::TextureId> m_textureIdMap;
    };

} // namespace StarryEngine