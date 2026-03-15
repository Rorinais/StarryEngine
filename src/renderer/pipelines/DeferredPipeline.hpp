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
        DeferredPipeline() = default;

        //bool initialize(std::shared_ptr<RHI::IRHI> rhi,
        //    RHI::DescriptorPoolHandle globalPool,
        //    uint32_t width, uint32_t height) override;

        bool initialize(std::shared_ptr<RHI::IRHI> rhi,
            RHI::DescriptorPoolHandle globalPool,
            uint32_t width, uint32_t height,
            const Assets::VertexLayout& vertexLayout) override;

        void update(const Scene::Scene& scene, float deltaTime) override;

        void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) override;

        void onResize(uint32_t width, uint32_t height) override;

        void setPipelineDesc() {

        }

        RHI::DescriptorSetLayoutHandle getDescriptorSetLayout() const { return m_descriptorSetLayout; }

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

    };

} // namespace StarryEngine