#pragma once
#include "../../assets/Assets.hpp"
#include "../../event/Events.hpp"
#include "../../logging/Logger.hpp"
#include "../../scene/Scene.hpp"
#include "../graph/RenderGraph.hpp"
#include "../backend/RHIFactory.hpp"

namespace StarryEngine {
    class IRenderPath {
    public:
        virtual ~IRenderPath() = default;

        //virtual bool initialize(std::shared_ptr<RHI::IRHI> rhi,
        //    RHI::DescriptorPoolHandle globalPool,
        //    uint32_t width, uint32_t height) = 0;

        virtual void setDrawItems(const Scene::AnalysisSceneResult& secneData) = 0;

        virtual void update(const glm::mat4& view, const glm::mat4& proj, float deltaTime) = 0;
        virtual void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) = 0;
        virtual void onResize(uint32_t width, uint32_t height) = 0;
    };
}