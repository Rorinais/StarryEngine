#pragma once
#include "IRenderPath.hpp"
#include "../graph/RenderGraph.hpp"
#include "../../scene/Scene.hpp"
#include "../passExecutor/IPassExecutor.hpp"
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

    // BaseRenderPath — Forward/Deferred 共用逻辑
    // 子类只需重写 buildConfigPasses()，其余由基类处理
    class BaseRenderPath : public IRenderPath {
    public:
        BaseRenderPath(std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height);
        ~BaseRenderPath() override = default;

        // IRenderPath
        bool initialize() override;
        void onResize(uint32_t width, uint32_t height) override;
        void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) override;
        void rebuildResources(const Scene::AnalysisSceneResult& sceneData) override;

        void addTextureDesc(std::string name, RHI::TextureDesc desc);
        void setTextureDescs(const std::unordered_map<std::string, RHI::TextureDesc>& descs);

        void setPresentationDescriptorData(RHI::DescriptorSetLayoutHandle globalSetLayout,
                                           RHI::DescriptorSetHandle globalDescSet);

        // Overlay 管理
        void addOverlayPass(const OverlayPassDesc& desc) override { m_overlayPasses.push_back(desc); }
        void removeOverlayPass(const std::string& tag) override;
        void clearOverlayPasses() override { m_overlayPasses.clear(); }
        const std::vector<OverlayPassDesc>& getOverlayPasses() const override { return m_overlayPasses; }

        std::shared_ptr<RenderGraph::RenderGraph> getRenderGraph() { return m_renderGraph; }

    protected:
        // ── 子类必须实现 ──
        // 从 JSON 或代码构建渲染 Pass
        virtual void buildConfigPasses(
            std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap) = 0;

        // 资源重建（材质纹理绑定 + 管线创建）
        virtual void doRebuildResources(const Scene::AnalysisSceneResult& sceneData) = 0;

        // 编译后回调（子类可在此创建自己的内置管线）
        virtual void onAfterCompile() {}

        // ImGui 初始化（需要 renderPass handle，必须在 compile 后调用）
        virtual void onAfterCompileImGui() {}

        // ── 子类可复用 ──
        bool buildGraph();
        std::unordered_map<std::string, RenderGraph::TextureId> buildTextureIdMap();

        // 呈现层：自动管理 Swapchain 输出
        void buildPresentationPasses(
            std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);
        void buildPresentationPass(
            std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);

        // 编译 + 内建 Pass 管线
        bool compileAndFinalize(
            std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap);
        void ensurePresentationShaders();
        void preparePresentationPipeline();

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

        // 子通道 → PassNode 映射（子类构建 config pass 时填充）
        std::unordered_map<std::string, SubpassTarget> m_tagToSubpass;
        std::unordered_map<std::string, RenderGraph::PassNode*> m_tagToPassNode;

        RHI::DescriptorSetLayoutHandle  m_globalSetLayout;
        RHI::DescriptorSetHandle        m_globalDescSet;

        std::unordered_map<std::string, RHI::TextureDesc> m_textureDescs;
        std::unordered_map<std::string, RenderGraph::TextureId> m_textureIdMap;

    private:
        // ── 呈现层管道 ──
        RHI::ShaderHandle               m_fullscreenVert;
        RHI::ShaderHandle               m_copyFrag;
        RHI::DescriptorSetLayoutHandle  m_presentSceneColorLayout;
        RHI::PipelineLayoutHandle       m_presentPipelineLayout;
        RHI::DescriptorSetHandle        m_presentSceneColorDescSet;
        RHI::DescriptorPoolHandle       m_presentSceneColorPool;       // 需显式销毁
        bool m_presentationShadersReady = false;
        bool m_presentationPipelineReady = false;
    };

} // namespace StarryEngine
