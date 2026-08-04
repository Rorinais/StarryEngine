#pragma once
#include "IRenderPath.hpp"
#include "../graph/RenderGraph.hpp"
#include "../../scene/Scene.hpp"
#include "../RenderTypes.hpp"
#include "../passExecutor/IPassExecutor.hpp"
#include "../passes/IPass.hpp"
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
        void setPresentationDescriptorData(RHI::DescriptorSetLayoutHandle globalSetLayout,RHI::DescriptorSetHandle globalDescSet);

        // 透明窗口：present 清屏色可配（alpha=0 让桌面透过来）
        void setPresentClearColor(const RHI::Color& color) { m_presentClearColor = color; }

        void addOverlayPass(const OverlayPassDesc& desc) override { m_overlayPasses.push_back(desc); }
        void removeOverlayPass(const std::string& tag) override;
        void clearOverlayPasses() override { m_overlayPasses.clear(); }
        const std::vector<OverlayPassDesc>& getOverlayPasses() const override { return m_overlayPasses; }

        void setPassList(PassList passes) { m_passes = std::move(passes); }
        std::shared_ptr<RenderGraph::RenderGraph> getRenderGraph() { return m_renderGraph; }

    protected:
        virtual void buildConfigPasses(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap) = 0;

        virtual void doRebuildResources(const AnalysisSceneResult& sceneData) = 0;

        virtual void onAfterCompile() {}

        virtual void onAfterCompileImGui() {}

        bool buildGraph();
        std::unordered_map<std::string, RenderGraph::TextureId> buildTextureIdMap();

        // 呈现层：自动管理 Swapchain 输出
        void buildPresentationPasses(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);
        void buildPresentationPass(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);

        // 编译 + 内建 Pass 管线
        bool compileAndFinalize(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);
        void ensurePresentationShaders();
        void preparePresentationPipeline(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);

        // ── 子类可访问的数据 ──
        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::shared_ptr<RenderGraph::RenderGraph> m_renderGraph;

        RenderPathConfig m_config;
        std::vector<OverlayPassDesc> m_overlayPasses;

        uint32_t m_width, m_height;
        bool m_resourceStatsPrinted = false;
        RHI::SamplerHandle m_defaultSampler;
        std::string m_swapchainTextureName = "Swapchain";
        RHI::Color m_presentClearColor = { 0.08f, 0.08f, 0.10f, 1.0f };

        // 子通道 → PassNode 映射（子类构建 config pass 时填充）
        std::unordered_map<std::string, SubpassTarget> m_tagToSubpass;
        std::unordered_map<std::string, RenderGraph::PassNode*> m_tagToPassNode;

        RHI::DescriptorSetLayoutHandle  m_globalSetLayout;
        RHI::DescriptorSetHandle        m_globalDescSet;

        std::unordered_map<std::string, RHI::TextureDesc> m_textureDescs;
        std::unordered_map<std::string, RenderGraph::TextureId> m_textureIdMap;

        PassList m_passes;

    private:
        // ── 呈现层管道 ──
        RHI::ShaderHandle               m_fullscreenVert;
        RHI::ShaderHandle               m_copyFrag;
        RHI::DescriptorSetLayoutHandle  m_presentSceneColorLayout;
        RHI::PipelineLayoutHandle       m_presentPipelineLayout;
        RHI::DescriptorSetHandle        m_presentSceneColorDescSet;
        RHI::DescriptorPoolHandle       m_presentSceneColorPool;       
        bool m_presentationShadersReady = false;
        bool m_presentationPipelineReady = false;
    };

} // namespace StarryEngine
