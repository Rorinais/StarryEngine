#pragma once
#include "IRenderPath.hpp"
#include "../graph/RenderGraph.hpp"
#include "../../scene/Scene.hpp"
#include "RenderPathConfig.hpp"
#include <vector>
#include <memory>
#include <unordered_map>

namespace StarryEngine {

    class ForwardRenderPath : public IRenderPath {
    public:
        ForwardRenderPath(std::shared_ptr<RHI::IRHI> rhi,
            uint32_t width, uint32_t height);
        ~ForwardRenderPath() = default;

        bool initialize() override;
        void setConfig(const RenderPathConfig& config) override;
        void onResize(uint32_t width, uint32_t height) override;
        void setDrawItems(const Scene::AnalysisSceneResult& sceneData) override;
        void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) override;
        void update(const glm::mat4& view, const glm::mat4& proj, float deltaTime) override;

        std::shared_ptr<RenderGraph::RenderGraph> getRenderGraph() { return m_renderGraph; }

        void setTextureDescs(const std::unordered_map<std::string, RHI::TextureDesc>& descs);
        void setLightingMaterial(std::shared_ptr<Assets::MaterialInstance> material) { m_material = material; }
        std::shared_ptr<Assets::MaterialInstance> getMaterial() const override { return m_material; }
    private:
        bool buildGraph();
        void distributeDrawItems(const Scene::AnalysisSceneResult& sceneData);

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        uint32_t m_width, m_height;

        RenderPathConfig m_config;
        std::shared_ptr<RenderGraph::RenderGraph> m_renderGraph;

        std::unordered_map<Scene::RenderStage, StagePassInfo> m_stagePassInfo;
        std::unordered_map<Scene::RenderStage, RenderGraph::PassNode*> m_stagePassNode;
        std::unordered_map<uint64_t, std::shared_ptr<ISubpassRecorder>> m_subpassRecorders;

        std::unordered_map<std::string, RHI::TextureDesc> m_textureDescs;
        std::unordered_map<std::string, RenderGraph::TextureId> m_textureIdMap;

        std::string m_swapchainTextureName = "Swapchain";
        std::shared_ptr<Assets::MaterialInstance> m_material;
    };

} // namespace StarryEngine