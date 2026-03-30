#include "DeferredRenderPath.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine {

    DeferredRenderPath::DeferredRenderPath(std::shared_ptr<RHI::IRHI> rhi,
        uint32_t width, uint32_t height)
        : m_rhi(rhi), m_resMgr(rhi->getResourceManager()),
        m_width(width), m_height(height) {
    }

    void DeferredRenderPath::setConfig(const RenderPathConfig& config) {
        m_config = config;
    }

    void DeferredRenderPath::setTextureDescs(const std::unordered_map<std::string, RHI::TextureDesc>& descs) {
        m_textureDescs = descs;
    }

    void DeferredRenderPath::addTextureDesc(std::string name, RHI::TextureDesc desc) {
        m_textureDescs[name] = desc;
    }

    void DeferredRenderPath::addSubpass(Scene::RenderStage stage, Scene::RenderQueue Queue, Subpass subpass) {
        m_config[stage][Queue] = subpass.getConfig();
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

        m_stagePassNode.clear();
        m_subpassRecorders.clear();
        m_stagePassInfo.clear();

        m_renderGraph = std::make_shared<RenderGraph::RenderGraph>(m_rhi);
        m_renderGraph->setSwapchainImageCount(m_rhi->getSwapChainImageCount());

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

        for (auto& [stage, queueMap] : m_config) {
            std::string passName = "Pass_" + std::to_string(static_cast<int>(stage));
            auto* passNode = m_renderGraph->addPassNode(passName);
            passNode->setRenderArea(m_width, m_height);
            m_stagePassNode[stage] = passNode;

            StagePassInfo& passInfo = m_stagePassInfo[stage];
            uint32_t subpassIdx = 0;

            for (auto& [queue, subpassCfg] : queueMap) {
                passInfo.queueToSubpass[queue] = subpassIdx;

                std::vector<std::string> colorKeys, inputKeys, resolveKeys, preserveKeys;
                std::string depthKey;

                // 颜色附件
                for (auto& att : subpassCfg.colorAttachments) {
                    auto it = texIdMap.find(att.textureName);
                    if (it == texIdMap.end()) {
                        throw std::runtime_error("Texture not found: " + att.textureName);
                    }
                    std::string key = passNode->addColorOutput(it->second, att.params);
                    // 示例：在添加颜色附件后
                    LOG_INFO("Stage {} Subpass {} Color Attachment: name={}, initialLayout={}, finalLayout={}",
                        static_cast<int>(stage), subpassIdx, att.textureName,
                        static_cast<int>(att.params.initialLayout.value_or(RHI::ImageLayout::Undefined)),
                        static_cast<int>(att.params.finalLayout.value_or(RHI::ImageLayout::Undefined)));
                    colorKeys.push_back(key);
                }

                // 深度附件
                if (subpassCfg.depthAttachment) {
                    auto it = texIdMap.find(subpassCfg.depthAttachment->textureName);
                    if (it == texIdMap.end()) {
                        throw std::runtime_error("Texture not found: " + subpassCfg.depthAttachment->textureName);
                    }
                    depthKey = passNode->addDepthOutput(it->second, subpassCfg.depthAttachment->params);
                }

                // 输入附件
                for (auto& att : subpassCfg.inputAttachments) {
                    auto it = texIdMap.find(att.textureName);
                    if (it == texIdMap.end()) {
                        throw std::runtime_error("Texture not found: " + att.textureName);
                    }
                    std::string key = passNode->addInput(it->second, att.params);
                    // 示例：在添加颜色附件后
                    LOG_INFO("Stage {} Subpass {} Color Attachment: name={}, initialLayout={}, finalLayout={}",
                        static_cast<int>(stage), subpassIdx, att.textureName,
                        static_cast<int>(att.params.initialLayout.value_or(RHI::ImageLayout::Undefined)),
                        static_cast<int>(att.params.finalLayout.value_or(RHI::ImageLayout::Undefined)));
                    inputKeys.push_back(key);
                }

                // 解析附件
                for (auto& att : subpassCfg.resolveAttachments) {
                    auto it = texIdMap.find(att.textureName);
                    if (it == texIdMap.end()) {
                        throw std::runtime_error("Texture not found: " + att.textureName);
                    }
                    std::string key = passNode->addResolve(it->second, att.params);
                    resolveKeys.push_back(key);
                }

                // 保留附件
                for (auto& texName : subpassCfg.preserveAttachments) {
                    auto it = texIdMap.find(texName);
                    if (it == texIdMap.end()) {
                        throw std::runtime_error("Texture not found: " + texName);
                    }
                    std::string key = passNode->addPreserve(it->second);
                    preserveKeys.push_back(key);
                }

                auto& subpassBuilder = passNode->addSubpass(subpassCfg.name);
                for (const auto& key : colorKeys) subpassBuilder.addColorAttachmentRef(key);
                if (!depthKey.empty()) subpassBuilder.addDepthStencilAttachmentRef(depthKey);
                for (const auto& key : inputKeys) subpassBuilder.addInputAttachmentRef(key);
                for (const auto& key : resolveKeys) subpassBuilder.addResolveAttachmentRef(key);
                for (const auto& key : preserveKeys) subpassBuilder.addPreserveAttachmentRef(key);
                subpassBuilder.setRecorder(subpassCfg.recorder);

                uint64_t recorderKey = (static_cast<uint64_t>(stage) << 32) | subpassIdx;
                m_subpassRecorders[recorderKey] = subpassCfg.recorder;

                ++subpassIdx;
            }
        }

        if (!m_renderGraph->compile()) {
            LOG_ERROR("Failed to compile RenderGraph");
            return false;
        }

        m_textureIdMap = std::move(texIdMap);

        // 为所有 stage 设置 RenderPassHandle
        for (auto& [stage, passNode] : m_stagePassNode) {
            if (passNode) {
                m_stagePassInfo[stage].renderPassHandle = passNode->getRenderPassHandle();
            }
        }

        return true;
    }

    void DeferredRenderPath::setDrawItems(const Scene::AnalysisSceneResult& sceneData) {
        m_cachedSceneData = std::make_shared<Scene::AnalysisSceneResult>(sceneData);
        updateMaterialTextures(sceneData);  
        distributeDrawItems(sceneData);
    }

    void DeferredRenderPath::distributeDrawItems(const Scene::AnalysisSceneResult& sceneData) {
        for (auto& [key, recorder] : m_subpassRecorders) {
            recorder->clearDrawItems();
        }

        // 临时分组：键 = (stage << 32) | subpass
        std::unordered_map<uint64_t, std::vector<std::shared_ptr<Scene::DrawItem>>> groups;

        size_t unmatched = 0;
        for (auto& item : sceneData.drawItems) {
            auto stageIt = m_stagePassInfo.find(item->stage);
            if (stageIt == m_stagePassInfo.end()) {
                ++unmatched; 
                continue;
            }
            const auto& passInfo = stageIt->second;

            auto queueIt = passInfo.queueToSubpass.find(item->queue);
            if (queueIt == passInfo.queueToSubpass.end()) {
                ++unmatched; 
                continue;
            }
            uint32_t subpass = queueIt->second;

            uint64_t key = (static_cast<uint64_t>(item->stage) << 32) | subpass;
            groups[key].push_back(item);
        }

        if (unmatched > 0) {
            LOG_WARN("distributeDrawItems: {} draw items had no matching stage/subpass", unmatched); 
        }

        for (auto& [key, items] : groups) {
            auto it = m_subpassRecorders.find(key);
            if (it != m_subpassRecorders.end()) {
                it->second->setDrawItems(items);
            }
            else {
                LOG_WARN("No recorder found for key {}", key); 
            }
        }
    }

    void DeferredRenderPath::update(const glm::mat4& view, const glm::mat4& proj, float deltaTime) {
        m_lastView = view;
        m_lastProj = proj;
        m_lastDeltaTime = deltaTime;

        if (!m_cachedSceneData) {
            LOG_WARN("update: no cached scene data, skipping");
            return;
        }

        for (auto& [stage, passInfo] : m_stagePassInfo) {
            for (auto& [queue, subpass] : passInfo.queueToSubpass) {
                uint64_t key = (static_cast<uint64_t>(stage) << 32) | subpass;
                auto recorderIt = m_subpassRecorders.find(key);
                if (recorderIt == m_subpassRecorders.end()) continue;
                auto& recorder = recorderIt->second;
                auto& items = recorder->getDrawItems();
                if (items.empty()) continue;

                // 收集该子通道中所有用到的 pipelineIndex（去重）
                std::unordered_set<uint32_t> usedPipelineIndices;
                for (auto& item : items) {
                    if (item->pipelineIndex < m_cachedSceneData->PSO.size()) {
                        usedPipelineIndices.insert(item->pipelineIndex);
                    }
                    else {
                        LOG_WARN("item pipelineIndex {} out of range", item->pipelineIndex);
                    }
                }

                // 为每个 pipelineIndex 创建管线句柄
                std::unordered_map<uint32_t, RHI::PipelineHandle> pipelineMapping;
                for (uint32_t idx : usedPipelineIndices) {
                    auto& pso = m_cachedSceneData->PSO[idx];
                    auto pipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
                        m_resMgr.get(), *pso, passInfo.renderPassHandle, subpass);
                    if (pipeline.isValid()) {
                        pipelineMapping[idx] = pipeline;
                    }
                    else {
                        LOG_ERROR("Failed to create pipeline for PSO index {}", idx);
                    }
                }

                // 将映射表传递给录制器
                recorder->setPipelineMapping(pipelineMapping);
            }
        }
    }

    void DeferredRenderPath::render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) {
        if (!m_renderGraph) return;

        auto context = buildRenderContext();
        m_renderGraph->execute(encoder, context, frameIndex);
    }

    void DeferredRenderPath::onResize(uint32_t width, uint32_t height) {
        m_width = width;
        m_height = height;

        // 更新纹理描述尺寸
        for (auto& [name, desc] : m_textureDescs) {
            desc.extent.width = width;
            desc.extent.height = height;
        }

        // 重新初始化渲染图
        if (!initialize()) {
            LOG_ERROR("Failed to rebuild render path on resize");
            return;
        }

        if (m_cachedSceneData) {
            setDrawItems(*m_cachedSceneData);
        }
    }

    void DeferredRenderPath::updateMaterialTextures(const Scene::AnalysisSceneResult& sceneData) {
        if (!m_renderGraph || m_textureIdMap.empty()) {
            LOG_WARN("RenderGraph not ready for texture updates");
            return;
        }

        // 默认采样器（可缓存）
        RHI::SamplerDesc defaultSamplerDesc;
        defaultSamplerDesc.minFilter = RHI::SamplerFilter::Linear;
        defaultSamplerDesc.magFilter = RHI::SamplerFilter::Linear;
        auto defaultSampler = m_resMgr->createSampler(defaultSamplerDesc);

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
                    material->setTexture(dep.set, dep.binding, phys, defaultSampler);
                    break;
                case Assets::ResourceDependencyType::InputAttachment:
                    LOG_INFO("Material setInputAttachment set={}, binding={}, texture='{}'", dep.set, dep.binding, texName);
                    material->setInputAttachment(dep.set, dep.binding, phys, RHI::ImageLayout::ShaderReadOnly);
                    break;
                }
            }
        }
    }

} // namespace StarryEngine