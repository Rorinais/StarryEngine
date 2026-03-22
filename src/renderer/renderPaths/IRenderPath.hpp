#pragma once
#include <any>  
#include <unordered_map>
#include <glm/glm.hpp>
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

        virtual bool initialize() = 0;
        virtual void onResize(uint32_t width, uint32_t height) = 0;
        virtual void setConfig(const RenderPathConfig& config) = 0;
        virtual std::shared_ptr<Assets::MaterialInstance> getMaterial() const = 0;
        virtual void setDrawItems(const Scene::AnalysisSceneResult& secneData) = 0;
        virtual void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) = 0;
        virtual void update(const glm::mat4& view, const glm::mat4& proj, float deltaTime) = 0;

        template<typename T>
        void setCustomData(const std::string& key, const T& data) {
            m_customData[key] = std::make_any<T>(data);  
        }

        template<typename T>
        T* getCustomData(const std::string& key) {
            auto it = m_customData.find(key);
            if (it == m_customData.end()) return nullptr;
            return std::any_cast<T>(&it->second);
        }

        template<typename T>
        const T* getCustomData(const std::string& key) const {
            auto it = m_customData.find(key);
            if (it == m_customData.end()) return nullptr;
            return std::any_cast<T>(&it->second);
        }

    protected:
        glm::mat4 m_lastView;
        glm::mat4 m_lastProj;
        float m_lastDeltaTime = 0.0f;

        std::unordered_map<std::string, std::any> m_customData;
        std::shared_ptr<Scene::AnalysisSceneResult> m_cachedSceneData;

        RenderContext buildRenderContext() const {
            RenderContext ctx(m_customData);  
            ctx.sceneData = m_cachedSceneData;
            ctx.viewMatrix = m_lastView;
            ctx.projMatrix = m_lastProj;
            ctx.deltaTime = m_lastDeltaTime;
            return ctx;
        }
    };
}