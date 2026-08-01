#include "BaseRenderPath.hpp"
#include "../passExecutor/PresentationExecutor.hpp"
#include "../../logging/Logger.hpp"
#include "../../assets/loader/ShaderLoader.hpp"
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
        RHI::DescriptorSetHandle globalDescSet) {
        m_globalSetLayout = globalSetLayout;
        m_globalDescSet = globalDescSet;
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
        if (width == m_width && height == m_height) return;  // 尺寸未变，跳过
        if (width == 0 || height == 0) return;
        m_width = width; m_height = height;
        for (auto& [name, desc] : m_textureDescs) {
            desc.extent.width = width; desc.extent.height = height;
        }
        if (!initialize()) { LOG_ERROR("Failed to rebuild on resize"); return; }
    }

    void BaseRenderPath::render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) {
        if (!m_renderGraph) return;
        auto context = buildRenderContext();
        m_renderGraph->execute(encoder, context, frameIndex);
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
        m_presentationPipelineReady = false;

        // 销毁旧的 descriptor pool（presentation pipeline 每帧重建）
        if (m_presentSceneColorPool.isValid()) {
            m_resMgr->destroy(m_presentSceneColorPool);
            m_presentSceneColorPool = RHI::DescriptorPoolHandle{};
            m_presentSceneColorDescSet = RHI::DescriptorSetHandle{};
        }

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
        scParams.clearColor = { 0.08f, 0.08f, 0.10f, 1.0f };
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

        auto rec = std::make_shared<PresentationExecutor>(
            RHI::PipelineHandle{}, RHI::PipelineLayoutHandle{},
            m_globalDescSet, RHI::DescriptorSetHandle{});
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

        if (m_tagToSubpass.count("Presentation") && m_globalSetLayout.isValid()) {
            ensurePresentationShaders();
            preparePresentationPipeline(texIdMap);
        }

        onAfterCompile();   // 子类扩展（如粒子管线）
        onAfterCompileImGui();  // ImGui 初始化（需要 renderPass handle）

        m_textureIdMap = std::move(texIdMap);
        return true;
    }

    void BaseRenderPath::ensurePresentationShaders() {
        if (m_presentationShadersReady) return;
        if (!m_globalSetLayout.isValid()) return;

        Assets::ShaderLoader loader(m_resMgr);
        auto vsInfo = loader.loadFromFile("assets/shaders/deferred/fullscreen.vert", RHI::ShaderStage::Vertex);
        auto fsInfo = loader.loadFromFile("assets/shaders/deferred/copy.frag", RHI::ShaderStage::Fragment);
        if (!vsInfo || !fsInfo) { LOG_ERROR("Presentation shaders failed"); return; }
        m_fullscreenVert = vsInfo->module;
        m_copyFrag = fsInfo->module;

        RHI::DescriptorSetLayoutDesc set1Layout;
        set1Layout.bindings = {{0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment}};
        m_presentSceneColorLayout = m_resMgr->createDescriptorSetLayout(set1Layout);

        RHI::PipelineLayoutDesc playoutDesc;
        playoutDesc.descriptorSetLayouts = { m_globalSetLayout, m_presentSceneColorLayout };
        m_presentPipelineLayout = m_resMgr->createPipelineLayout(playoutDesc);

        m_presentationShadersReady = true;
        LOG_INFO("PresentationPass shaders loaded");
    }

    void BaseRenderPath::preparePresentationPipeline(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap) {
        if (!m_presentationShadersReady) ensurePresentationShaders();
        if (!m_presentationShadersReady || m_presentationPipelineReady) return;

        auto tagIt = m_tagToSubpass.find("Presentation");
        if (tagIt == m_tagToSubpass.end()) return;
        RHI::RenderPassHandle rp = tagIt->second.renderPass;
        if (!rp.isValid()) return;

        GraphicsPipelineState pso;
        pso.vertexShader = m_fullscreenVert; pso.fragmentShader = m_copyFrag;
        pso.layout = m_presentPipelineLayout;
        pso.cullMode = RHI::CullMode::None;
        pso.depthTestEnable = false; pso.depthWriteEnable = false;
        pso.topology = RHI::PrimitiveTopology::TriangleList;
        pso.dynamicStates = { RHI::DynamicState::Viewport, RHI::DynamicState::Scissor };
        pso.vertexInput = {};
        RHI::BlendAttachmentState blend; blend.blendEnable = false;
        pso.attachments = { blend };

        auto pipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
            m_resMgr.get(), pso, rp, tagIt->second.subpassIndex);
        if (!pipeline.isValid()) { LOG_ERROR("Presentation pipeline failed"); return; }

        auto sceneColorIt = texIdMap.find("SceneColor");
        if (sceneColorIt != texIdMap.end()) {
            RHI::TextureHandle physHandle = m_renderGraph->getPhysicalTextureHandle(sceneColorIt->second);
            if (physHandle.isValid()) {
                auto* texObj = m_resMgr->getTexture(physHandle);
                auto* samplerObj = m_resMgr->getSampler(m_defaultSampler);
                if (texObj && samplerObj) {
                    RHI::DescriptorPoolDesc poolDesc;
                    poolDesc.maxSets = 1;
                    poolDesc.poolSizes = {{RHI::DescriptorType::CombinedImageSampler, 1}};
                    m_presentSceneColorPool = m_resMgr->createDescriptorPool(poolDesc);

                    RHI::DescriptorSetDesc setDesc;
                    setDesc.descriptorSetLayout = m_presentSceneColorLayout;
                    setDesc.descriptorPool = m_presentSceneColorPool;
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

        auto recIt = m_tagToSubpass.find("Presentation");
        if (recIt != m_tagToSubpass.end() && recIt->second.executor) {
            auto* rec = dynamic_cast<PresentationExecutor*>(recIt->second.executor.get());
            if (rec) {
                rec->setPipeline(pipeline);
                rec->setLayout(m_presentPipelineLayout);
                rec->setSceneColorSet(m_presentSceneColorDescSet);
            }
        }
        m_presentationPipelineReady = true;
        LOG_INFO("PresentationPass pipeline ready");
    }

} // namespace StarryEngine
