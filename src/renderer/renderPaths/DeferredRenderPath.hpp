#pragma once
#include "IRenderPath.hpp"
#include "../graph/RenderGraph.hpp"
#include "../../scene/Scene.hpp"
#include "../passes/Subpass.hpp"
#include <vector>
#include <memory>
#include <unordered_map>

namespace StarryEngine {

    class DeferredRenderPath : public IRenderPath {
    public:
        DeferredRenderPath(std::shared_ptr<RHI::IRHI> rhi,
            uint32_t width, uint32_t height);
        ~DeferredRenderPath() = default;

        bool initialize() override;
        void setConfig(const RenderPathConfig& config) override;
        void onResize(uint32_t width, uint32_t height) override;
        void setDrawItems(const Scene::AnalysisSceneResult& sceneData) override;
        void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) override;
        void rebuildResources(const Scene::AnalysisSceneResult& sceneData) override;

        std::shared_ptr<RenderGraph::RenderGraph> getRenderGraph() { return m_renderGraph; }

        void setTextureDescs(const std::unordered_map<std::string, RHI::TextureDesc>& descs);

        void addTextureDesc(std::string name, RHI::TextureDesc desc);
    private:
        bool buildGraph();
        void distributeDrawItems(const Scene::AnalysisSceneResult& sceneData);
        void updateMaterialTextures(const Scene::AnalysisSceneResult& sceneData);
        void prepareAllPipelines(const Scene::AnalysisSceneResult& sceneData);

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        uint32_t m_width, m_height;

        RenderPathConfig m_config;
        std::shared_ptr<RenderGraph::RenderGraph> m_renderGraph;

        struct SubpassTarget {
            RHI::RenderPassHandle renderPass;  // 初始化时为空，compile 后填入
            uint32_t subpassIndex;
            std::shared_ptr<ISubpassRecorder> recorder;
        };

        std::unordered_map<std::string, SubpassTarget> m_tagToSubpass;   // 标签 → Subpass 物理信息
        std::unordered_map<std::string, RenderGraph::PassNode*> m_tagToPassNode; // 标签 → PassNode

        std::unordered_map<std::string, RHI::TextureDesc> m_textureDescs;
        std::unordered_map<std::string, RenderGraph::TextureId> m_textureIdMap;

        std::string m_swapchainTextureName = "Swapchain";

        RHI::SamplerHandle m_defaultSampler;
        bool m_resourceStatsPrinted = false;
    };

} // namespace StarryEngine