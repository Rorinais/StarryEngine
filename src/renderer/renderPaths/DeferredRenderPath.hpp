#pragma once
#include "IRenderPath.hpp"
#include "../graph/RenderGraph.hpp"
#include "../subpassRecorder/GbufferRecorder.hpp"
#include "../../scene/Scene.hpp"
#include <vector>
#include <memory>

namespace StarryEngine {

    class DeferredRenderPath : public IRenderPath {
    public:
        DeferredRenderPath(std::shared_ptr<RHI::IRHI> rhi,
            RHI::DescriptorPoolHandle globalPool,
            uint32_t width, uint32_t height)
            :m_rhi(rhi),m_globalPool(globalPool),
            m_resMgr(rhi->getResourceManager()),
            m_width(width),m_height(height){ }

        //bool initialize(std::shared_ptr<RHI::IRHI> rhi,
        //    RHI::DescriptorPoolHandle globalPool,
        //    uint32_t width, uint32_t height) override;

        bool initialize(RHI::DescriptorSetLayoutHandle globalSetLayout);

        bool createRenderGraph();

        void setDrawItems(const Scene::AnalysisSceneResult& secneData) override;

        void update(const glm::mat4& view, const glm::mat4& proj, float deltaTime) override;

        void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) override;

        void onResize(uint32_t width, uint32_t height) override;

        bool createGridResources();

        void setGlobalDescriptorSet(RHI::DescriptorSetHandle globalSet) { m_globalDescriptorSet = globalSet; }
        RHI::DescriptorSetLayoutHandle getDescriptorSetLayout() { return m_descriptorSetLayout; }

    private:
        RHI::PipelineHandle getOrCreatePipeline(const Scene::GraphicsPipelineState& state,
            RHI::PipelineLayoutHandle layout);

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        uint32_t m_width = 0, m_height = 0;

        std::shared_ptr<RenderGraph::RenderGraph> m_renderGraph;
        RenderGraph::TextureId m_swapchainTex;
        RHI::RenderPassHandle m_renderPassHandle;

        RHI::DescriptorPoolHandle m_globalPool;
        RHI::DescriptorSetLayoutHandle m_globalSetLayout;
        RHI::DescriptorSetHandle m_globalDescriptorSet;

        RHI::DescriptorSetLayoutHandle m_descriptorSetLayout;
        RHI::PipelineLayoutHandle m_pipelineLayout;        // 用于普通物体（set 0 + set 1 非空）
        RHI::PipelineLayoutHandle m_pipelineLayoutGrid;    // 用于网格（set 0 + set 1 为空）

        std::shared_ptr<RenderGraph::MeshDrawRecorder> m_recorder;
        std::vector<std::shared_ptr<Scene::DrawItem>> m_cachedDrawItems;
        std::vector<std::shared_ptr<Scene::GraphicsPipelineState>> m_cachedPipelines;

        std::unordered_map<size_t, RHI::PipelineHandle> m_pipelineCache; // 管线缓存

        std::shared_ptr<RenderGraph::MeshDrawRecorder> m_gridRecorder;
        std::shared_ptr<Assets::Geometry> m_gridGeometry;
        std::shared_ptr<Assets::Material> m_gridMaterial;

    };

} // namespace StarryEngine