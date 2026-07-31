#pragma once
#include <any>
#include <typeindex>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include "../../assets/Assets.hpp"
#include "../../event/Events.hpp"
#include "../../logging/Logger.hpp"
#include "../../scene/Scene.hpp"
#include "../graph/RenderGraph.hpp"
#include "../backend/RHIFactory.hpp"
#include"../passes/Subpass.hpp"


namespace StarryEngine {
    struct OverlayPassDesc {
        std::string tag;
        std::shared_ptr<IPassExecutor> executor;

        // 颜色输出附件 {纹理名, 附件参数}
        std::vector<std::pair<std::string, RenderGraph::AttachmentParams>> colorOutputs;
        // 深度附件（可选）
        std::optional<std::pair<std::string, RenderGraph::AttachmentParams>> depthOutput;
        // 输入附件 {纹理名, 附件参数}
        std::vector<std::pair<std::string, RenderGraph::AttachmentParams>> inputAttachments;
    };

    // 便捷构造：UI overlay（读 SceneColor → 写 Swapchain）
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

    // ── 类型安全的 Blackboard ───────────────────────────────────────
    // 模块间通过 TYPE 共享数据，不需要知道对方是谁。
    // 用法：
    //   struct SSAOData { float radius; TextureHandle kernel; };
    //   blackboard.put(SSAOData{2.5f, tex});        // SSAO 模块写入
    //   auto& d = blackboard.get<SSAOData>();         // Tonemap 模块读取
    //
    // 对比 Frostbite 的 Blackboard：
    //   你的 m_customData（字符串 key）→ 打错字返回 nullptr
    //   RenderBlackboard（类型 key）    → 编译期保证类型正确
    class RenderBlackboard {
    public:
        template<typename T>
        void put(T value) {
            m_data[std::type_index(typeid(T))] = std::make_any<T>(std::move(value));
        }

        template<typename T>
        T* get() {
            auto it = m_data.find(std::type_index(typeid(T)));
            if (it == m_data.end()) return nullptr;
            return std::any_cast<T>(&it->second);
        }

        template<typename T>
        const T* get() const {
            auto it = m_data.find(std::type_index(typeid(T)));
            if (it == m_data.end()) return nullptr;
            return std::any_cast<T>(&it->second);
        }

        void clear() { m_data.clear(); }

    private:
        std::unordered_map<std::type_index, std::any> m_data;
    };

    class IRenderPath {
    public:
        virtual ~IRenderPath() = default;

        virtual bool initialize() = 0;
        virtual void onResize(uint32_t width, uint32_t height) = 0;
        virtual void setConfig(const RenderPathConfig& config) = 0;
        virtual void setDrawItems(const Scene::AnalysisSceneResult& secneData) = 0;
        virtual void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) = 0;
        virtual void rebuildResources(const Scene::AnalysisSceneResult& sceneData) = 0;

        virtual void addOverlayPass(const OverlayPassDesc& desc) = 0;
        virtual void removeOverlayPass(const std::string& tag) = 0;
        virtual void clearOverlayPasses() = 0;
        virtual const std::vector<OverlayPassDesc>& getOverlayPasses() const = 0;

        void addOverlayPass(const std::string& tag,std::shared_ptr<IPassExecutor> executor) {
            addOverlayPass({ tag, std::move(executor) });
        }

        // 类型安全的跨模块数据共享
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