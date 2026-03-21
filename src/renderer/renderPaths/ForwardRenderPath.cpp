#include "ForwardRenderPath.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine {

    ForwardRenderPath::ForwardRenderPath(std::shared_ptr<RHI::IRHI> rhi,
        uint32_t width, uint32_t height)
        : m_rhi(rhi), m_resMgr(rhi->getResourceManager()),
        m_width(width), m_height(height) {
    }

    void ForwardRenderPath::setConfig(const RenderPathConfig& config) {
        m_config = config;
    }

    void ForwardRenderPath::setTextureDescs(const std::unordered_map<std::string, RHI::TextureDesc>& descs) {
        m_textureDescs = descs;
    }

    bool ForwardRenderPath::initialize() {
        if (m_config.empty()) {
            LOG_ERROR("ForwardRenderPath: No config set!");
            return false;
        }
        if (m_textureDescs.empty()) {
            LOG_ERROR("ForwardRenderPath: No texture descriptions set!");
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

    bool ForwardRenderPath::buildGraph() {
        m_stagePassNode.clear();
        m_subpassRecorders.clear();
        m_stagePassInfo.clear();

        m_renderGraph = std::make_shared<RenderGraph::RenderGraph>(m_rhi);
        m_renderGraph->setSwapchainImageCount(m_rhi->getSwapChainImageCount());

        std::unordered_map<std::string, RenderGraph::TextureId> texIdMap;

        for (const auto& [name, desc] : m_textureDescs) {
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

        m_renderGraph->dependencyAnalysis();
        if (!m_renderGraph->compile()) {
            LOG_ERROR("Failed to compile RenderGraph");
            return false;
        }
        m_renderGraph->createFrameBuffer();

        const auto& sortedPasses = m_renderGraph->getSortedPasses();
        for (auto& [stage, passNode] : m_stagePassNode) {
            auto it = std::find_if(sortedPasses.begin(), sortedPasses.end(),
                [passNode](auto* p) { return p == passNode; });
            if (it != sortedPasses.end()) {
                m_stagePassInfo[stage].renderPassHandle = (*it)->getRenderPassHandle();
            }
            else {
                LOG_ERROR("PassNode for stage {} not found in sorted passes", static_cast<int>(stage));
                return false;
            }
        }

        return true;
    }

    void ForwardRenderPath::setDrawItems(const Scene::AnalysisSceneResult& sceneData) {
        m_cachedSceneData = std::make_shared<Scene::AnalysisSceneResult>(sceneData);
        distributeDrawItems(sceneData);
    }

    void ForwardRenderPath::distributeDrawItems(const Scene::AnalysisSceneResult& sceneData) {
        for (auto& [key, recorder] : m_subpassRecorders) {
            recorder->clearDrawItems();
        }

        // 临时分组：键 = (stage << 32) | subpass
        std::unordered_map<uint64_t, std::vector<std::shared_ptr<Scene::DrawItem>>> groups;

        for (auto& item : sceneData.drawItems) {
            auto stageIt = m_stagePassInfo.find(item->stage);
            if (stageIt == m_stagePassInfo.end()) continue;
            const auto& passInfo = stageIt->second;

            auto queueIt = passInfo.queueToSubpass.find(item->queue);
            if (queueIt == passInfo.queueToSubpass.end()) continue;
            uint32_t subpass = queueIt->second;

            uint64_t key = (static_cast<uint64_t>(item->stage) << 32) | subpass;
            groups[key].push_back(item);
        }

        for (auto& [key, items] : groups) {
            auto it = m_subpassRecorders.find(key);
            if (it != m_subpassRecorders.end()) {
                it->second->setDrawItems(items);
            }
        }
    }

    void ForwardRenderPath::update(const glm::mat4& view, const glm::mat4& proj, float deltaTime) {
        if (!m_cachedSceneData) return;

        for (auto& [stage, passInfo] : m_stagePassInfo) {
            for (auto& [queue, subpass] : passInfo.queueToSubpass) {
                uint64_t key = (static_cast<uint64_t>(stage) << 32) | subpass;
                auto recorderIt = m_subpassRecorders.find(key);
                if (recorderIt == m_subpassRecorders.end()) continue;
                auto& recorder = recorderIt->second;
                auto& items = recorder->getDrawItems();
                if (items.empty()) continue;

                // 收集该子通道中所有唯一的材质状态（PSO）
                std::unordered_map<size_t, std::shared_ptr<Scene::GraphicsPipelineState>> uniquePSOs;
                for (auto& item : items) {
                    if (item->pipelineIndex >= m_cachedSceneData->PSO.size()) continue;
                    auto& pso = m_cachedSceneData->PSO[item->pipelineIndex];
                    size_t hash = std::hash<Scene::GraphicsPipelineState>{}(*pso);
                    uniquePSOs[hash] = pso;
                }

                // 为每个唯一状态创建管线
                std::vector<RHI::PipelineHandle> pipelines;
                for (auto& [hash, pso] : uniquePSOs) {
                    auto pipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
                        m_resMgr.get(), *pso, passInfo.renderPassHandle, subpass);
                    if (pipeline.isValid())
                        pipelines.push_back(pipeline);
                }
                recorder->setPipelines(pipelines);
            }
        }
    }

    void ForwardRenderPath::render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) {
        if (m_renderGraph) {
            m_renderGraph->execute(frameIndex, encoder);
        }
    }

    void ForwardRenderPath::onResize(uint32_t width, uint32_t height) {
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