#pragma once
#include "../../assets/Assets.hpp"
#include "../../event/Events.hpp"
#include "../../logging/Logger.hpp"
#include "../../scene/Scene.hpp"
#include "../graph/RenderGraph.hpp"
#include "../backend/RHIFactory.hpp"

namespace StarryEngine {
    class IPipeline {
    public:
        virtual ~IPipeline() = default;
        virtual bool initialize(std::shared_ptr<RHI::IRHI> rhi,
            RHI::DescriptorPoolHandle globalPool,
            uint32_t width, uint32_t height, 
            const Assets::VertexLayout& vertexLayout) = 0;

        //virtual bool initialize(std::shared_ptr<RHI::IRHI> rhi,
        //    RHI::DescriptorPoolHandle globalPool,
        //    uint32_t width, uint32_t height) = 0;
        virtual void update(const Scene::Scene& scene, float deltaTime) = 0;
        virtual void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) = 0;
        virtual void onResize(uint32_t width, uint32_t height) = 0;
    };
}