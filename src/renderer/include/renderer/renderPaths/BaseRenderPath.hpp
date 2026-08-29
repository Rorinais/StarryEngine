#pragma once
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

    class BaseRenderPath : public IRenderPath {
    public:
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

        void setPresentClearColor(const RHI::Color& color) { m_presentClearColor = color; }

        void setScene(Scene::Scene* scene) { m_blackboard.put<Scene::Scene*>(scene); }

        void addOverlayPass(const OverlayPassDesc& desc) override { m_overlayPasses.push_back(desc); }
        void removeOverlayPass(const std::string& tag) override;
        void clearOverlayPasses() override { m_overlayPasses.clear(); }
        const std::vector<OverlayPassDesc>& getOverlayPasses() const override { return m_overlayPasses; }

        void setPassList(PassList passes) { m_passes = std::move(passes); }
        std::shared_ptr<RenderGraph::RenderGraph> getRenderGraph() { return m_renderGraph; }

        void setParallelRecording(const ParallelRecordingContext* parallel) override { m_parallel = parallel; }

    protected:
        virtual void buildConfigPasses(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap) = 0;

        virtual void doRebuildResources(const AnalysisSceneResult& sceneData) = 0;

        virtual void onAfterCompile() {}

        virtual void onAfterCompileImGui() {}

        bool buildGraph();
        std::unordered_map<std::string, RenderGraph::TextureId> buildTextureIdMap();
        bool hasSceneColorClearPass() const;

        void buildPresentationPasses(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);
        void buildPresentationPass(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);
        bool compileAndFinalize(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);
        void preparePresentationExecutor();

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::shared_ptr<RenderGraph::RenderGraph> m_renderGraph;

        RenderPathConfig m_config;
        std::vector<OverlayPassDesc> m_overlayPasses;

        uint32_t m_width, m_height;
        const ParallelRecordingContext* m_parallel = nullptr;   
        bool m_resourceStatsPrinted = false;
        RHI::SamplerHandle m_defaultSampler;
        std::string m_swapchainTextureName = "Swapchain";
        RHI::Color m_presentClearColor = { 0.1f, 0.15f, 0.3f, 1.0f };

        // 子通道 → PassNode 映射（子类构建 config pass 时填充）
        std::unordered_map<std::string, SubpassTarget> m_tagToSubpass;
        std::unordered_map<std::string, RenderGraph::PassNode*> m_tagToPassNode;

        RHI::DescriptorSetLayoutHandle  m_globalSetLayout;
        std::vector<RHI::DescriptorSetHandle> m_globalDescSets;

        std::unordered_map<std::string, RHI::TextureDesc> m_textureDescs;
        std::unordered_map<std::string, RenderGraph::TextureId> m_textureIdMap;

        PassList m_passes;
    };

} // namespace StarryEngine
