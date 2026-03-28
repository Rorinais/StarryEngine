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
        LOG_INFO("ForwardRenderPath::setTextureDescs called, size = {}", m_textureDescs.size());
    }

    bool ForwardRenderPath::initialize() {
        LOG_INFO("ForwardRenderPath::initialize, m_textureDescs size = {}", m_textureDescs.size());
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
                    RHI::ImageLayout::PresentSrc, name);  
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

        // 编译 RenderGraph
        if (!m_renderGraph->compile()) {
            LOG_ERROR("Failed to compile RenderGraph");
            return false;
        }

        // 统一处理所有子通道的固定管线创建和纹理绑定
        for (auto& [stage, queueMap] : m_config) {
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
                        LOG_ERROR("Failed to create pipeline for subpass: {}", subpassCfg.name);
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
        m_lastView = view;
        m_lastProj = proj;
        m_lastDeltaTime = deltaTime;

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
            }
        }
    }

    void ForwardRenderPath::render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) {
        if (!m_renderGraph) return;

        auto context = buildRenderContext();
        m_renderGraph->execute(encoder, context, frameIndex);
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