#include "DeferredRenderPath.hpp"
#include "../../logging/Logger.hpp"
#include "../../ui/ImGuiManager.hpp"

namespace StarryEngine {

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

        if (m_imguiManager) {
            m_imguiManager->setRenderGraph(nullptr);
        }
        m_renderGraph.reset();

        m_renderGraph = std::make_shared<RenderGraph::RenderGraph>(m_rhi);
        m_renderGraph->setSwapchainImageCount(m_rhi->getSwapChainImageCount());

        auto texIdMap = buildTextureIdMap();
        buildConfigPasses(texIdMap);
        buildOverlayPasses(texIdMap);
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
            auto* passNode = m_renderGraph->addPassNode(passDesc.name);
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

    void DeferredRenderPath::buildOverlayPasses(
        std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap)
    {
        for (const auto& overlay : m_overlayPasses) {
            auto* passNode = m_renderGraph->addPassNode(overlay.tag + "Pass");
            passNode->setRenderArea(m_width, m_height);

            auto& subpassBuilder = passNode->addSubpass(overlay.tag);
            subpassBuilder.setTag(overlay.tag);

            // ---- Swapchain 颜色附件 ----
            RenderGraph::AttachmentParams scParams;
            scParams.loadOp        = RHI::AttachmentLoadOp::Clear;       // 清除背景
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

            // 注意：这里的 subpassIndex 始终是 0，因为该 Pass 只有一个子通道
            m_tagToSubpass[overlay.tag] = SubpassTarget{ {}, 0, overlay.recorder };
            m_tagToPassNode[overlay.tag] = passNode;
        }
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
                    m_imguiManager->initializeVulkanBackend( m_rhi.get(), m_resMgr.get(),it->second.renderPass, m_imguiImageCount);
                }
            }
            m_imguiManager->setDefaultSampler(m_defaultSampler);
            m_imguiManager->registerSceneTexture(m_resMgr.get());
        }

        m_textureIdMap = std::move(texIdMap);
        return true;
    }

    void DeferredRenderPath::rebuildResources(const Scene::AnalysisSceneResult& sceneData) {
        m_cachedSceneData = std::make_shared<Scene::AnalysisSceneResult>(sceneData);

        // ① 刷新材质纹理依赖（从 RenderGraph 解析到 DescriptorSet）
        updateMaterialTextures(sceneData);

        // ② 分发 DrawItems 到各个 Recorder
        distributeDrawItems(sceneData);

        // ③ 为所有 DrawItem 预创建管线映射
        prepareAllPipelines(sceneData);

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