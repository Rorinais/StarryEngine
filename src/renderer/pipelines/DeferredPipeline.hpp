#pragma once
#include "IPipeline.hpp"
#include "../graph/RenderGraph.hpp"
#include "../subpassRecorder/GbufferRecorder.hpp"
#include "../../scene/Scene.hpp"
#include <vector>
#include <memory>

namespace StarryEngine {

    class DeferredPipeline : public IPipeline {
    public:
        DeferredPipeline(std::shared_ptr<RHI::IRHI> rhi,
            RHI::DescriptorPoolHandle globalPool,
            uint32_t width, uint32_t height)
            :m_rhi(rhi),m_globalPool(globalPool),
            m_resMgr(rhi->getResourceManager()),
            m_width(width),m_height(height){ }

        //bool initialize(std::shared_ptr<RHI::IRHI> rhi,
        //    RHI::DescriptorPoolHandle globalPool,
        //    uint32_t width, uint32_t height) override;

        bool initialize(const Assets::VertexLayout& vertexLayout);

        bool createRenderGraph();

        void update(const Scene::Scene& scene, float deltaTime) override;

        void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) override;

        void onResize(uint32_t width, uint32_t height) override;

        RHI::DescriptorSetLayoutHandle getDescriptorSetLayout() const { return m_descriptorSetLayout; }

        bool createGridResources();

        RHI::PipelineHandle getOrCreatePipeline(RHI::ShaderHandle vertShader, RHI::ShaderHandle fragShader, uint32_t subpassIndex);

    private:
        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        RHI::DescriptorPoolHandle m_globalPool;
        uint32_t m_width = 0, m_height = 0;

        std::shared_ptr<RenderGraph::RenderGraph> m_renderGraph;
        std::shared_ptr<RenderGraph::MeshDrawRecorder> m_recorder;

        RenderGraph::TextureId m_swapchainTex;

        RHI::PipelineLayoutHandle m_pipelineLayout;
        RHI::DescriptorSetLayoutHandle m_descriptorSetLayout;
        Assets::VertexLayout m_vertexLayout;
        RHI::RenderPassHandle m_renderPassHandle;
        std::unordered_map<size_t, RHI::PipelineHandle> m_pipelineCache;

        std::shared_ptr<RenderGraph::MeshDrawRecorder> m_gridRecorder;
        std::shared_ptr<Assets::Geometry> m_gridGeometry;
        std::shared_ptr<Assets::Material> m_gridMaterial;
    };

} // namespace StarryEngine