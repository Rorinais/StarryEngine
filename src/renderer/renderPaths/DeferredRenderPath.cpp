#include "DeferredRenderPath.hpp"
#include "../../logging/Logger.hpp"
#include "../../ui/ImGuiManager.hpp"
#include "../../assets/loader/ShaderLoader.hpp"
#include <algorithm>

namespace StarryEngine {

    // ──── 内置全屏 Blit Recorder（PresentationPass 专用） ──────────────
    // 画一个覆盖 NDC 的全屏三角形，采样 SceneColor 纹理输出到 Swapchain。
    // 使用 fullscreen.vert（gl_VertexIndex 驱动）+ copy.frag（sampler2D）。
    class PresentationRecorder : public ISubpassRecorder {
    public:
        PresentationRecorder(RHI::PipelineHandle pipeline,
                             RHI::PipelineLayoutHandle layout,
                             RHI::DescriptorSetHandle globalSet,
                             RHI::DescriptorSetHandle sceneColorSet)
            : m_pipeline(pipeline), m_layout(layout),
              m_globalSet(globalSet), m_sceneColorSet(sceneColorSet) {}

        void setPipeline(RHI::PipelineHandle p)   { m_pipeline = p; }
        void setSceneColorSet(RHI::DescriptorSetHandle s) { m_sceneColorSet = s; }
        void setLayout(RHI::PipelineLayoutHandle l) { m_layout = l; }

        // ── ISubpassRecorder 接口 ──
        void clearDrawItems() override {}
        void setDrawItems(const std::vector<std::shared_ptr<Scene::DrawItem>>&) override {}
        const std::vector<std::shared_ptr<Scene::DrawItem>>& getDrawItems() override { return m_empty; }
        void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>&) override {}
        void addDrawItem(std::shared_ptr<Scene::DrawItem>) override {}

        void recordCommands(RHI::RHICommandEncoder* encoder,
                            const RenderContext& /*rctx*/,
                            const PassContext& pctx,
                            uint32_t /*subpassIndex*/) override
        {
            if (!m_pipeline.isValid()) return;

            auto resMgr = pctx.getResourceManager();
            auto* pipeline = resMgr->getPipeline(m_pipeline);
            if (!pipeline) return;
            encoder->bindPipeline(pipeline);

            auto* playout = resMgr->getPipelineLayout(m_layout);
            if (playout) {
                if (m_globalSet.isValid())
                    encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics,
                        playout, 0, { m_globalSet }, {});
                if (m_sceneColorSet.isValid())
                    encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics,
                        playout, 1, { m_sceneColorSet }, {});
            }

            // 全屏三角形：3 顶点，无顶点缓冲（gl_VertexIndex 驱动）
            encoder->draw(3, 1, 0, 0);
        }

    private:
        RHI::PipelineHandle       m_pipeline;
        RHI::PipelineLayoutHandle m_layout;
        RHI::DescriptorSetHandle  m_globalSet;       // set=0: GlobalUniforms
        RHI::DescriptorSetHandle  m_sceneColorSet;   // set=1: SceneColor sampler
        std::vector<std::shared_ptr<Scene::DrawItem>> m_empty;
    };

    // ──── 粒子 Compute Recorder ────────────────────────────────────
    class ParticleCSRecorder : public ISubpassRecorder {
    public:
        ParticleCSRecorder(RHI::PipelineLayoutHandle layout, RHI::DescriptorSetHandle descSet,
                           uint32_t particleCount, const ParticleParams& params)
            : m_layout(layout), m_descSet(descSet),
              m_particleCount(particleCount), m_params(params) {}
        void clearDrawItems() override {}
        void setDrawItems(const std::vector<std::shared_ptr<Scene::DrawItem>>&) override {}
        const std::vector<std::shared_ptr<Scene::DrawItem>>& getDrawItems() override { return m_empty; }
        void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>&) override {}
        void addDrawItem(std::shared_ptr<Scene::DrawItem>) override {}
        void recordCommands(RHI::RHICommandEncoder* encoder, const RenderContext& rctx,
                            const PassContext& pctx, uint32_t) override {
            auto resMgr = pctx.getResourceManager();
            auto* plo = resMgr->getPipelineLayout(m_layout);
            if (plo && m_descSet.isValid())
                encoder->bindDescriptorSets(RHI::PipelineBindPoint::Compute, plo, 0, {m_descSet}, {});
            // push 全部 CS 参数（offset 0, 48 bytes）
            float dt = rctx.deltaTime > 0.0f ? rctx.deltaTime : 0.016f;
            uint32_t n = m_particleCount;
            struct CS_PC { float dt; uint32_t n; float g, smin, smax, life, sxz, sf, sa, ey, td, tt; } pc;
            pc = {dt, n, m_params.gravity, m_params.speedMin, m_params.speedMax,
                  m_params.lifetime, m_params.spreadXZ, m_params.swayFreq, m_params.swayAmp,
                  m_params.emitterY, m_params.topDiffuse, m_params.topThreshold};
            encoder->pushConstants(plo, RHI::ShaderStage::Compute, 0, sizeof(pc), &pc);
        }
    private:
        RHI::PipelineLayoutHandle m_layout;
        RHI::DescriptorSetHandle m_descSet;
        uint32_t m_particleCount;
        ParticleParams m_params;
        std::vector<std::shared_ptr<Scene::DrawItem>> m_empty;
    };

    // ──── 粒子 Render Recorder ─────────────────────────────────────
    class ParticleRenderRecorder : public ISubpassRecorder {
    public:
        ParticleRenderRecorder(RHI::PipelineHandle pipeline, RHI::PipelineLayoutHandle layout,
                               RHI::DescriptorSetHandle globalSet, RHI::DescriptorSetHandle particleSet,
                               uint32_t count)
            : m_pipeline(pipeline), m_layout(layout),
              m_globalSet(globalSet), m_particleSet(particleSet), m_count(count) {}

        void setPipeline(RHI::PipelineHandle p)       { m_pipeline = p; }
        void setParticleSet(RHI::DescriptorSetHandle s) { m_particleSet = s; }

        void clearDrawItems() override {}
        void setDrawItems(const std::vector<std::shared_ptr<Scene::DrawItem>>&) override {}
        const std::vector<std::shared_ptr<Scene::DrawItem>>& getDrawItems() override { return m_empty; }
        void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>&) override {}
        void addDrawItem(std::shared_ptr<Scene::DrawItem>) override {}

        void recordCommands(RHI::RHICommandEncoder* encoder, const RenderContext&,
                            const PassContext& pctx, uint32_t) override {
            if (!m_pipeline.isValid()) return;
            auto resMgr = pctx.getResourceManager();
            auto* ppl = resMgr->getPipeline(m_pipeline);
            if (!ppl) return;
            encoder->bindPipeline(ppl);
            auto* plo = resMgr->getPipelineLayout(m_layout);
            if (plo) {
                if (m_globalSet.isValid())
                    encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, plo, 0, {m_globalSet}, {});
                if (m_particleSet.isValid())
                    encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, plo, 1, {m_particleSet}, {});
                // push VS 渲染参数（offset 64）
                encoder->pushConstants(plo, RHI::ShaderStage::Vertex, 64, sizeof(m_vsPC), &m_vsPC);
            }
            encoder->draw(m_count, 1, 0, 0);
        }
        // VS push constant data（offset 64+，与 shader layout 一致）
        struct VS_PC {
            float colorYoung[4];
            float colorMiddle[4];
            float colorOld[4];
            float pointSizeMin;
            float pointSizeMax;
        };
        void setVSParams(const ParticleParams& p) {
            memcpy(m_vsPC.colorYoung,  p.colorYoung,  sizeof(m_vsPC.colorYoung));
            memcpy(m_vsPC.colorMiddle, p.colorMiddle, sizeof(m_vsPC.colorMiddle));
            memcpy(m_vsPC.colorOld,    p.colorOld,    sizeof(m_vsPC.colorOld));
            m_vsPC.pointSizeMin = p.pointSizeMin;
            m_vsPC.pointSizeMax = p.pointSizeMax;
        }

    private:
        RHI::PipelineHandle       m_pipeline;
        RHI::PipelineLayoutHandle m_layout;
        RHI::DescriptorSetHandle  m_globalSet, m_particleSet;
        uint32_t                  m_count;
        VS_PC                     m_vsPC = {};
        std::vector<std::shared_ptr<Scene::DrawItem>> m_empty;
    };

    // ──── DeferredRenderPath 实现 ────

    DeferredRenderPath::DeferredRenderPath(std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height)
        : m_rhi(rhi), m_resMgr(rhi->getResourceManager()), m_width(width), m_height(height) {

        RHI::SamplerDesc samplerDesc;
        samplerDesc.minFilter = RHI::SamplerFilter::Linear;
        samplerDesc.magFilter = RHI::SamplerFilter::Linear;
        m_defaultSampler = m_resMgr->createSampler(samplerDesc);
    }

    void DeferredRenderPath::setConfig(const RenderPathConfig& config) {
        m_config = config;
    }

    void DeferredRenderPath::setDrawItems(const Scene::AnalysisSceneResult& sceneData) {
        m_cachedSceneData = std::make_shared<Scene::AnalysisSceneResult>(sceneData);
        updateMaterialTextures(sceneData);
        distributeDrawItems(sceneData);
    }

    void DeferredRenderPath::setTextureDescs(const std::unordered_map<std::string, RHI::TextureDesc>& descs) {
        m_textureDescs = descs;
    }

    void DeferredRenderPath::addTextureDesc(std::string name, RHI::TextureDesc desc) {
        m_textureDescs[name] = desc;
    }

    void DeferredRenderPath::setImGuiManager(ImGuiManager* mgr, uint32_t imageCount) {
        m_imguiManager = mgr;
        m_imguiImageCount = imageCount;
    }

    void DeferredRenderPath::render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) {
        if (!m_renderGraph) return;

        auto context = buildRenderContext();
        m_renderGraph->execute(encoder, context, frameIndex);
    }

    void DeferredRenderPath::onResize(uint32_t width, uint32_t height) {
        m_width = width;
        m_height = height;
        for (auto& [name, desc] : m_textureDescs) {
            desc.extent.width = width;
            desc.extent.height = height;
        }
        if (!initialize()) {
            LOG_ERROR("Failed to rebuild render path on resize");
            return;
        }
        if (m_cachedSceneData) {
            rebuildResources(*m_cachedSceneData);
        }
    }

    bool DeferredRenderPath::initialize() {
        if (m_config.empty()) {
            LOG_ERROR("DeferredRenderPath: No config set!");
            return false;
        }
        if (m_textureDescs.empty()) {
            LOG_ERROR("DeferredRenderPath: No texture descriptions set!");
            return false;
        }
        try {
            buildGraph();
        }
        catch (const std::exception& e) {
            LOG_ERROR("RenderGraph initialization failed: {}", e.what());
            return false;
        }
        return true;
    }

    bool DeferredRenderPath::buildGraph() {
        m_tagToSubpass.clear();
        m_tagToPassNode.clear();

        // 重置 ready 标志：resize 时 render pass / 物理纹理全部重建，
        // 管线和描述符集必须重新创建
        m_presentationPipelineReady = false;
        m_presentSceneColorDescSet = RHI::DescriptorSetHandle{};  // 旧 set 失效
        m_particleVS = RHI::ShaderHandle{};                       // 触发重新加载

        if (m_imguiManager) {
            m_imguiManager->setRenderGraph(nullptr);
        }
        m_renderGraph.reset();

        m_renderGraph = std::make_shared<RenderGraph::RenderGraph>(m_rhi);
        m_renderGraph->setSwapchainImageCount(m_rhi->getSwapChainImageCount());

        auto texIdMap = buildTextureIdMap();
        buildConfigPasses(texIdMap);
        buildPresentationPasses(texIdMap);
        buildTestComputePass();  // 测试 Compute Pass
        return compileAndFinalize(texIdMap);
    }

    std::unordered_map<std::string, RenderGraph::TextureId> DeferredRenderPath::buildTextureIdMap() {
        std::unordered_map<std::string, RenderGraph::TextureId> texIdMap;
        for (const auto& [name, desc] : m_textureDescs) {
            try {
                if (name == m_swapchainTextureName) {
                    std::vector<void*> views;
                    for (uint32_t i = 0; i < m_rhi->getSwapChainImageCount(); ++i) {
                        views.push_back(m_rhi->getSwapChainImageView(i));
                    }
                    auto id = m_renderGraph->importExternalTexture(
                        RHI::TextureHandle::Null(), views, desc,
                        RHI::ImageLayout::Undefined, name);
                    texIdMap[name] = id;
                }
                else {
                    auto id = m_renderGraph->createVirtualTexture(desc, name);
                    texIdMap[name] = id;
                }
            }
            catch (const std::exception& e) {
                LOG_ERROR("Failed to create texture '{}': {}", name, e.what());
                throw;
            }
        }
        return texIdMap;
    }

    void DeferredRenderPath::buildConfigPasses(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap){
        for (const auto& passDesc : m_config) {
            auto* passNode = m_renderGraph->addGraphicsPassNode(passDesc.name);
            passNode->setRenderArea(m_width, m_height);

            // 用于该 Pass 内附件去重：按纹理 ID + 用途分类存储已注册的 key
            std::unordered_map<RenderGraph::TextureId, std::string> colorKeyMap;
            std::unordered_map<RenderGraph::TextureId, std::string> depthKeyMap;
            std::unordered_map<RenderGraph::TextureId, std::string> inputKeyMap;
            std::unordered_map<RenderGraph::TextureId, std::string> resolveKeyMap;
            std::unordered_map<RenderGraph::TextureId, std::string> preserveKeyMap;

            // 5. 遍历该 Pass 内的 Subpass
            for (size_t subpassIdx = 0; subpassIdx < passDesc.subpasses.size(); ++subpassIdx) {
                const auto& subpassCfg = passDesc.subpasses[subpassIdx];

                auto& subpassBuilder = passNode->addSubpass(subpassCfg.name);
                subpassBuilder.setTag(subpassCfg.tag);

                // ---- 颜色附件 ----
                std::vector<std::string> colorKeys;
                for (auto& att : subpassCfg.colorAttachments) {
                    auto texId = texIdMap.at(att.textureName);
                    auto& key = colorKeyMap[texId];
                    if (key.empty()) {
                        key = passNode->addColorOutput(texId, att.params);   // 获取 PassNode 生成的实际 key
                    }
                    colorKeys.push_back(key);
                }

                // ---- 深度附件 ----
                std::string depthKey;
                if (subpassCfg.depthAttachment) {
                    auto& att = *subpassCfg.depthAttachment;
                    auto texId = texIdMap.at(att.textureName);
                    auto& key = depthKeyMap[texId];
                    if (key.empty()) {
                        key = passNode->addDepthOutput(texId, att.params);
                    }
                    depthKey = key;
                }

                // ---- 输入附件 ----
                std::vector<std::string> inputKeys;
                for (auto& att : subpassCfg.inputAttachments) {
                    auto texId = texIdMap.at(att.textureName);
                    auto& key = inputKeyMap[texId];
                    if (key.empty()) {
                        key = passNode->addInput(texId, att.params);
                    }
                    inputKeys.push_back(key);
                }

                // ---- 解析附件 ----
                std::vector<std::string> resolveKeys;
                for (auto& att : subpassCfg.resolveAttachments) {
                    auto texId = texIdMap.at(att.textureName);
                    auto& key = resolveKeyMap[texId];
                    if (key.empty()) {
                        key = passNode->addResolve(texId, att.params);
                    }
                    resolveKeys.push_back(key);
                }

                // ---- 保留附件 ----
                std::vector<std::string> preserveKeys;
                for (auto& texName : subpassCfg.preserveAttachments) {
                    auto texId = texIdMap.at(texName);
                    auto& key = preserveKeyMap[texId];
                    if (key.empty()) {
                        key = passNode->addPreserve(texId);   // preserve 无 params
                    }
                    preserveKeys.push_back(key);
                }

                // 将附件 key 绑定到当前 Subpass
                for (auto& k : colorKeys) subpassBuilder.addColorAttachmentRef(k);
                if (!depthKey.empty()) subpassBuilder.addDepthStencilAttachmentRef(depthKey);
                for (auto& k : inputKeys) subpassBuilder.addInputAttachmentRef(k);
                for (auto& k : resolveKeys) subpassBuilder.addResolveAttachmentRef(k);
                for (auto& k : preserveKeys) subpassBuilder.addPreserveAttachmentRef(k);

                // 设置 Recorder 及建立标签映射
                subpassBuilder.setRecorder(subpassCfg.recorder);
                m_tagToSubpass[subpassCfg.tag] = SubpassTarget{
                    {},
                    static_cast<uint32_t>(subpassIdx),
                    subpassCfg.recorder
                };
                m_tagToPassNode[subpassCfg.tag] = passNode;
            }
        }
    }

    // ──── 呈现层入口 ────────────────────────────────────────────────
    // 有 overlay → 构建 overlay passes（现有逻辑）
    // 无 overlay → 构建 PresentationPass（SceneColor → Swapchain）
    // 这样做保证了：无论 overlay 存在与否，Swapchain 始终有写入者，
    // Pass Culling 也有正确的"根"节点可以反向遍历。
    void DeferredRenderPath::buildPresentationPasses(
        std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap)
    {
        if (!m_overlayPasses.empty()) {
            // ── 有 overlay：构建 overlay passes ──
            for (const auto& overlay : m_overlayPasses) {
                auto* passNode = m_renderGraph->addGraphicsPassNode(overlay.tag + "Pass");
                passNode->setRenderArea(m_width, m_height);

                auto& subpassBuilder = passNode->addSubpass(overlay.tag);
                subpassBuilder.setTag(overlay.tag);

                // ---- Swapchain 颜色附件 ----
                RenderGraph::AttachmentParams scParams;
                scParams.loadOp        = RHI::AttachmentLoadOp::Clear;
                scParams.storeOp       = RHI::AttachmentStoreOp::Store;
                scParams.initialLayout = RHI::ImageLayout::Undefined;
                scParams.finalLayout   = RHI::ImageLayout::PresentSrc;
                scParams.clearColor    = { 0.08f, 0.08f, 0.10f, 1.0f };

                auto swapchainTexId = texIdMap.at(m_swapchainTextureName);
                std::string scKey = passNode->addColorOutput(swapchainTexId, scParams);
                subpassBuilder.addColorAttachmentRef(scKey);

                // ---- SceneColor 输入附件（制造依赖，确保顺序） ----
                RenderGraph::AttachmentParams inputParams;
                inputParams.loadOp        = RHI::AttachmentLoadOp::Load;
                inputParams.storeOp       = RHI::AttachmentStoreOp::DontCare;
                inputParams.initialLayout = RHI::ImageLayout::ShaderReadOnly;
                inputParams.finalLayout   = RHI::ImageLayout::ShaderReadOnly;

                auto sceneColorTexId = texIdMap.at("SceneColor");
                std::string inputKey = passNode->addInput(sceneColorTexId, inputParams);
                subpassBuilder.addInputAttachmentRef(inputKey);

                subpassBuilder.setRecorder(overlay.recorder);

                m_tagToSubpass[overlay.tag] = SubpassTarget{ {}, 0, overlay.recorder };
                m_tagToPassNode[overlay.tag] = passNode;
            }
        } else {
            // ── 无 overlay：构建 PresentationPass ──
            buildPresentationPass(texIdMap);
        }
    }

    // ──── 内置 Presentation Pass（无 overlay 时的兜底） ──────────────
    // 职责：将 SceneColor（config passes 的最终输出）"搬运"到 Swapchain，
    //       使 Swapchain 始终有写入者。
    //
    // 当前使用一个占位 recorder——Swapchain 会被 Clear 到背景色。
    // 要启用完整的 SceneColor → Swapchain 全屏复制，请将已有的
    //   assets/shaders/deferred/fullscreen.vert
    //   assets/shaders/deferred/copy.frag
    // 与 CopyToSwapchainRecorder 组合，替换此处的空 recorder。
    //
    // 实现提示：
    //   1. 用 m_resMgr->createShader(...) 加载两个 shader
    //   2. 创建 GraphicsPipelineState（depthTest=false, cullMode=None）
    //   3. 添加 Procedural DrawItem（vertexCount=3）到 recorder
    //   4. 在 prepareAllPipelines 中为 "Presentation" tag 创建管线
    void DeferredRenderPath::buildPresentationPass(
        std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap)
    {
        const std::string tag = "Presentation";

        auto* passNode = m_renderGraph->addGraphicsPassNode("PresentationPass");
        passNode->setRenderArea(m_width, m_height);

        auto& subpassBuilder = passNode->addSubpass("PresentBlit");
        subpassBuilder.setTag(tag);

        // ---- Swapchain 颜色附件（最终输出） ----
        RenderGraph::AttachmentParams scParams;
        scParams.loadOp        = RHI::AttachmentLoadOp::Clear;      // TODO: 换为 Load 以保留 SceneColor
        scParams.storeOp       = RHI::AttachmentStoreOp::Store;
        scParams.initialLayout = RHI::ImageLayout::Undefined;
        scParams.finalLayout   = RHI::ImageLayout::PresentSrc;
        scParams.clearColor    = { 0.08f, 0.08f, 0.10f, 1.0f };

        auto swapchainTexId = texIdMap.at(m_swapchainTextureName);
        std::string scKey = passNode->addColorOutput(swapchainTexId, scParams);
        subpassBuilder.addColorAttachmentRef(scKey);

        // ---- SceneColor 输入附件（制造依赖，保持 DAG 完整） ----
        RenderGraph::AttachmentParams inputParams;
        inputParams.loadOp        = RHI::AttachmentLoadOp::Load;
        inputParams.storeOp       = RHI::AttachmentStoreOp::DontCare;
        inputParams.initialLayout = RHI::ImageLayout::ShaderReadOnly;
        inputParams.finalLayout   = RHI::ImageLayout::ShaderReadOnly;

        auto sceneColorTexId = texIdMap.at("SceneColor");
        std::string inputKey = passNode->addInput(sceneColorTexId, inputParams);
        subpassBuilder.addInputAttachmentRef(inputKey);

        // 内置全屏 blit recorder（具体的管线/描述符集 handle 在 preparePresentationPipeline 中设置）
        auto presentRecorder = std::make_shared<PresentationRecorder>(
            RHI::PipelineHandle{},
            RHI::PipelineLayoutHandle{},
            RHI::DescriptorSetHandle{},
            RHI::DescriptorSetHandle{}
        );
        subpassBuilder.setRecorder(presentRecorder);

        m_tagToSubpass[tag] = SubpassTarget{ {}, 0, presentRecorder };
        m_tagToPassNode[tag] = passNode;

        LOG_INFO("Built PresentationPass (no overlay — swapchain clear fallback)");
    }

    // ──── Overlay 动态管理 ──────────────────────────────────────────
    void DeferredRenderPath::removeOverlayPass(const std::string& tag) {
        auto it = std::find_if(m_overlayPasses.begin(), m_overlayPasses.end(),
            [&tag](const OverlayPassDesc& desc) { return desc.tag == tag; });
        if (it != m_overlayPasses.end()) {
            m_overlayPasses.erase(it);
            LOG_INFO("Removed overlay pass: {}", tag);
        }
    }

    // ──── 全屏 Blit 管线 ────────────────────────────────────────────
    void DeferredRenderPath::setPresentationDescriptorData(
        RHI::DescriptorSetLayoutHandle globalSetLayout,
        RHI::DescriptorSetHandle globalDescSet)
    {
        m_globalSetLayout = globalSetLayout;
        m_globalDescSet = globalDescSet;
    }

    void DeferredRenderPath::ensurePresentationShaders() {
        if (m_presentationShadersReady) return;
        // 全局描述符集数据必须已通过 setPresentationDescriptorData() 传入
        if (!m_globalSetLayout.isValid()) return;

        Assets::ShaderLoader loader(m_resMgr);

        // 加载全屏三角形顶点着色器（gl_VertexIndex 驱动，无需顶点缓冲）
        auto vsInfo = loader.loadFromFile("assets/shaders/deferred/fullscreen.vert",
                                          RHI::ShaderStage::Vertex);
        if (!vsInfo || !vsInfo->module.isValid()) {
            LOG_ERROR("Failed to load fullscreen.vert for PresentationPass");
            return;
        }
        m_fullscreenVert = vsInfo->module;

        // 加载 copy 片段着色器（采样 SceneColor 输出到 Swapchain）
        auto fsInfo = loader.loadFromFile("assets/shaders/deferred/copy.frag",
                                          RHI::ShaderStage::Fragment);
        if (!fsInfo || !fsInfo->module.isValid()) {
            LOG_ERROR("Failed to load copy.frag for PresentationPass");
            return;
        }
        m_copyFrag = fsInfo->module;

        // set=1 的布局：SceneColor 作为 combined image sampler
        RHI::DescriptorSetLayoutDesc set1Layout;
        set1Layout.bindings = {
            { 0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment }
        };
        m_presentSceneColorLayout = m_resMgr->createDescriptorSetLayout(set1Layout);
        if (!m_presentSceneColorLayout.isValid()) {
            LOG_ERROR("Failed to create SceneColor desc layout for PresentationPass");
            return;
        }

        // 管线布局：set=0（全局 UBO）+ set=1（SceneColor sampler）
        RHI::PipelineLayoutDesc playoutDesc;
        playoutDesc.descriptorSetLayouts = { m_globalSetLayout, m_presentSceneColorLayout };
        m_presentPipelineLayout = m_resMgr->createPipelineLayout(playoutDesc);
        if (!m_presentPipelineLayout.isValid()) {
            LOG_ERROR("Failed to create pipeline layout for PresentationPass");
            return;
        }

        m_presentationShadersReady = true;
        LOG_INFO("PresentationPass shaders loaded");
    }

    void DeferredRenderPath::preparePresentationPipeline() {
        if (!m_presentationShadersReady) ensurePresentationShaders();
        if (!m_presentationShadersReady) return;
        if (m_presentationPipelineReady) return;

        // 获取 PresentationPass 的 RenderPass（编译后才存在）
        auto tagIt = m_tagToSubpass.find("Presentation");
        if (tagIt == m_tagToSubpass.end()) return;

        RHI::RenderPassHandle rp = tagIt->second.renderPass;
        if (!rp.isValid()) return;

        // 构建 GraphicsPipelineState
        Scene::GraphicsPipelineState pso;
        pso.vertexShader   = m_fullscreenVert;
        pso.fragmentShader = m_copyFrag;
        pso.layout         = m_presentPipelineLayout;
        pso.cullMode       = RHI::CullMode::None;
        pso.frontFace      = RHI::FrontFace::CounterClockwise;
        pso.depthTestEnable  = false;
        pso.depthWriteEnable = false;
        pso.topology       = RHI::PrimitiveTopology::TriangleList;
        pso.dynamicStates  = { RHI::DynamicState::Viewport, RHI::DynamicState::Scissor };
        pso.vertexInput    = {};   // gl_VertexIndex 驱动，无顶点缓冲

        RHI::BlendAttachmentState blend;
        blend.blendEnable = false;
        pso.attachments = { blend };

        uint32_t subpassIdx = tagIt->second.subpassIndex;
        auto pipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
            m_resMgr.get(), pso, rp, subpassIdx);

        if (!pipeline.isValid()) {
            LOG_ERROR("Failed to create PresentationPass pipeline");
            return;
        }

        // 创建 set=1 描述符集：绑定 SceneColor 作为 sampler
        auto sceneColorIt = m_textureIdMap.find("SceneColor");
        if (sceneColorIt != m_textureIdMap.end()) {
            RHI::TextureHandle physHandle =
                m_renderGraph->getPhysicalTextureHandle(sceneColorIt->second);

            if (physHandle.isValid()) {
                auto* texObj = m_resMgr->getTexture(physHandle);
                auto* samplerObj = m_resMgr->getSampler(m_defaultSampler);

                if (texObj && samplerObj) {
                    RHI::DescriptorPoolDesc poolDesc;
                    poolDesc.maxSets = 1;
                    poolDesc.poolSizes = {
                        { RHI::DescriptorType::CombinedImageSampler, 1 }
                    };
                    auto pool = m_resMgr->createDescriptorPool(poolDesc);

                    RHI::DescriptorSetDesc setDesc;
                    setDesc.descriptorSetLayout = m_presentSceneColorLayout;
                    setDesc.descriptorPool = pool;
                    m_presentSceneColorDescSet = m_resMgr->createDescriptorSet(setDesc);

                    if (m_presentSceneColorDescSet.isValid()) {
                        auto* descSet = m_resMgr->getDescriptorSet(m_presentSceneColorDescSet);
                        if (descSet) {
                            descSet->writeTexture(0, 0, texObj, samplerObj,
                                RHI::ImageLayout::ShaderReadOnly);
                            descSet->update();
                        }
                    }
                }
            }
        }

        // 更新 recorder 中的管线
        auto recIt = m_tagToSubpass.find("Presentation");
        if (recIt != m_tagToSubpass.end() && recIt->second.recorder) {
            auto* rec = dynamic_cast<PresentationRecorder*>(recIt->second.recorder.get());
            if (rec) {
                rec->setPipeline(pipeline);
                rec->setLayout(m_presentPipelineLayout);
                rec->setSceneColorSet(m_presentSceneColorDescSet);
            }
        }

        m_presentationPipelineReady = true;
        LOG_INFO("PresentationPass pipeline ready");
    }

    // ── 粒子系统（Compute + Render，端到端验证）────────────────────
    void DeferredRenderPath::buildTestComputePass() {
        if (!m_globalSetLayout.isValid()) return;  // 首次 build 时 descriptor 数据未就绪
        const uint32_t PARTICLE_COUNT = 1024;
        Assets::ShaderLoader loader(m_resMgr);

        // ═══ 1. 粒子存储缓冲 ═══
        RHI::BufferDesc bufDesc;
        bufDesc.size = PARTICLE_COUNT * sizeof(float) * 4;
        bufDesc.type = RHI::BufferType::Storage;
        bufDesc.memoryType = RHI::MemoryType::CPU_To_GPU;  // host 可写，避免 GPU 未初始化值
        bufDesc.allowUpdate = true;
        auto bufId = m_renderGraph->createVirtualBuffer(bufDesc, "ParticleBuffer");

        // ═══ 2. Compute Shader + Pipeline ═══
        auto csInfo = loader.loadFromFile("assets/shaders/test/particle.comp",
                                          RHI::ShaderStage::Compute);
        if (!csInfo || !csInfo->module.isValid()) {
            LOG_WARN("Particle: failed to load particle.comp"); return;
        }
        // Layout: set=0 → storage buffer
        RHI::DescriptorSetLayoutDesc csLayoutDesc;
        csLayoutDesc.bindings = {{0, RHI::DescriptorType::StorageBuffer, 1, RHI::ShaderStage::Compute}};
        auto csDescLayout = m_resMgr->createDescriptorSetLayout(csLayoutDesc);
        RHI::PipelineLayoutDesc csPlDesc;
        csPlDesc.descriptorSetLayouts = { csDescLayout };
        csPlDesc.pushConstants = {{RHI::ShaderStage::Compute, 0, 48}};  // 12 floats
        auto csPlLayout = m_resMgr->createPipelineLayout(csPlDesc);
        RHI::ComputePipelineDesc cpDesc;
        cpDesc.computeShader = csInfo->module;
        cpDesc.pipelineLayoutHandle = csPlLayout;
        auto csPipeline = m_resMgr->createComputePipeline(cpDesc);

        // ═══ 3. Particle Render Shaders + Pipeline ═══
        auto vsInfo = loader.loadFromFile("assets/shaders/test/particle.vert", RHI::ShaderStage::Vertex);
        auto fsInfo = loader.loadFromFile("assets/shaders/test/particle.frag", RHI::ShaderStage::Fragment);
        if (!vsInfo || !fsInfo) { LOG_WARN("Particle: failed to load render shaders"); return; }
        // Layout: set=0 → 复用全局 GlobalUBO layout（确保与 m_globalDescSet 兼容）
        //         set=1 → storage buffer（ParticleBuffer）
        RHI::DescriptorSetLayoutDesc renderLayout1;
        renderLayout1.bindings = {{0, RHI::DescriptorType::StorageBuffer, 1, RHI::ShaderStage::Vertex}};
        auto renderLayout1H = m_resMgr->createDescriptorSetLayout(renderLayout1);
        RHI::PipelineLayoutDesc renderPlDesc;
        renderPlDesc.descriptorSetLayouts = { m_globalSetLayout, renderLayout1H };
        renderPlDesc.pushConstants = {{RHI::ShaderStage::Vertex, 64, 56}};  // VS 颜色+大小
        auto renderPlLayout = m_resMgr->createPipelineLayout(renderPlDesc);

        // ═══ 4. Compute Pass Node ═══
        auto* csPass = m_renderGraph->addComputePassNode("ParticleUpdate");
        csPass->addWriteBuffer(bufId);
        csPass->setComputePipeline(csPipeline);
        csPass->setDispatchSize((PARTICLE_COUNT + 255) / 256, 1, 1);

        // ═══ 5. Particle Render Pass Node ═══
        auto* renderPass = m_renderGraph->addGraphicsPassNode("ParticleRender");
        renderPass->setRenderArea(m_width, m_height);
        renderPass->addReadBuffer(bufId);  // 读 Compute 输出 → 依赖分析自动排序

        // 颜色输出到 SceneColor（保留已有场景内容）
        RenderGraph::AttachmentParams colorParams;
        colorParams.loadOp        = RHI::AttachmentLoadOp::Load;
        colorParams.storeOp       = RHI::AttachmentStoreOp::Store;
        colorParams.initialLayout = RHI::ImageLayout::ShaderReadOnly;
        colorParams.finalLayout   = RHI::ImageLayout::ShaderReadOnly;
        auto scId = m_renderGraph->getTextureId("SceneColor");
        std::string colorKey = renderPass->addColorOutput(scId, colorParams);

        auto& subpass = renderPass->addSubpass("ParticleDraw");
        subpass.addColorAttachmentRef(colorKey);
        subpass.setTag("ParticleDraw");

        // 点精灵渲染 pipeline（在 compileAndFinalize 中创建）
        m_particleVS = vsInfo->module;
        m_particleFS = fsInfo->module;
        m_particleRenderLayout = renderPlLayout;
        m_particleBufferId = bufId;
        m_particleCount = PARTICLE_COUNT;
        m_particleCSDescLayout = csDescLayout;
        m_particleCSLayout = csPlLayout;
        m_particleCSPipeline = csPipeline;

        // placeho lder recorder（compileAndFinalize 中替换为真实 pipeline）
        auto dummyRecorder = std::make_shared<CopyToSwapchainRecorder>();
        subpass.setRecorder(dummyRecorder);
        m_tagToSubpass["ParticleDraw"] = SubpassTarget{{}, 0, dummyRecorder};
        m_tagToPassNode["ParticleDraw"] = renderPass;

        LOG_INFO("Particle system built: {} particles, bufId={}, passes={}",
            PARTICLE_COUNT, bufId.id(), m_renderGraph->getPassCount());
    }

    // ── 粒子管线创建 ───────────────────────────────────────────────
    void DeferredRenderPath::prepareParticlePipeline() {
        // ── Compute Descriptor Set + Buffer Init ──
        auto physBuf = m_renderGraph->getPhysicalBuffer(m_particleBufferId);
        if (!physBuf.isValid()) return;

        // 初始化粒子数据（分散生命周期，避免同时喷发）
        auto* bufObj = m_resMgr->getBuffer(physBuf);
        if (bufObj) {
            std::vector<float> init(m_particleCount * 4, 0.0f);
            for (uint32_t i = 0; i < m_particleCount; ++i) {
                auto rnd = [](uint32_t s) { return float((s * 2654435761u) & 0xFFFF) / 65535.0f; };
                init[i*4 + 0] = (rnd(i*3+1) - 0.5f) * 0.8f;
                init[i*4 + 1] = rnd(i*3+2) * 3.0f - 3.0f;          // y: [-3, 0]
                init[i*4 + 2] = (rnd(i*3+3) - 0.5f) * 0.8f;
                init[i*4 + 3] = rnd(i*7+4) * 0.95f;                  // 随机初始寿命
            }
            bufObj->update(init.data(), init.size() * sizeof(float), 0);
        }

        RHI::DescriptorPoolDesc csPoolDesc;
        csPoolDesc.maxSets = 1;
        csPoolDesc.poolSizes = {{RHI::DescriptorType::StorageBuffer, 1}};
        auto csPool = m_resMgr->createDescriptorPool(csPoolDesc);
        RHI::DescriptorSetDesc csSetDesc;
        csSetDesc.descriptorSetLayout = m_particleCSDescLayout;
        csSetDesc.descriptorPool = csPool;
        auto csDescSet = m_resMgr->createDescriptorSet(csSetDesc);
        if (csDescSet.isValid()) {
            auto* ds = m_resMgr->getDescriptorSet(csDescSet);
            ds->writeBuffer(0, 0, m_resMgr->getBuffer(physBuf), 0,
                m_particleCount * sizeof(float) * 4);
            ds->update();
        }

        // 给 Compute Pass 加 recorder
        auto& passes = m_renderGraph->getPasses();
        for (auto& p : passes) {
            if (p->getName() == "ParticleUpdate") {
                p->setComputeRecorder(std::make_shared<ParticleCSRecorder>(
                    m_particleCSLayout, csDescSet, m_particleCount, m_particleParams));
                break;
            }
        }

        // ── Particle Render Pipeline ──
        auto tagIt = m_tagToSubpass.find("ParticleDraw");
        if (tagIt == m_tagToSubpass.end()) return;
        RHI::RenderPassHandle rp = tagIt->second.renderPass;
        if (!rp.isValid()) return;

        Scene::GraphicsPipelineState pso;
        pso.vertexShader   = m_particleVS;
        pso.fragmentShader = m_particleFS;
        pso.layout         = m_particleRenderLayout;
        pso.cullMode       = RHI::CullMode::None;
        pso.frontFace      = RHI::FrontFace::CounterClockwise;
        pso.depthTestEnable  = false;
        pso.depthWriteEnable = false;
        pso.topology       = RHI::PrimitiveTopology::PointList;
        pso.dynamicStates  = { RHI::DynamicState::Viewport, RHI::DynamicState::Scissor };
        pso.vertexInput    = {};
        RHI::BlendAttachmentState blend;
        blend.blendEnable = true;
        blend.srcColorBlendFactor = RHI::BlendFactor::SrcAlpha;
        blend.dstColorBlendFactor = RHI::BlendFactor::OneMinusSrcAlpha;
        blend.colorBlendOp = RHI::BlendOp::Add;
        pso.attachments = { blend };

        auto renderPipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
            m_resMgr.get(), pso, rp, tagIt->second.subpassIndex);
        if (!renderPipeline.isValid()) { LOG_ERROR("Particle render pipeline failed"); return; }

        // ── Particle Descriptor Set (set=1: buffer) ──
        RHI::DescriptorSetHandle particleDescSet;
        if (physBuf.isValid()) {
            // set=0 layout for render (uniform buffer)
            RHI::DescriptorSetLayoutDesc set0Desc;
            set0Desc.bindings = {{0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Vertex}};
            auto set0Layout = m_resMgr->createDescriptorSetLayout(set0Desc);

            // set=1 layout (storage buffer)
            RHI::DescriptorSetLayoutDesc set1Desc;
            set1Desc.bindings = {{0, RHI::DescriptorType::StorageBuffer, 1, RHI::ShaderStage::Vertex}};
            auto set1Layout = m_resMgr->createDescriptorSetLayout(set1Desc);

            RHI::DescriptorPoolDesc poolDesc;
            poolDesc.maxSets = 2;
            poolDesc.poolSizes = {
                {RHI::DescriptorType::UniformBuffer, 1},
                {RHI::DescriptorType::StorageBuffer, 1}};
            auto pool = m_resMgr->createDescriptorPool(poolDesc);

            RHI::DescriptorSetDesc dsDesc1;
            dsDesc1.descriptorSetLayout = set1Layout;
            dsDesc1.descriptorPool = pool;
            particleDescSet = m_resMgr->createDescriptorSet(dsDesc1);
            if (particleDescSet.isValid()) {
                auto* ds = m_resMgr->getDescriptorSet(particleDescSet);
                if (ds) {
                    ds->writeBuffer(0, 0, m_resMgr->getBuffer(physBuf), 0,
                        m_particleCount * sizeof(float) * 4);
                    ds->update();
                }
            }
        }

        // ── 替换占位 recorder → 真实 particle recorder ──
        auto rec = std::make_shared<ParticleRenderRecorder>(
            renderPipeline, m_particleRenderLayout,
            m_globalDescSet, particleDescSet, m_particleCount);
        rec->setVSParams(m_particleParams);  // 传入渲染参数（颜色+大小）
        tagIt->second.recorder = rec;
        // 同时更新 PassNode 内部的 subpassRecorders（execute 实际读取的位置）
        auto passIt = m_tagToPassNode.find("ParticleDraw");
        if (passIt != m_tagToPassNode.end()) {
            passIt->second->setSubpassRecorder(0, rec);
        }

        LOG_INFO("Particle render pipeline ready ({} points)", m_particleCount);
    }

    bool DeferredRenderPath::compileAndFinalize(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap){
        if (!m_renderGraph->compile()) {
            LOG_ERROR("Failed to compile RenderGraph");
            return false;
        }

        for (auto& [tag, target] : m_tagToSubpass) {
            auto passIt = m_tagToPassNode.find(tag);
            if (passIt != m_tagToPassNode.end()) {
                target.renderPass = passIt->second->getRenderPassHandle();
            }
        }

        if (m_imguiManager) {
            m_imguiManager->setRenderGraph(m_renderGraph);

            if (!m_imguiManager->isVulkanReady()) {
                auto it = m_tagToSubpass.find("ImGui");
                if (it != m_tagToSubpass.end()) {
                    m_imguiManager->initializeVulkanBackend(m_rhi.get(), m_resMgr.get(),
                        it->second.renderPass, m_imguiImageCount);
                }
            }
            m_imguiManager->setDefaultSampler(m_defaultSampler);
            m_imguiManager->registerSceneTexture(m_resMgr.get());
        }

        m_textureIdMap = std::move(texIdMap);

        // PresentationPass 管线
        if (m_tagToSubpass.count("Presentation") && m_globalSetLayout.isValid()) {
            ensurePresentationShaders();
            preparePresentationPipeline();
        }

        // 粒子系统管线
        if (m_tagToSubpass.count("ParticleDraw") && m_particleVS.isValid()) {
            prepareParticlePipeline();
        }

        return true;
    }

    void DeferredRenderPath::rebuildResources(const Scene::AnalysisSceneResult& sceneData) {
        // 先使用 sceneData 完成所有操作，最后再缓存副本
        // 否则 m_cachedSceneData 赋值会销毁旧数据 → sceneData 引用悬空
        updateMaterialTextures(sceneData);

        distributeDrawItems(sceneData);

        prepareAllPipelines(sceneData);

        m_cachedSceneData = std::make_shared<Scene::AnalysisSceneResult>(sceneData);

        if (!m_resourceStatsPrinted) {
            m_rhi->printResourceStatistics();
            m_resourceStatsPrinted = true;
        }
    }

    void DeferredRenderPath::updateMaterialTextures(const Scene::AnalysisSceneResult& sceneData) {
        if (!m_renderGraph || m_textureIdMap.empty()) {
            LOG_WARN("RenderGraph not ready for texture updates");
            return;
        }

        RHI::TextureHandle swapchainPhys;
        auto swapchainIt = m_textureIdMap.find("Swapchain");
        if (swapchainIt != m_textureIdMap.end()) {
            swapchainPhys = m_renderGraph->getPhysicalTextureHandle(swapchainIt->second);
        }

        for (auto& material : sceneData.materials) {
            if (!material) continue;

            for (const auto& [texName, dep] : material->getTextureDependencies()) {
                if (texName == "Swapchain") {
                    LOG_ERROR("Material illegally depends on Swapchain texture!");
                    continue;
                }

                auto texIt = m_textureIdMap.find(texName);
                if (texIt == m_textureIdMap.end()) {
                    LOG_WARN("Material requires texture '{}' not found in textureIdMap", texName);
                    continue;
                }
                auto phys = m_renderGraph->getPhysicalTextureHandle(texIt->second);
                if (!phys.isValid()) {
                    LOG_WARN("Material texture '{}' physical handle invalid", texName);
                    continue;
                }

                if (swapchainPhys.isValid() && phys == swapchainPhys) {
                    LOG_ERROR("Material BINDS SWAPCHAIN as '{}' (set={}, binding={})", texName, dep.set, dep.binding);
                    continue;
                }

                switch (dep.type) {
                case Assets::ResourceDependencyType::Sampler:
                    LOG_INFO("Material setTexture set={}, binding={}, texture='{}'", dep.set, dep.binding, texName);
                    material->setTexture(dep.set, dep.binding, phys, m_defaultSampler);
                    break;
                case Assets::ResourceDependencyType::InputAttachment:
                    LOG_INFO("Material setInputAttachment set={}, binding={}, texture='{}'", dep.set, dep.binding, texName);
                    material->setInputAttachment(dep.set, dep.binding, phys, RHI::ImageLayout::ShaderReadOnly);
                    break;
                }
            }
        }
    }

    void DeferredRenderPath::distributeDrawItems(const Scene::AnalysisSceneResult& sceneData) {
        for (auto& [tag, target] : m_tagToSubpass) {
            target.recorder->clearDrawItems();
        }

        std::string defaultTag = m_config.front().subpasses.front().tag;
        for (auto& item : sceneData.drawItems) {
            std::string tag = item->passTag;
            if (tag.empty()) {
                tag = defaultTag;
                LOG_WARN("DrawItem had empty passTag, assigned to default: {}", tag);
            }

            auto it = m_tagToSubpass.find(tag);
            if (it != m_tagToSubpass.end()) {
                it->second.recorder->addDrawItem(item);
            }
            else {
                LOG_ERROR("No subpass for tag '{}'", tag);
            }
        }
    }

    void DeferredRenderPath::prepareAllPipelines(const Scene::AnalysisSceneResult& sceneData) {
        for (auto& [tag, target] : m_tagToSubpass) {
            auto& items = target.recorder->getDrawItems();
            if (items.empty()) continue;

            std::unordered_set<uint32_t> usedIndices;
            for (auto& item : items) {
                if (item->pipelineIndex < sceneData.PSO.size())
                    usedIndices.insert(item->pipelineIndex);
            }

            std::unordered_map<uint32_t, RHI::PipelineHandle> mapping;
            for (uint32_t idx : usedIndices) {
                const auto& pso = sceneData.PSO[idx];
                auto pipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
                    m_resMgr.get(), *pso, target.renderPass, target.subpassIndex);
                if (pipeline.isValid()) mapping[idx] = pipeline;
                else LOG_ERROR("Failed to create pipeline for PSO index {}", idx);
            }
            target.recorder->setPipelineMapping(std::move(mapping));
        }
    }

} // namespace StarryEngine