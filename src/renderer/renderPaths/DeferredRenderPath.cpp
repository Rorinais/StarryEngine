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

        // 统一处理所有子通道的固定管线创建和纹理绑定
        for (auto& [stage, queueMap] : m_config) {
            uint32_t idx = 0;
            for (auto& [queue, subpassCfg] : queueMap) {
                ++idx;
            }

            for (auto& [queue, subpassCfg] : queueMap) {
                auto* passNode = m_stagePassNode[stage];
                uint32_t subpassIdx = m_stagePassInfo[stage].queueToSubpass[queue];
                RHI::RenderPassHandle rpHandle = passNode->getRenderPassHandle();

                if (subpassCfg.pipelineDesc) {
                    const auto& state = *subpassCfg.pipelineDesc;

                    RHI::GraphicsPipelineDesc gpDesc;
                    gpDesc.vertexShader = state.vertexShader;
                    gpDesc.fragmentShader = state.fragmentShader;
                    gpDesc.pipelineLayoutHandle = state.layout;
                    gpDesc.vertexInput = state.vertexInput;
                    gpDesc.topology = state.topology;
                    gpDesc.rasterizer.cullMode = state.cullMode;
                    gpDesc.rasterizer.frontFace = state.frontFace;
                    gpDesc.rasterizer.lineWidth = state.lineWidth;
                    gpDesc.depthStencil.depthTestEnable = state.depthTestEnable;
                    gpDesc.depthStencil.depthWriteEnable = state.depthWriteEnable;
                    gpDesc.depthStencil.depthCompareOp = state.depthCompareOp;
                    gpDesc.colorBlend.attachments = state.attachments;
                    gpDesc.viewport.viewports = state.viewports;
                    gpDesc.viewport.scissors = state.scissors;
                    gpDesc.dynamicStates = state.dynamicStates;
                    gpDesc.renderPass = rpHandle;
                    gpDesc.subpass = subpassIdx;
                    gpDesc.debugName = subpassCfg.name + "_Pipeline";

                    auto pipeline = m_resMgr->createGraphicsPipeline(gpDesc);
                    if (pipeline.isValid()) {
                        subpassCfg.recorder->setPipeline(pipeline);
                    }
                    else {
                        LOG_ERROR("Pipeline creation failed for subpass: {}", subpassCfg.name);
                    }
                }

                // 纹理绑定
                auto material = subpassCfg.recorder->getMaterial();
                if (material) {
                    for (const auto& binding : subpassCfg.textureBindings) {
                        auto texIt = texIdMap.find(binding.textureName);
                        if (texIt == texIdMap.end()) {
                            LOG_WARN("Texture '{}' not found for subpass '{}'", binding.textureName, subpassCfg.name);
                            continue;
                        }
                        auto phys = m_renderGraph->getPhysicalTextureHandle(texIt->second);
                        if (!phys.isValid()) {
                            LOG_WARN("Physical texture handle for '{}' is invalid", binding.textureName);
                            continue;
                        }
                        auto sampler = m_resMgr->createSampler(binding.samplerDesc);
                        material->setTexture(binding.set, binding.binding, phys, sampler);
                    }
                }
            }
        }

        // 为所有 stage 设置 RenderPassHandle（用于 update 中的 PSO 创建）
        for (auto& [stage, passNode] : m_stagePassNode) {
            if (passNode) {
                m_stagePassInfo[stage].renderPassHandle = passNode->getRenderPassHandle();
            }
        }

        return true;
    }

    void DeferredRenderPath::setDrawItems(const Scene::AnalysisSceneResult& sceneData) {
        m_cachedSceneData = std::make_shared<Scene::AnalysisSceneResult>(sceneData);
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
        if (unmatched > 0) { // [LOG]
            LOG_WARN("distributeDrawItems: {} draw items had no matching stage/subpass", unmatched); // [LOG]
        }

        for (auto& [key, items] : groups) {

            auto it = m_subpassRecorders.find(key);
            if (it != m_subpassRecorders.end()) {
                it->second->setDrawItems(items);
            }
            else {
                LOG_WARN("No recorder found for key {}", key); // [LOG]
            }
        }
    }

    //void DeferredRenderPath::update(const glm::mat4& view, const glm::mat4& proj, float deltaTime) {
    //    LOG_INFO("update: view/proj updated, deltaTime={}", deltaTime); // [LOG]

    //    m_lastView = view;
    //    m_lastProj = proj;
    //    m_lastDeltaTime = deltaTime;

    //    if (!m_cachedSceneData) {
    //        LOG_WARN("update: no cached scene data, skipping"); // [LOG]
    //        return;
    //    }

    //    size_t totalPipelinesCreated = 0; // [LOG]

    //    for (auto& [stage, passInfo] : m_stagePassInfo) {
    //        for (auto& [queue, subpass] : passInfo.queueToSubpass) {
    //            uint64_t key = (static_cast<uint64_t>(stage) << 32) | subpass;
    //            auto recorderIt = m_subpassRecorders.find(key);
    //            if (recorderIt == m_subpassRecorders.end()) continue;
    //            auto& recorder = recorderIt->second;
    //            auto& items = recorder->getDrawItems();
    //            if (items.empty()) continue;

    //            LOG_DEBUG("update: stage={}, subpass={}, items={}", static_cast<int>(stage), subpass, items.size()); // [LOG]

    //            // 收集该子通道中所有唯一的材质状态（PSO）
    //            std::unordered_map<size_t, std::shared_ptr<Scene::GraphicsPipelineState>> uniquePSOs;
    //            for (auto& item : items) {
    //                if (item->pipelineIndex >= m_cachedSceneData->PSO.size()) {
    //                    LOG_WARN("item pipelineIndex {} out of range (PSO size={})", // [LOG]
    //                        item->pipelineIndex, m_cachedSceneData->PSO.size()); // [LOG]
    //                    continue;
    //                }
    //                auto& pso = m_cachedSceneData->PSO[item->pipelineIndex];
    //                size_t hash = std::hash<Scene::GraphicsPipelineState>{}(*pso);
    //                uniquePSOs[hash] = pso;
    //            }

    //            LOG_DEBUG("update: {} unique PSOs found for subpass", uniquePSOs.size()); // [LOG]

    //            // 为每个唯一状态创建管线
    //            std::vector<RHI::PipelineHandle> pipelines;
    //            for (auto& [hash, pso] : uniquePSOs) {
    //                auto pipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
    //                    m_resMgr.get(), *pso, passInfo.renderPassHandle, subpass);
    //                if (pipeline.isValid()) {
    //                    pipelines.push_back(pipeline);
    //                    ++totalPipelinesCreated; // [LOG]
    //                }
    //                else {
    //                    LOG_ERROR("Failed to create pipeline for PSO hash {}", hash); // [LOG]
    //                }
    //            }
    //            recorder->setPipelines(pipelines);
    //            LOG_DEBUG("update: set {} pipelines for subpass", pipelines.size()); // [LOG]
    //        }
    //    }
    //    LOG_INFO("update: completed, total pipelines created/retrieved: {}", totalPipelinesCreated); // [LOG]
    //}

    void DeferredRenderPath::update(const glm::mat4& view, const glm::mat4& proj, float deltaTime) {
        m_lastView = view;
        m_lastProj = proj;
        m_lastDeltaTime = deltaTime;

        if (!m_cachedSceneData) {
            LOG_WARN("update: no cached scene data, skipping");
            return;
        }

        size_t totalPipelinesCreated = 0;

        for (auto& [stage, passInfo] : m_stagePassInfo) {
            for (auto& [queue, subpass] : passInfo.queueToSubpass) {
                uint64_t key = (static_cast<uint64_t>(stage) << 32) | subpass;
                auto recorderIt = m_subpassRecorders.find(key);
                if (recorderIt == m_subpassRecorders.end()) continue;
                auto& recorder = recorderIt->second;
                auto& items = recorder->getDrawItems();
                if (items.empty()) continue;

                // 收集唯一 PSO，并建立全局索引 -> 局部索引的映射
                std::unordered_map<size_t, std::shared_ptr<Scene::GraphicsPipelineState>> uniquePSOs;
                std::unordered_map<uint32_t, uint32_t> globalToLocal; 
                uint32_t localIdx = 0;                                

                for (auto& item : items) {
                    if (item->pipelineIndex >= m_cachedSceneData->PSO.size()) {
                        LOG_WARN("item pipelineIndex {} out of range (PSO size={})",
                            item->pipelineIndex, m_cachedSceneData->PSO.size());
                        continue;
                    }
                    auto& pso = m_cachedSceneData->PSO[item->pipelineIndex];
                    size_t hash = std::hash<Scene::GraphicsPipelineState>{}(*pso);
                    auto it = uniquePSOs.find(hash);
                    if (it == uniquePSOs.end()) {
                        uniquePSOs[hash] = pso;
                        // 记录当前全局索引对应的局部索引（局部索引即本次插入的顺序）
                        globalToLocal[item->pipelineIndex] = localIdx;
                        ++localIdx;
                    }
                }

                // 为每个唯一状态创建管线（按照 uniquePSOs 的迭代顺序，与映射一致）
                std::vector<RHI::PipelineHandle> pipelines;
                for (auto& [hash, pso] : uniquePSOs) {
                    auto pipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
                        m_resMgr.get(), *pso, passInfo.renderPassHandle, subpass);
                    if (pipeline.isValid()) {
                        pipelines.push_back(pipeline);
                        ++totalPipelinesCreated;
                    }
                    else {
                        LOG_ERROR("Failed to create pipeline for PSO hash {}", hash);
                    }
                }

                // 将 pipelines 和映射传递给 recorder
                recorder->setPipelines(pipelines);
                recorder->setGlobalToLocalMapping(globalToLocal);  // 新增：传递映射
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

        for (auto& [name, desc] : m_textureDescs) {
            desc.extent.width = width;
            desc.extent.height = height;
        }

        if (!initialize()) {
            LOG_ERROR("Failed to rebuild render path on resize");
        }
    }

} // namespace StarryEngine