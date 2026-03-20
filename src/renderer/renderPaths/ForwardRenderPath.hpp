#pragma once
#include "IRenderPath.hpp"
#include "../graph/RenderGraph.hpp"
#include "../subpassRecorder/GbufferRecorder.hpp"
#include "../../scene/Scene.hpp"
#include <vector>
#include <memory>

namespace StarryEngine {

    class ForwardRenderPath : public IRenderPath {
    public:
        ForwardRenderPath(std::shared_ptr<RHI::IRHI> rhi,
            RHI::DescriptorPoolHandle globalPool,
            uint32_t width, uint32_t height)
            : m_rhi(rhi), m_globalPool(globalPool),
            m_resMgr(rhi->getResourceManager()),
            m_width(width), m_height(height) {
        }

        bool initialize(RHI::DescriptorSetLayoutHandle globalSetLayout) override;

        void setDrawItems(const Scene::AnalysisSceneResult& sceneData) override;

        void update(const glm::mat4& view, const glm::mat4& proj, float deltaTime) override;

        void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) override;

        void onResize(uint32_t width, uint32_t height) override;

        void setGlobalDescriptorSet(RHI::DescriptorSetHandle globalSet) { m_globalDescriptorSet = globalSet; }

    private:
        bool createRenderGraph();

        RHI::PipelineHandle getOrCreatePipeline(const Scene::GraphicsPipelineState& state,
            RHI::RenderPassHandle renderPass,
            uint32_t subpassIndex);

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        uint32_t m_width = 0, m_height = 0;

        std::shared_ptr<RenderGraph::RenderGraph> m_renderGraph;
        RenderGraph::TextureId m_swapchainTex;
        RHI::RenderPassHandle m_renderPassHandle;

        RHI::DescriptorPoolHandle m_globalPool;
        RHI::DescriptorSetLayoutHandle m_globalSetLayout;
        RHI::DescriptorSetHandle m_globalDescriptorSet;

        // 普通物体录制器
        std::shared_ptr<RenderGraph::MeshDrawRecorder> m_recorder;
        std::vector<std::shared_ptr<Scene::DrawItem>> m_cachedDrawItems;
        std::vector<std::shared_ptr<Scene::GraphicsPipelineState>> m_cachedPipelines;
    };

} // namespace StarryEngine