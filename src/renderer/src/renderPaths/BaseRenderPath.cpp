#include <renderer/renderPaths/BaseRenderPath.hpp>
#include <renderer/renderPaths/LayerRegistry.hpp>
#include <renderer/renderPaths/TextureRegistry.hpp>
#include <renderer/passExecutor/PresentationExecutor.hpp>
#include <renderer/passes/GraphicsPass.hpp>
#include <renderer/passes/ComputePass.hpp>
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
            [&tag](const OverlayPassDesc& desc) { return subpassName(desc.layer, desc.queue) == tag; });
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
        if (width <= 1 || height <= 1) return;

        if (m_renderGraph && width == m_width && height == m_height) return;

        uint32_t oldW = m_width, oldH = m_height;
        m_width = width; m_height = height;
        for (auto& [name, desc] : m_textureDescs) {
            if (desc.extent.width == oldW && desc.extent.height == oldH) {
                desc.extent.width = width; desc.extent.height = height;
            }
        }

        const bool prevFlag = m_resizeRebuild;
        m_resizeRebuild = true;

        if (m_renderGraph && m_incrementalResize) {
            refreshSwapchainImport();                       // 交换链重建后其 image view 已变
            if (m_renderGraph->resizeViewport(width, height)) {
                if (m_cachedSceneData) {
                    try { doRebuildResources(*m_cachedSceneData); } //子类实现
                    catch (const std::exception& e) { LOG_WARN("resize 后重新绑定资源失败: {}", e.what()); }
                }

                try { onAfterCompileImGui(); } //子类实现
                catch (const std::exception& e) { LOG_WARN("resize 后刷新 ImGui 场景纹理失败: {}", e.what()); }
                m_resizeRebuild = prevFlag;
                return;
            }
            LOG_WARN("增量 resize 不可用，回落全量重建渲染图");
        }

        // ── 兜底：全量重建（与原来行为一致）──
        bool ok = initialize();
        m_resizeRebuild = prevFlag;
        if (!ok) { LOG_ERROR("Failed to rebuild on resize"); return; }
    }

    void BaseRenderPath::refreshSwapchainImport() {
        if (!m_renderGraph) return;
        auto it = m_textureDescs.find(m_swapchainTextureName);
        if (it == m_textureDescs.end()) return;

        std::vector<void*> views;
        views.reserve(m_rhi->getSwapChainImageCount());
        for (uint32_t i = 0; i < m_rhi->getSwapChainImageCount(); ++i) {
            views.push_back(m_rhi->getSwapChainImageView(i));
        }

        m_renderGraph->setSwapchainImageCount(m_rhi->getSwapChainImageCount());
        m_renderGraph->updateImportedTexture(m_swapchainTextureName, views, it->second);
    }

    void BaseRenderPath::render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) {
        if (!m_renderGraph) return;

        if (!m_swapchainTextureName.empty()) {
            const uint32_t cnt = m_rhi->getSwapChainImageCount();
            void* v0 = (cnt > 0) ? m_rhi->getSwapChainImageView(0) : nullptr;
            if (cnt != m_lastSwapchainCount || v0 != m_lastSwapchainView0) {
                LOG_INFO("[path] 交换链视图变化（count {}→{}，view0 变化），刷新附件视图表",
                         m_lastSwapchainCount, cnt);
                m_lastSwapchainCount = cnt;
                m_lastSwapchainView0 = v0;
                refreshSwapchainImport();
            }
        }

        auto context = buildRenderContext();
        context.frameSlot = m_rhi->getCurrentFrameIndex();
        if (context.frameSlot >= RHI::kMaxFramesInFlight) context.frameSlot = 0;
        m_renderGraph->execute(encoder, context, frameIndex, context.frameSlot, m_parallel);
    }

    void BaseRenderPath::rebuildResources(const AnalysisSceneResult& sceneData) {
        doRebuildResources(sceneData);//子类实现
        m_cachedSceneData = std::make_shared<AnalysisSceneResult>(sceneData);
        if (!m_resourceStatsPrinted) {
            m_rhi->printResourceStatistics();
            m_resourceStatsPrinted = true;
        }
    }

    // ──── Build Graph ──────────────────────────────────────────────

    bool BaseRenderPath::buildGraph() {
        m_keyToSubpass.clear();
        m_keyToPassNode.clear();

        if (m_renderGraph) {
            m_rhi->waitIdle();
            onBeforeGraphDestroy();//子类实现
            const bool dynamicRendering = m_renderGraph && m_renderGraph->usesDynamicRendering();
            if (!m_resizeRebuild || !dynamicRendering) {
                Assets::PipelineCache::invalidateAll(m_resMgr.get());
            }
            m_renderGraph.reset();
        }
        m_renderGraph = std::make_shared<RenderGraph::RenderGraph>(m_rhi);
        m_renderGraph->setSwapchainImageCount(m_rhi->getSwapChainImageCount());
        m_renderGraph->setViewportSize(m_width, m_height);   

        auto texIdMap = buildTextureIdMap();

        buildConfigPasses(texIdMap);         // 子类实现
        buildPresentationPasses(texIdMap);    // 基类实现
        return compileAndFinalize(texIdMap);
    }

    // 是否有 GraphicsPass 通过颜色附件清除 SceneColor（loadOp 缺省按 Clear 处理）
    bool BaseRenderPath::hasSceneColorClearPass() const {
        for (const auto& pass : m_passes) {
            if (auto* gp = dynamic_cast<const GraphicsPass*>(pass.get())) {
                for (const auto& sp : gp->getSubpassDescs()) {
                    for (const auto& att : sp.colorAttachments) {
                        if (att.textureName == "SceneColor" &&
                            att.params.loadOp.value_or(RHI::AttachmentLoadOp::Clear) == RHI::AttachmentLoadOp::Clear) {
                            return true;
                        }
                    }
                }
            }
            // compute pass 写 SceneColor（如软光线追踪输出）也视为已清屏
            if (auto* cp = dynamic_cast<const ComputePass*>(pass.get())) {
                for (const auto& r : cp->getDesc().resources) {
                    if (r.type == RHI::DescriptorType::StorageImage &&
                        r.write && r.resourceName == "SceneColor") {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    std::unordered_map<std::string, RenderGraph::TextureId> BaseRenderPath::buildTextureIdMap() {
        std::unordered_map<std::string, RenderGraph::TextureId> texIdMap;
        // 合并外部注册表（RenderTextures）与路径自身描述：本地优先
        std::unordered_map<std::string, RHI::TextureDesc> allDescs = RenderTextures::instance().all();
        for (auto& [n, d] : m_textureDescs) allDescs[n] = d;
        for (const auto& [name, desc] : allDescs) {
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

        if (!hasSceneColorClearPass()) {
            auto clearPass = std::make_shared<GraphicsPass>("AutoClearPass");
            RenderGraph::AttachmentParams bg;
            bg.loadOp = RHI::AttachmentLoadOp::Clear;
            bg.storeOp = RHI::AttachmentStoreOp::Store;
            bg.initialLayout = RHI::ImageLayout::ShaderReadOnly;
            bg.finalLayout = RHI::ImageLayout::ShaderReadOnly;
            bg.clearColor = m_presentClearColor;
            SubpassDesc sp;
            sp.layer = RenderLayers::Background; sp.queue = 99;
            sp.colorAttachments.push_back({ "SceneColor", bg });
            clearPass->addSubpass(sp);
            m_passes.insert(m_passes.begin(), clearPass);
            LOG_INFO("[BaseRenderPath] 无场景 pass 清除 SceneColor → 自动插入清屏 pass（颜色={},{},{}）",
                m_presentClearColor.r, m_presentClearColor.g, m_presentClearColor.b);
        }
        return texIdMap;
    }

    // ──── 呈现层 ────────────────────────────────────────────────────

    void BaseRenderPath::buildPresentationPasses(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap) {
        if (!m_overlayPasses.empty()) {
            for (const auto& overlay : m_overlayPasses) {
                auto* passNode = m_renderGraph->addGraphicsPassNode(subpassName(overlay.layer, overlay.queue) + "Pass");
                passNode->setRenderArea(m_width, m_height);
                auto& subpassBuilder = passNode->addSubpass(subpassName(overlay.layer, overlay.queue));
                subpassBuilder.setLayer(overlay.layer);
                subpassBuilder.setQueue(overlay.queue);

                // 颜色输出（每个 overlay 自己声明）
                for (auto& [texName, params] : overlay.colorOutputs) {
                    auto texIt = texIdMap.find(texName);
                    if (texIt == texIdMap.end()) { LOG_ERROR("[path] overlay 颜色输出纹理 '{}' 未声明", texName); continue; }
                    std::string key = passNode->addColorOutput(texIt->second, params);
                    subpassBuilder.addColorAttachmentRef(key);
                }
                // 深度输出（可选）
                if (overlay.depthOutput) {
                    auto dIt = texIdMap.find(overlay.depthOutput->first);
                    if (dIt == texIdMap.end()) {
                        LOG_ERROR("[path] overlay 深度输出纹理 '{}' 未声明", overlay.depthOutput->first);
                    } else {
                        std::string depthKey = passNode->addDepthOutput(dIt->second, overlay.depthOutput->second);
                        subpassBuilder.addDepthStencilAttachmentRef(depthKey);
                    }
                }
                // 输入附件
                for (auto& [texName, params] : overlay.inputAttachments) {
                    auto inIt = texIdMap.find(texName);
                    if (inIt == texIdMap.end()) { LOG_ERROR("[path] overlay 输入纹理 '{}' 未声明", texName); continue; }
                    std::string key = passNode->addInput(inIt->second, params);
                    subpassBuilder.addInputAttachmentRef(key);
                }

                subpassBuilder.setExecutor(overlay.executor);
                const long long key = (static_cast<long long>(overlay.layer) << 32) | (overlay.queue & 0xFFFFFFFFLL);
                m_keyToSubpass[key] = SubpassTarget{ {}, 0, overlay.executor };
                m_keyToPassNode[key] = passNode;
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


        RenderGraph::AttachmentParams scParams;
        scParams.loadOp = RHI::AttachmentLoadOp::Clear;
        scParams.storeOp = RHI::AttachmentStoreOp::Store;
        scParams.initialLayout = RHI::ImageLayout::Undefined;
        scParams.finalLayout = RHI::ImageLayout::PresentSrc;
        scParams.clearColor = m_presentClearColor;
        auto scIt = texIdMap.find(m_swapchainTextureName);
        if (scIt == texIdMap.end()) { LOG_ERROR("[path] 交换链纹理 '{}' 未声明", m_swapchainTextureName); return; }
        auto swapchainTexId = scIt->second;
        std::string scKey = passNode->addColorOutput(swapchainTexId, scParams);
        subpassBuilder.addColorAttachmentRef(scKey);

        RenderGraph::AttachmentParams inputParams;
        inputParams.loadOp = RHI::AttachmentLoadOp::Load;
        inputParams.storeOp = RHI::AttachmentStoreOp::DontCare;
        inputParams.initialLayout = RHI::ImageLayout::ShaderReadOnly;
        inputParams.finalLayout = RHI::ImageLayout::ShaderReadOnly;
        auto pIt = texIdMap.find(m_presentInputTexture);
        if (pIt == texIdMap.end()) { LOG_ERROR("[path] 呈现输入纹理 '{}' 未声明", m_presentInputTexture); return; }
        auto sceneColorTexId = pIt->second;
        std::string inputKey = passNode->addInput(sceneColorTexId, inputParams);
        subpassBuilder.addInputAttachmentRef(inputKey);

        auto rec = std::make_shared<PresentationExecutor>();
        rec->setInputTextureName(m_presentInputTexture);
        subpassBuilder.setExecutor(rec);
        m_keyToSubpass[presentKey()] = SubpassTarget{ {}, 0, rec };
        m_keyToPassNode[presentKey()] = passNode;
        LOG_INFO("Built PresentationPass (no overlay)");
    }

    // ──── 编译 + 内建管道 ──────────────────────────────────────────

    bool BaseRenderPath::compileAndFinalize(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap) {
        if (!m_renderGraph->compile()) { LOG_ERROR("RenderGraph compile failed"); return false; }

        for (auto& [key, target] : m_keyToSubpass) {
            auto passIt = m_keyToPassNode.find(key);
            if (passIt != m_keyToPassNode.end())
                target.renderPass = passIt->second->getRenderPassHandle();
        }

        if (m_keyToSubpass.count(presentKey())) {
            preparePresentationExecutor();
        }

        onAfterCompile();   // 子类扩展（如粒子管线）
        onAfterCompileImGui();  //子类实现

        m_textureIdMap = std::move(texIdMap);
        return true;
    }

    // 呈现 executor 自建管线：把编译好的 render pass + 全局描述符数据交给 PresentationExecutor::onPrepare
    void BaseRenderPath::preparePresentationExecutor() {
        auto it = m_keyToSubpass.find(presentKey());
        if (it == m_keyToSubpass.end() || !it->second.executor) return;

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

    // ── 路径槽位（Unity URP ScriptableRenderer 模型）──

    namespace {
        /// 旧枚举槽位 → RenderLayers 系统层（Before/After 归到同名层）
        int legacyLayerOf(RenderPassEvent ev) {
            switch (ev) {
                case RenderPassEvent::BeforeShadow:
                case RenderPassEvent::Shadow:
                case RenderPassEvent::AfterShadow:     return RenderLayers::Background;
                case RenderPassEvent::BeforeOpaque:
                case RenderPassEvent::Opaque:
                case RenderPassEvent::AfterOpaque:     return RenderLayers::Opaque;
                case RenderPassEvent::BeforePost:
                case RenderPassEvent::Post:
                case RenderPassEvent::AfterPost:       return RenderLayers::Surface;
                case RenderPassEvent::BeforeStencil:
                case RenderPassEvent::Stencil:
                case RenderPassEvent::AfterStencil:    return RenderLayers::Outline;
                case RenderPassEvent::BeforeParticles:
                case RenderPassEvent::Particles:
                case RenderPassEvent::AfterParticles:  return RenderLayers::Particles;
                default:                               return RenderLayers::Background;
            }
        }
    }

    void BaseRenderPath::setDefaultSlot(int layer, std::shared_ptr<IPass> pass) {
        for (auto& s : m_slots) {
            if (s.layer != layer) continue;
            if (s.pass && s.pass != pass) {
                LOG_WARN("[path] 层 {} 已有其它 pass（'{}'），将被新 pass 替换",
                         layer, s.pass->getName());
            }
            s.pass = std::move(pass);
            s.enabled = true;
            return;
        }
        PassSlot slot;
        slot.layer = layer;
        slot.pass = std::move(pass);
        slot.enabled = true;
        auto it = std::find_if(m_slots.begin(), m_slots.end(),
            [&slot](const PassSlot& s) { return s.layer > slot.layer; });
        m_slots.insert(it, std::move(slot));
    }

    void BaseRenderPath::setDefaultSlot(const std::string& layerName, std::shared_ptr<IPass> pass) {
        int layer = RenderLayers::instance().find(layerName);
        if (layer < 0) {
            LOG_WARN("[layer] 未定义的渲染层 '{}'，退回 Opaque", layerName);
            layer = RenderLayers::Opaque;
        }
        setDefaultSlot(layer, std::move(pass));
    }

    void BaseRenderPath::setDefaultSlot(RenderPassEvent ev, std::shared_ptr<IPass> pass) {
        setDefaultSlot(legacyLayerOf(ev), std::move(pass));
    }

    void BaseRenderPath::insertPass(int layer, std::shared_ptr<IPass> pass) {
        setDefaultSlot(layer, std::move(pass));
    }

    void BaseRenderPath::insertPass(const std::string& layerName, std::shared_ptr<IPass> pass) {
        setDefaultSlot(layerName, std::move(pass));
    }

    void BaseRenderPath::insertPass(RenderPassEvent ev, std::shared_ptr<IPass> pass) {
        setDefaultSlot(ev, std::move(pass));
    }

    void BaseRenderPath::overrideSlot(const std::string& layerName, std::shared_ptr<IPass> pass) {
        const int layer = RenderLayers::instance().find(layerName);
        for (auto& slot : m_slots) {
            if (slot.layer == layer) {
                slot.pass = std::move(pass);
                slot.enabled = true;
                return;
            }
        }
        // 无默认槽 → 直接插入
        insertPass(layer, std::move(pass));
    }

    void BaseRenderPath::overrideSlot(RenderPassEvent ev, std::shared_ptr<IPass> pass) {
        const int layer = legacyLayerOf(ev);
        for (auto& slot : m_slots) {
            if (slot.layer == layer) {
                slot.pass = std::move(pass);
                slot.enabled = true;
                return;
            }
        }
        insertPass(ev, std::move(pass));
    }

    void BaseRenderPath::setSlotEnabled(const std::string& layerName, bool enabled) {
        const int layer = RenderLayers::instance().find(layerName);
        for (auto& slot : m_slots) {
            if (slot.layer == layer) slot.enabled = enabled;
        }
    }

    void BaseRenderPath::setSlotEnabled(RenderPassEvent ev, bool enabled) {
        const int layer = legacyLayerOf(ev);
        for (auto& slot : m_slots) {
            if (slot.layer == layer) slot.enabled = enabled;
        }
    }

    void BaseRenderPath::assemblePassList() {
        PassList passes;
        for (auto& slot : m_slots) {
            if (!slot.enabled || !slot.pass) continue;
            passes.push_back(slot.pass);
        }
        setPassList(std::move(passes));
    }

} // namespace StarryEngine
