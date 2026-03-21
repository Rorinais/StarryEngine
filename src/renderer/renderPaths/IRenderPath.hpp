#pragma once
#include "../../assets/Assets.hpp"
#include "../../event/Events.hpp"
#include "../../logging/Logger.hpp"
#include "../../scene/Scene.hpp"
#include "../graph/RenderGraph.hpp"
#include "../backend/RHIFactory.hpp"
#include "RenderPathConfig.hpp" 

namespace StarryEngine {
    class IRenderPath {
    public:
        virtual ~IRenderPath() = default;

        virtual void setConfig(const RenderPathConfig& config) = 0;

        virtual bool initialize() = 0;

        virtual void setDrawItems(const Scene::AnalysisSceneResult& secneData) = 0;

        virtual void update(const glm::mat4& view, const glm::mat4& proj, float deltaTime) = 0;
        virtual void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) = 0;
        virtual void onResize(uint32_t width, uint32_t height) = 0;
    };
}