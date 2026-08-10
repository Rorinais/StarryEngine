#pragma once
#include <any>
#include <typeindex>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <assets/Assets.hpp>
#include <renderer/RenderTypes.hpp>
#include <event/Events.hpp>
#include <logging/Logger.hpp>
#include <scene/Scene.hpp>
#include <renderer/graph/RenderGraph.hpp>
#include <renderer/backend/RHIFactory.hpp>
#include <renderer/passes/Type.hpp>


#include <renderer/RenderBlackboard.hpp>

namespace StarryEngine {

    struct OverlayPassDesc {
        std::string tag;
        std::shared_ptr<IPassExecutor> executor;

        std::vector<std::pair<std::string, RenderGraph::AttachmentParams>> colorOutputs;
        std::optional<std::pair<std::string, RenderGraph::AttachmentParams>> depthOutput;
        std::vector<std::pair<std::string, RenderGraph::AttachmentParams>> inputAttachments;
    };

    inline OverlayPassDesc makeUIOverlay(const std::string& tag,
                                         std::shared_ptr<IPassExecutor> executor) {
        OverlayPassDesc desc{tag, executor};
        RenderGraph::AttachmentParams scParams;
        scParams.loadOp = RHI::AttachmentLoadOp::Clear;
        scParams.storeOp = RHI::AttachmentStoreOp::Store;
        scParams.initialLayout = RHI::ImageLayout::Undefined;
        scParams.finalLayout = RHI::ImageLayout::PresentSrc;
        scParams.clearColor = {0.08f, 0.08f, 0.10f, 1.0f};
        desc.colorOutputs.emplace_back("Swapchain", scParams);

        RenderGraph::AttachmentParams inParams;
        inParams.loadOp = RHI::AttachmentLoadOp::Load;
        inParams.storeOp = RHI::AttachmentStoreOp::DontCare;
        inParams.initialLayout = RHI::ImageLayout::ShaderReadOnly;
        inParams.finalLayout = RHI::ImageLayout::ShaderReadOnly;
        desc.inputAttachments.emplace_back("SceneColor", inParams);

        return desc;
    }

    class IRenderPath {
    public:
        virtual ~IRenderPath() = default;

        virtual bool initialize() = 0;
        virtual void onResize(uint32_t width, uint32_t height) = 0;
        virtual void setConfig(const RenderPathConfig& config) = 0;
        virtual void setDrawItems(const AnalysisSceneResult& secneData) = 0;
        virtual void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) = 0;
        virtual void rebuildResources(const AnalysisSceneResult& sceneData) = 0;

        virtual void addOverlayPass(const OverlayPassDesc& desc) = 0;
        virtual void removeOverlayPass(const std::string& tag) = 0;
        virtual void clearOverlayPasses() = 0;
        virtual const std::vector<OverlayPassDesc>& getOverlayPasses() const = 0;

        void addOverlayPass(const std::string& tag,std::shared_ptr<IPassExecutor> executor) {
            addOverlayPass({ tag, std::move(executor) });
        }

        RenderBlackboard& getBlackboard() { return m_blackboard; }

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
        RenderBlackboard m_blackboard;
        std::shared_ptr<AnalysisSceneResult> m_cachedSceneData;

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