#include <renderer/renderPaths/BaseRenderPath.hpp>
#include <renderer/passExecutor/PresentationExecutor.hpp>
#include <logging/Logger.hpp>
#include <assets/loader/ShaderLoader.hpp>
#include <algorithm>

namespace StarryEngine {

    // ──── BaseRenderPath ────────────────────────────────────────────

    BaseRenderPath::BaseRenderPath(std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height)
        : m_rhi(rhi), m_resMgr(rhi->getResourceManager()), m_width(width), m_height(height) {
        RHI::SamplerDesc samplerDesc;
        samplerDesc.minFilter = RHI::SamplerFilter::Linear;
        samplerDesc.magFilter = RHI::SamplerFilter::Linear;
        m_defaultSampler = m_resMgr->createSampler(samplerDesc);
    }

    void BaseRenderPath::addTextureDesc(std::string name, RHI::TextureDesc desc) {
        m_textureDescs[name] = desc;
    }

    void BaseRenderPath::setTextureDescs(const std::unordered_map<std::string, RHI::TextureDesc>& descs) {
        m_textureDescs = descs;
    }

    void BaseRenderPath::setPresentationDescriptorData(
        RHI::DescriptorSetLayoutHandle globalSetLayout,
        std::vector<RHI::DescriptorSetHandle> globalDescSets) {
        m_globalSetLayout = globalSetLayout;
        m_globalDescSets = std::move(globalDescSets);
    }

    void BaseRenderPath::removeOverlayPass(const std::string& tag) {
        auto it = std::find_if(m_overlayPasses.begin(), m_overlayPasses.end(),
            [&tag](const OverlayPassDesc& desc) { return desc.tag == tag; });
        if (it != m_overlayPasses.end()) {
            m_overlayPasses.erase(it);
            LOG_INFO("Removed overlay pass: {}", tag);
        }
    }

    // ──── 生命周期 ──────────────────────────────────────────────────

    bool BaseRenderPath::initialize() {
        if (m_textureDescs.empty()) { LOG_ERROR("No texture descs set!"); return false; }
        try { return buildGraph(); }
        catch (const std::exception& e) {
            LOG_ERROR("RenderGraph init failed: {}", e.what());
            return false;
        }
    }

    void BaseRenderPath::onResize(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) return;
        uint32_t oldW = m_width, oldH = m_height;
        m_width = width; m_height = height;
        for (auto& [name, desc] : m_textureDescs) {
            //只更新跟随窗口变化的纹理，阴影纹理大小不跟新
            if (desc.extent.width == oldW && desc.extent.height == oldH) {
                desc.extent.width = width; desc.extent.height = height;
            }
        }
        if (!initialize()) { LOG_ERROR("Failed to rebuild on resize"); return; }
    }

    void BaseRenderPath::render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) {
        if (!m_renderGraph) return;
        auto context = buildRenderContext();
        // 帧槽位（ADR-6）：render() 在 renderFrame 内调用，此时 getCurrentFrameIndex() == 本帧槽位。
        // 录制 job 按值捕获 context（含 frameSlot），executor 绑 slot 对应的描述符集/实例缓冲。
        context.frameSlot = m_rhi->getCurrentFrameIndex();
        if (context.frameSlot >= RHI::kMaxFramesInFlight) context.frameSlot = 0;
        m_renderGraph->execute(encoder, context, frameIndex, context.frameSlot, m_parallel);
    }

    void BaseRenderPath::rebuildResources(const AnalysisSceneResult& sceneData) {
        doRebuildResources(sceneData);
        m_cachedSceneData = std::make_shared<AnalysisSceneResult>(sceneData);
        if (!m_resourceStatsPrinted) {
            m_rhi->printResourceStatistics();
            m_resourceStatsPrinted = true;
        }
    }

    // ──── Build Graph ──────────────────────────────────────────────

    bool BaseRenderPath::buildGraph() {
        m_tagToSubpass.clear();
        m_tagToPassNode.clear();

        if (m_renderGraph) {
            m_rhi->waitIdle();
            Assets::PipelineCache::invalidateAll(m_resMgr.get());
            m_renderGraph.reset();
        }
        m_renderGraph = std::make_shared<RenderGraph::RenderGraph>(m_rhi);
        m_renderGraph->setSwapchainImageCount(m_rhi->getSwapChainImageCount());

        auto texIdMap = buildTextureIdMap();
        buildConfigPasses(texIdMap);         // 子类实现
        buildPresentationPasses(texIdMap);    // 基类实现
        return compileAndFinalize(texIdMap);
    }

    std::unordered_map<std::string, RenderGraph::TextureId> BaseRenderPath::buildTextureIdMap() {
        std::unordered_map<std::string, RenderGraph::TextureId> texIdMap;
        for (const auto& [name, desc] : m_textureDescs) {
            try {
                if (name == m_swapchainTextureName) {
                    std::vector<void*> views;
                    for (uint32_t i = 0; i < m_rhi->getSwapChainImageCount(); ++i)
                        views.push_back(m_rhi->getSwapChainImageView(i));
                    auto id = m_renderGraph->importExternalTexture(
                        RHI::TextureHandle::Null(), views, desc,
                        RHI::ImageLayout::Undefined, name);
                    texIdMap[name] = id;
                } else {
                    auto id = m_renderGraph->createVirtualTexture(desc, name);
                    texIdMap[name] = id;
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to create texture '{}': {}", name, e.what());
                throw;
            }
        }
        return texIdMap;
    }

    // ──── 呈现层 ────────────────────────────────────────────────────

    void BaseRenderPath::buildPresentationPasses(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap) {
        if (!m_overlayPasses.empty()) {
            for (const auto& overlay : m_overlayPasses) {
                auto* passNode = m_renderGraph->addGraphicsPassNode(overlay.tag + "Pass");
                passNode->setRenderArea(m_width, m_height);
                auto& subpassBuilder = passNode->addSubpass(overlay.tag);
                subpassBuilder.setTag(overlay.tag);

                // 颜色输出（每个 overlay 自己声明）
                for (auto& [texName, params] : overlay.colorOutputs) {
                    std::string key = passNode->addColorOutput(texIdMap.at(texName), params);
                    subpassBuilder.addColorAttachmentRef(key);
                }
                // 深度输出（可选）
                if (overlay.depthOutput) {
                    std::string key = passNode->addDepthOutput(
                        texIdMap.at(overlay.depthOutput->first), overlay.depthOutput->second);
                    subpassBuilder.addDepthStencilAttachmentRef(key);
                }
                // 输入附件
                for (auto& [texName, params] : overlay.inputAttachments) {
                    std::string key = passNode->addInput(texIdMap.at(texName), params);
                    subpassBuilder.addInputAttachmentRef(key);
                }

                subpassBuilder.setExecutor(overlay.executor);
                m_tagToSubpass[overlay.tag] = SubpassTarget{ {}, 0, overlay.executor };
                m_tagToPassNode[overlay.tag] = passNode;
            }
        } else {
            buildPresentationPass(texIdMap);
        }
    }

    void BaseRenderPath::buildPresentationPass(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap) {
        const std::string tag = "Presentation";
        auto* passNode = m_renderGraph->addGraphicsPassNode("PresentationPass");
        passNode->setRenderArea(m_width, m_height);
        auto& subpassBuilder = passNode->addSubpass("PresentBlit");
        subpassBuilder.setTag(tag);

        RenderGraph::AttachmentParams scParams;
        scParams.loadOp = RHI::AttachmentLoadOp::Clear;
        scParams.storeOp = RHI::AttachmentStoreOp::Store;
        scParams.initialLayout = RHI::ImageLayout::Undefined;
        scParams.finalLayout = RHI::ImageLayout::PresentSrc;
        scParams.clearColor = m_presentClearColor;
        auto swapchainTexId = texIdMap.at(m_swapchainTextureName);
        std::string scKey = passNode->addColorOutput(swapchainTexId, scParams);
        subpassBuilder.addColorAttachmentRef(scKey);

        RenderGraph::AttachmentParams inputParams;
        inputParams.loadOp = RHI::AttachmentLoadOp::Load;
        inputParams.storeOp = RHI::AttachmentStoreOp::DontCare;
        inputParams.initialLayout = RHI::ImageLayout::ShaderReadOnly;
        inputParams.finalLayout = RHI::ImageLayout::ShaderReadOnly;
        auto sceneColorTexId = texIdMap.at("SceneColor");
        std::string inputKey = passNode->addInput(sceneColorTexId, inputParams);
        subpassBuilder.addInputAttachmentRef(inputKey);

        auto rec = std::make_shared<PresentationExecutor>();
        subpassBuilder.setExecutor(rec);
        m_tagToSubpass[tag] = SubpassTarget{ {}, 0, rec };
        m_tagToPassNode[tag] = passNode;
        LOG_INFO("Built PresentationPass (no overlay)");
    }

    // ──── 编译 + 内建管道 ──────────────────────────────────────────

    bool BaseRenderPath::compileAndFinalize(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap) {
        if (!m_renderGraph->compile()) { LOG_ERROR("RenderGraph compile failed"); return false; }

        for (auto& [tag, target] : m_tagToSubpass) {
            auto passIt = m_tagToPassNode.find(tag);
            if (passIt != m_tagToPassNode.end())
                target.renderPass = passIt->second->getRenderPassHandle();
        }

        if (m_tagToSubpass.count("Presentation")) {
            preparePresentationExecutor();
        }

        onAfterCompile();   // 子类扩展（如粒子管线）
        onAfterCompileImGui();  // ImGui 初始化（需要 renderPass handle）

        m_textureIdMap = std::move(texIdMap);
        return true;
    }

    // 呈现 executor 自建管线：把编译好的 render pass + 全局描述符数据交给 PresentationExecutor::onPrepare
    void BaseRenderPath::preparePresentationExecutor() {
        auto it = m_tagToSubpass.find("Presentation");
        if (it == m_tagToSubpass.end() || !it->second.executor) return;

        auto* rec = dynamic_cast<PresentationExecutor*>(it->second.executor.get());
        if (!rec) return;

        IPassExecutor::ExecutorPrepareContext ctx;
        ctx.resMgr = m_resMgr;
        ctx.rhi = m_rhi;
        ctx.renderGraph = m_renderGraph.get();
        ctx.globalSetLayout = m_globalSetLayout;
        ctx.globalDescSets = m_globalDescSets;
        ctx.renderPass = it->second.renderPass;
        ctx.subpassIndex = it->second.subpassIndex;
        rec->onPrepare(ctx);
    }

} // namespace StarryEngine
