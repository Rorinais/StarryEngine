#include <renderer/renderPaths/BaseRenderPath.hpp>
#include <renderer/renderPaths/LayerRegistry.hpp>
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
        // <=1 是窗口最小化时的退化尺寸（X11 报 1x1），重建毫无意义且代价极高
        if (width <= 1 || height <= 1) return;

        // 尺寸没变就不要重建。窗口管理器在拖拽过程中会反复派发同一个尺寸，
        // 每次都走全量重建会让帧循环彻底跟不上（实测密集拖拽时 60 秒内重建 448 次）。
        if (m_renderGraph && width == m_width && height == m_height) return;

        uint32_t oldW = m_width, oldH = m_height;
        m_width = width; m_height = height;
        for (auto& [name, desc] : m_textureDescs) {
            //只更新跟随窗口变化的纹理，阴影纹理大小不跟新
            if (desc.extent.width == oldW && desc.extent.height == oldH) {
                desc.extent.width = width; desc.extent.height = height;
            }
        }

        // 标记：这次重建由窗口尺寸变化引起。管线与窗口尺寸无关（动态渲染下缓存键里的
        // renderPass 始终无效、格式也不变），因此不应在 resize 时丢弃管线缓存 ——
        // 否则每次拖拽都要重新编译几十条管线，既卡顿又会让交换链被拖入卡死状态。
        const bool prevFlag = m_resizeRebuild;
        m_resizeRebuild = true;

        // ── 首选：增量 resize ──
        // 只重分配跟随视口尺寸的纹理/framebuffer，并更新 pass 渲染区域；pass / 管线 /
        // render pass / 描述符布局 / 拓扑全部保留。任何不变量不满足则安全回落到下面的全量重建。
        // 是否走增量路径由成员开关决定（默认启用）；增量路径内部任何不变量不满足时
        // 会自动回落到下面的全量重建。需要走旧路径时调用 setIncrementalResize(false)。
        if (m_renderGraph && m_incrementalResize) {
            refreshSwapchainImport();                       // 交换链重建后其 image view 已变
            if (m_renderGraph->resizeViewport(width, height)) {
                // 材质等绑定的是图的物理纹理句柄，句柄换了要重新绑定一次
                if (m_cachedSceneData) {
                    try { doRebuildResources(*m_cachedSceneData); }
                    catch (const std::exception& e) { LOG_WARN("resize 后重新绑定资源失败: {}", e.what()); }
                }
                // ImGui 后端缓存了 SceneColor 的视图/描述符（m_sceneTextureID）。蓝图重分配后
                // 旧视图已销毁，不重新注册就会引用悬空视图 → 设备丢失。
                try { onAfterCompileImGui(); }
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
        // 图像数量也必须同步：交换链重建后可能是 2 张也可能是 3 张，
        // 若图里仍记着旧数量，收集附件视图时会漏视图 → 零颜色附件 → 设备丢失。
        m_renderGraph->setSwapchainImageCount(m_rhi->getSwapChainImageCount());
        m_renderGraph->updateImportedTexture(m_swapchainTextureName, views, it->second);
    }

    void BaseRenderPath::render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) {
        if (!m_renderGraph) return;
        auto context = buildRenderContext();
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
            onBeforeGraphDestroy();
            // 管线缓存的失效策略必须区分两条渲染路径：
            //  · 动态渲染（VK_KHR_dynamic_rendering）：管线只绑定附件格式，与渲染图对象无关，
            //    缓存键里的 renderPass 恒为无效句柄 —— resize 引起的重建可以安全复用管线，
            //    于是重建变成一次查表（否则每次拖拽都要重编译几十条管线 → 卡顿并拖死交换链）。
            //  · 旧版 render pass 路径：管线绑定在 render pass 对象上，重建渲染图会生成新对象
            //    （句柄可能被回收复用），复用缓存会拿到指向已销毁对象的管线，因此必须照旧失效。
            // 着色器热重载等真正换管线的场景由 RasterRenderer::reloadShader 里的失效逻辑负责。
            const bool dynamicRendering = m_renderGraph && m_renderGraph->usesDynamicRendering();
            if (!m_resizeRebuild || !dynamicRendering) {
                Assets::PipelineCache::invalidateAll(m_resMgr.get());
            }
            m_renderGraph.reset();
        }
        m_renderGraph = std::make_shared<RenderGraph::RenderGraph>(m_rhi);
        m_renderGraph->setSwapchainImageCount(m_rhi->getSwapChainImageCount());
        m_renderGraph->setViewportSize(m_width, m_height);   // 供 resizeViewport 判断哪些纹理跟随视口

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

        if (!hasSceneColorClearPass()) {
            auto clearPass = std::make_shared<GraphicsPass>("AutoClearPass");
            RenderGraph::AttachmentParams bg;
            bg.loadOp = RHI::AttachmentLoadOp::Clear;
            bg.storeOp = RHI::AttachmentStoreOp::Store;
            bg.initialLayout = RHI::ImageLayout::ShaderReadOnly;
            bg.finalLayout = RHI::ImageLayout::ShaderReadOnly;
            bg.clearColor = m_presentClearColor;
            SubpassDesc sp;
            sp.tag = "AutoClear";
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
        auto sceneColorTexId = texIdMap.at(m_presentInputTexture);
        std::string inputKey = passNode->addInput(sceneColorTexId, inputParams);
        subpassBuilder.addInputAttachmentRef(inputKey);

        auto rec = std::make_shared<PresentationExecutor>();
        rec->setInputTextureName(m_presentInputTexture);
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

    void BaseRenderPath::setDefaultSlot(int layer, std::shared_ptr<IPass> pass, const std::string& tag) {
        PassSlot slot;
        slot.layer = layer;
        slot.tag = tag;
        slot.pass = std::move(pass);
        slot.enabled = true;
        auto it = std::find_if(m_slots.begin(), m_slots.end(),
            [&slot](const PassSlot& s) { return s.layer > slot.layer; });
        m_slots.insert(it, std::move(slot));
    }

    void BaseRenderPath::setDefaultSlot(const std::string& layerName, std::shared_ptr<IPass> pass, const std::string& tag) {
        int layer = RenderLayers::instance().find(layerName);
        if (layer < 0) {
            LOG_WARN("[layer] 未定义的渲染层 '{}'，退回 Opaque", layerName);
            layer = RenderLayers::Opaque;
        }
        setDefaultSlot(layer, std::move(pass), tag);
    }

    void BaseRenderPath::setDefaultSlot(RenderPassEvent ev, std::shared_ptr<IPass> pass, const std::string& tag) {
        setDefaultSlot(legacyLayerOf(ev), std::move(pass), tag);
    }

    void BaseRenderPath::insertPass(int layer, std::shared_ptr<IPass> pass, const std::string& tag) {
        setDefaultSlot(layer, std::move(pass), tag);
    }

    void BaseRenderPath::insertPass(const std::string& layerName, std::shared_ptr<IPass> pass, const std::string& tag) {
        setDefaultSlot(layerName, std::move(pass), tag);
    }

    void BaseRenderPath::insertPass(RenderPassEvent ev, std::shared_ptr<IPass> pass, const std::string& tag) {
        setDefaultSlot(ev, std::move(pass), tag);
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
