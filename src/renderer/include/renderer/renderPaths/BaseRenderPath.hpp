#pragma once
#include <renderer/renderPaths/LayerRegistry.hpp>
#include <renderer/renderPaths/IRenderPath.hpp>
#include <renderer/graph/RenderGraph.hpp>
#include <scene/Scene.hpp>
#include <renderer/RenderTypes.hpp>
#include <renderer/passExecutor/IPassExecutor.hpp>
#include <renderer/passes/IPass.hpp>
#include <vector>
#include <memory>
#include <unordered_map>

namespace StarryEngine {
    class ImGuiManager;

    struct SubpassTarget {
        RHI::RenderPassHandle renderPass;
        uint32_t subpassIndex;
        std::shared_ptr<IPassExecutor> executor;
    };


    enum class RenderPassEvent {
        BeforeShadow, Shadow, AfterShadow,
        BeforeOpaque, Opaque, AfterOpaque,
        BeforePost, Post, AfterPost,
        BeforeStencil, Stencil, AfterStencil,
        BeforeParticles, Particles, AfterParticles,
        BeforePresent,
        Count
    };

    struct PassSlot {
        int         layer = 1;          ///< 渲染层索引（见 RenderLayers；0 背景 … 8+ 用户自定义）
        std::shared_ptr<IPass> pass;
        bool enabled = true;
    };

    class BaseRenderPath : public IRenderPath {
    public:
        /// 呈现层/叠加层 subpass 的键（UI 层=5, queue 0）——替代旧 "Presentation"/"ImGui" 字符串
        static long long presentKey() { return (static_cast<long long>(RenderLayers::UI) << 32); }

        BaseRenderPath(std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height);
        ~BaseRenderPath() override = default;

        bool initialize() override;
        void onResize(uint32_t width, uint32_t height) override;
        void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) override;
        void rebuildResources(const AnalysisSceneResult& sceneData) override;

        void addTextureDesc(std::string name, RHI::TextureDesc desc);
        void setTextureDescs(const std::unordered_map<std::string, RHI::TextureDesc>& descs);
        void setPresentationDescriptorData(RHI::DescriptorSetLayoutHandle globalSetLayout,
            std::vector<RHI::DescriptorSetHandle> globalDescSets);

        // ── 槽位操作（demo 注入/覆盖/启停）──
        void insertPass(int layer, std::shared_ptr<IPass> pass);
        void insertPass(const std::string& layerName, std::shared_ptr<IPass> pass);
        void insertPass(RenderPassEvent ev, std::shared_ptr<IPass> pass);
        void overrideSlot(const std::string& layerName, std::shared_ptr<IPass> pass);
        void overrideSlot(RenderPassEvent ev, std::shared_ptr<IPass> pass);
        void setSlotEnabled(const std::string& layerName, bool enabled);
        void setSlotEnabled(RenderPassEvent ev, bool enabled);
        void clearSlots() { m_slots.clear(); }

        void setPresentClearColor(const RHI::Color& color) { m_presentClearColor = color; }

        void setPresentInput(const std::string& tex) { m_presentInputTexture = tex; }

        void setScene(Scene::Scene* scene) { m_blackboard.put<Scene::Scene*>(scene); }

        void addOverlayPass(const OverlayPassDesc& desc) override { m_overlayPasses.push_back(desc); }
        void removeOverlayPass(const std::string& tag) override;
        void clearOverlayPasses() override { m_overlayPasses.clear(); }
        const std::vector<OverlayPassDesc>& getOverlayPasses() const override { return m_overlayPasses; }

        void setPassList(PassList passes) { m_passes = std::move(passes); }
        std::shared_ptr<RenderGraph::RenderGraph> getRenderGraph() { return m_renderGraph; }

        void setParallelRecording(const ParallelRecordingContext* parallel) override { m_parallel = parallel; }

    protected:

        void setDefaultSlot(int layer, std::shared_ptr<IPass> pass);
        void setDefaultSlot(const std::string& layerName, std::shared_ptr<IPass> pass);
        void setDefaultSlot(RenderPassEvent ev, std::shared_ptr<IPass> pass);
        void assemblePassList();

        std::vector<PassSlot> m_slots;

        virtual void buildConfigPasses(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap) = 0;

        virtual void doRebuildResources(const AnalysisSceneResult& sceneData) = 0;

        virtual void onAfterCompile() {}

        virtual void onAfterCompileImGui() {}

        virtual void onBeforeGraphDestroy() {}

        bool buildGraph();
        std::unordered_map<std::string, RenderGraph::TextureId> buildTextureIdMap();
        bool hasSceneColorClearPass() const;

        void refreshSwapchainImport();

        void setIncrementalResize(bool enabled) { m_incrementalResize = enabled; }
        bool incrementalResize() const { return m_incrementalResize; }

        void buildPresentationPasses(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);
        void buildPresentationPass(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);
        bool compileAndFinalize(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);
        void preparePresentationExecutor();

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::shared_ptr<RenderGraph::RenderGraph> m_renderGraph;
        bool m_incrementalResize = true;          

        RenderPathConfig m_config;
        std::vector<OverlayPassDesc> m_overlayPasses;

        uint32_t m_width, m_height;
        const ParallelRecordingContext* m_parallel = nullptr;   
        bool m_resourceStatsPrinted = false;

        bool m_resizeRebuild = false;
        RHI::SamplerHandle m_defaultSampler;
        std::string m_swapchainTextureName = "Swapchain";
        std::string m_presentInputTexture = "SceneColor";
        RHI::Color m_presentClearColor = { 0.1f, 0.15f, 0.3f, 1.0f };

        // 子通道 → PassNode 映射（子类构建 config pass 时填充）
        std::unordered_map<long long, SubpassTarget> m_keyToSubpass;   ///< (layer<<32|queue) → subpass
        std::unordered_map<long long, RenderGraph::GraphNode*> m_keyToPassNode;

        RHI::DescriptorSetLayoutHandle  m_globalSetLayout;
        std::vector<RHI::DescriptorSetHandle> m_globalDescSets;

        std::unordered_map<std::string, RHI::TextureDesc> m_textureDescs;
        uint32_t m_lastSwapchainCount = 0;      ///< 上次见过的交换链图像数（检测重建）
        void*    m_lastSwapchainView0 = nullptr;///< 上次见过的首张 image view（检测重建）
        std::unordered_map<std::string, RenderGraph::TextureId> m_textureIdMap;

        PassList m_passes;
    };

} // namespace StarryEngine
