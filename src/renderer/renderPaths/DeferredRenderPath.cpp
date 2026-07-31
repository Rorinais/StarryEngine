#include "DeferredRenderPath.hpp"
#include "../passExecutor/CopyToSwapchainExecutor.hpp"
#include "../passExecutor/ParticleCSExecutor.hpp"
#include "../passExecutor/ParticleRenderExecutor.hpp"
#include "../../logging/Logger.hpp"
#include "../../ui/ImGuiManager.hpp"
#include "../../assets/loader/ShaderLoader.hpp"
#include <algorithm>

namespace StarryEngine {

    // ──── DeferredRenderPath ──────────────────────────────────────

    DeferredRenderPath::DeferredRenderPath(std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height)
        : BaseRenderPath(rhi, width, height) {}

    void DeferredRenderPath::setConfig(const RenderPathConfig& config) { m_config = config; }

    void DeferredRenderPath::setDrawItems(const Scene::AnalysisSceneResult& sceneData) {
        m_cachedSceneData = std::make_shared<Scene::AnalysisSceneResult>(sceneData);
        updateMaterialTextures(sceneData);
        distributeDrawItems(sceneData);
    }

    void DeferredRenderPath::setImGuiManager(ImGuiManager* mgr, uint32_t imageCount) {
        m_imguiManager = mgr;
        m_imguiImageCount = imageCount;
    }

    // ──── Config Pass Building ────────────────────────────────────

    void DeferredRenderPath::buildConfigPasses(
        std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap)
    {
        for (const auto& passDesc : m_config) {
            auto* passNode = m_renderGraph->addGraphicsPassNode(passDesc.name);
            passNode->setRenderArea(m_width, m_height);

            std::unordered_map<RenderGraph::TextureId, std::string> colorKeyMap, depthKeyMap,
                                                                       inputKeyMap, resolveKeyMap, preserveKeyMap;

            for (size_t subpassIdx = 0; subpassIdx < passDesc.subpasses.size(); ++subpassIdx) {
                const auto& subpassCfg = passDesc.subpasses[subpassIdx];
                auto& subpassBuilder = passNode->addSubpass(subpassCfg.name);
                subpassBuilder.setTag(subpassCfg.tag);

                std::vector<std::string> colorKeys;
                for (auto& att : subpassCfg.colorAttachments) {
                    auto texId = texIdMap.at(att.textureName);
                    auto& key = colorKeyMap[texId];
                    if (key.empty()) key = passNode->addColorOutput(texId, att.params);
                    colorKeys.push_back(key);
                }

                std::string depthKey;
                if (subpassCfg.depthAttachment) {
                    auto& att = *subpassCfg.depthAttachment;
                    auto texId = texIdMap.at(att.textureName);
                    auto& key = depthKeyMap[texId];
                    if (key.empty()) key = passNode->addDepthOutput(texId, att.params);
                    depthKey = key;
                }

                std::vector<std::string> inputKeys;
                for (auto& att : subpassCfg.inputAttachments) {
                    auto texId = texIdMap.at(att.textureName);
                    auto& key = inputKeyMap[texId];
                    if (key.empty()) key = passNode->addInput(texId, att.params);
                    inputKeys.push_back(key);
                }

                for (auto& att : subpassCfg.resolveAttachments) {
                    auto texId = texIdMap.at(att.textureName);
                    auto& key = resolveKeyMap[texId];
                    if (key.empty()) key = passNode->addResolve(texId, att.params);
                }
                for (auto& texName : subpassCfg.preserveAttachments) {
                    auto texId = texIdMap.at(texName);
                    auto& key = preserveKeyMap[texId];
                    if (key.empty()) key = passNode->addPreserve(texId);
                }

                for (auto& k : colorKeys) subpassBuilder.addColorAttachmentRef(k);
                if (!depthKey.empty()) subpassBuilder.addDepthStencilAttachmentRef(depthKey);
                for (auto& k : inputKeys) subpassBuilder.addInputAttachmentRef(k);

                subpassBuilder.setExecutor(subpassCfg.executor);
                m_tagToSubpass[subpassCfg.tag] = SubpassTarget{
                    {}, static_cast<uint32_t>(subpassIdx), subpassCfg.executor
                };
                m_tagToPassNode[subpassCfg.tag] = passNode;
            }
        }
        buildTestComputePass();

        // 清除 ImGuiManager 对旧 RenderGraph 的引用，否则 reset 后旧图仍存活
        if (m_imguiManager) m_imguiManager->setRenderGraph(nullptr);
    }

    // ──── Rebuild ─────────────────────────────────────────────────

    void DeferredRenderPath::doRebuildResources(const Scene::AnalysisSceneResult& sceneData) {
        updateMaterialTextures(sceneData);
        distributeDrawItems(sceneData);
        prepareAllPipelines(sceneData);
    }

    void DeferredRenderPath::onAfterCompile() {
        if (m_tagToSubpass.count("ParticleDraw") && m_particleVS.isValid())
            prepareParticlePipeline();
    }

    void DeferredRenderPath::onAfterCompileImGui() {
        if (!m_imguiManager) return;
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

    // ──── Particle System ─────────────────────────────────────────

    void DeferredRenderPath::buildTestComputePass() {
        if (!m_globalSetLayout.isValid()) return;
        const uint32_t PARTICLE_COUNT = 1024;

        auto destroyHandle = [this](auto& h) { if (h.isValid()) { m_resMgr->destroy(h); h = {}; } };
        destroyHandle(m_particleVS); destroyHandle(m_particleFS); destroyHandle(m_particleCS);
        destroyHandle(m_particleCSDescLayout); destroyHandle(m_particleCSLayout);
        destroyHandle(m_particleCSPipeline); destroyHandle(m_particleRenderLayout);
        destroyHandle(m_particleRenderDescLayout);

        Assets::ShaderLoader loader(m_resMgr);
        RHI::BufferDesc bufDesc;
        bufDesc.size = PARTICLE_COUNT * sizeof(float) * 4;
        bufDesc.type = RHI::BufferType::Storage;
        bufDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
        bufDesc.allowUpdate = true;
        auto bufId = m_renderGraph->createVirtualBuffer(bufDesc, "ParticleBuffer");

        auto csInfo = loader.loadFromFile("assets/shaders/test/particle.comp", RHI::ShaderStage::Compute);
        if (!csInfo || !csInfo->module.isValid()) { LOG_WARN("Particle CS failed"); return; }

        RHI::DescriptorSetLayoutDesc csLayoutDesc;
        csLayoutDesc.bindings = {{0, RHI::DescriptorType::StorageBuffer, 1, RHI::ShaderStage::Compute}};
        auto csDescLayout = m_resMgr->createDescriptorSetLayout(csLayoutDesc);
        RHI::PipelineLayoutDesc csPlDesc;
        csPlDesc.descriptorSetLayouts = { csDescLayout };
        csPlDesc.pushConstants = {{RHI::ShaderStage::Compute, 0, 48}};
        auto csPlLayout = m_resMgr->createPipelineLayout(csPlDesc);
        RHI::ComputePipelineDesc cpDesc;
        cpDesc.computeShader = csInfo->module; cpDesc.pipelineLayoutHandle = csPlLayout;
        auto csPipeline = m_resMgr->createComputePipeline(cpDesc);

        auto vsInfo = loader.loadFromFile("assets/shaders/test/particle.vert", RHI::ShaderStage::Vertex);
        auto fsInfo = loader.loadFromFile("assets/shaders/test/particle.frag", RHI::ShaderStage::Fragment);
        if (!vsInfo || !fsInfo) { LOG_WARN("Particle render shaders failed"); return; }

        RHI::DescriptorSetLayoutDesc renderLayout1;
        renderLayout1.bindings = {{0, RHI::DescriptorType::StorageBuffer, 1, RHI::ShaderStage::Vertex}};
        m_particleRenderDescLayout = m_resMgr->createDescriptorSetLayout(renderLayout1);
        RHI::PipelineLayoutDesc renderPlDesc;
        renderPlDesc.descriptorSetLayouts = { m_globalSetLayout, m_particleRenderDescLayout };
        renderPlDesc.pushConstants = {{RHI::ShaderStage::Vertex, 64, 56}};
        auto renderPlLayout = m_resMgr->createPipelineLayout(renderPlDesc);

        auto* csPass = m_renderGraph->addComputePassNode("ParticleUpdate");
        csPass->addWriteBuffer(bufId);
        csPass->setComputePipeline(csPipeline);
        csPass->setDispatchSize((PARTICLE_COUNT + 255) / 256, 1, 1);

        auto* renderPass = m_renderGraph->addGraphicsPassNode("ParticleRender");
        renderPass->setRenderArea(m_width, m_height);
        renderPass->addReadBuffer(bufId);

        RenderGraph::AttachmentParams colorParams;
        colorParams.loadOp = RHI::AttachmentLoadOp::Load;
        colorParams.storeOp = RHI::AttachmentStoreOp::Store;
        colorParams.initialLayout = RHI::ImageLayout::ShaderReadOnly;
        colorParams.finalLayout = RHI::ImageLayout::ShaderReadOnly;
        auto scId = m_renderGraph->getTextureId("SceneColor");
        std::string colorKey = renderPass->addColorOutput(scId, colorParams);
        auto& subpass = renderPass->addSubpass("ParticleDraw");
        subpass.addColorAttachmentRef(colorKey);
        subpass.setTag("ParticleDraw");

        auto dummy = std::make_shared<CopyToSwapchainExecutor>();
        subpass.setExecutor(dummy);
        m_tagToSubpass["ParticleDraw"] = SubpassTarget{{}, 0, dummy};
        m_tagToPassNode["ParticleDraw"] = renderPass;

        m_particleVS = vsInfo->module; m_particleFS = fsInfo->module; m_particleCS = csInfo->module;
        m_particleRenderLayout = renderPlLayout; m_particleBufferId = bufId;
        m_particleCount = PARTICLE_COUNT;
        m_particleCSDescLayout = csDescLayout; m_particleCSLayout = csPlLayout;
        m_particleCSPipeline = csPipeline;

        LOG_INFO("Particle system: {} particles", PARTICLE_COUNT);
    }

    void DeferredRenderPath::prepareParticlePipeline() {
        // 销毁旧资源
        if (m_particleCSPool.isValid())    { m_resMgr->destroy(m_particleCSPool);    m_particleCSPool    = {}; }
        if (m_particleRenderPool.isValid()) { m_resMgr->destroy(m_particleRenderPool); m_particleRenderPool = {}; }
        if (m_particleRenderSet1Layout.isValid()) { m_resMgr->destroy(m_particleRenderSet1Layout); m_particleRenderSet1Layout = {}; }

        auto physBuf = m_renderGraph->getPhysicalBuffer(m_particleBufferId);
        if (!physBuf.isValid()) return;

        auto* bufObj = m_resMgr->getBuffer(physBuf);
        if (bufObj) {
            std::vector<float> init(m_particleCount * 4, 0.0f);
            for (uint32_t i = 0; i < m_particleCount; ++i) {
                auto rnd = [](uint32_t s) { return float((s * 2654435761u) & 0xFFFF) / 65535.0f; };
                init[i*4+0] = (rnd(i*3+1) - 0.5f) * 0.8f;
                init[i*4+1] = rnd(i*3+2) * 3.0f - 3.0f;
                init[i*4+2] = (rnd(i*3+3) - 0.5f) * 0.8f;
                init[i*4+3] = rnd(i*7+4) * 0.95f;
            }
            bufObj->update(init.data(), init.size() * sizeof(float), 0);
        }

        RHI::DescriptorPoolDesc csPoolDesc;
        csPoolDesc.maxSets = 1; csPoolDesc.poolSizes = {{RHI::DescriptorType::StorageBuffer, 1}};
        m_particleCSPool = m_resMgr->createDescriptorPool(csPoolDesc);
        RHI::DescriptorSetDesc csSetDesc;
        csSetDesc.descriptorSetLayout = m_particleCSDescLayout; csSetDesc.descriptorPool = m_particleCSPool;
        auto csDescSet = m_resMgr->createDescriptorSet(csSetDesc);
        if (csDescSet.isValid()) {
            auto* ds = m_resMgr->getDescriptorSet(csDescSet);
            ds->writeBuffer(0, 0, m_resMgr->getBuffer(physBuf), 0, m_particleCount * sizeof(float) * 4);
            ds->update();
        }

        for (auto& p : m_renderGraph->getPasses()) {
            if (p->getName() == "ParticleUpdate") {
                p->setComputeExecutor(std::make_shared<ParticleCSExecutor>(
                    m_particleCSLayout, csDescSet, m_particleCount, m_particleParams));
                break;
            }
        }

        auto tagIt = m_tagToSubpass.find("ParticleDraw");
        if (tagIt == m_tagToSubpass.end()) return;
        RHI::RenderPassHandle rp = tagIt->second.renderPass;
        if (!rp.isValid()) return;

        Scene::GraphicsPipelineState pso;
        pso.vertexShader = m_particleVS; pso.fragmentShader = m_particleFS;
        pso.layout = m_particleRenderLayout;
        pso.cullMode = RHI::CullMode::None; pso.depthTestEnable = false; pso.depthWriteEnable = false;
        pso.topology = RHI::PrimitiveTopology::PointList;
        pso.dynamicStates = {RHI::DynamicState::Viewport, RHI::DynamicState::Scissor};
        pso.vertexInput = {};
        RHI::BlendAttachmentState blend; blend.blendEnable = true;
        blend.srcColorBlendFactor = RHI::BlendFactor::SrcAlpha;
        blend.dstColorBlendFactor = RHI::BlendFactor::OneMinusSrcAlpha;
        blend.colorBlendOp = RHI::BlendOp::Add;
        pso.attachments = {blend};

        auto renderPipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
            m_resMgr.get(), pso, rp, tagIt->second.subpassIndex);
        if (!renderPipeline.isValid()) { LOG_ERROR("Particle pipeline failed"); return; }

        RHI::DescriptorSetHandle particleDescSet;
        if (physBuf.isValid()) {
            RHI::DescriptorSetLayoutDesc set1Desc;
            set1Desc.bindings = {{0, RHI::DescriptorType::StorageBuffer, 1, RHI::ShaderStage::Vertex}};
            m_particleRenderSet1Layout = m_resMgr->createDescriptorSetLayout(set1Desc);
            RHI::DescriptorPoolDesc poolDesc;
            poolDesc.maxSets = 2;
            poolDesc.poolSizes = {{RHI::DescriptorType::UniformBuffer, 1}, {RHI::DescriptorType::StorageBuffer, 1}};
            m_particleRenderPool = m_resMgr->createDescriptorPool(poolDesc);
            RHI::DescriptorSetDesc dsDesc1;
            dsDesc1.descriptorSetLayout = m_particleRenderSet1Layout; dsDesc1.descriptorPool = m_particleRenderPool;
            particleDescSet = m_resMgr->createDescriptorSet(dsDesc1);
            if (particleDescSet.isValid()) {
                auto* ds = m_resMgr->getDescriptorSet(particleDescSet);
                ds->writeBuffer(0, 0, m_resMgr->getBuffer(physBuf), 0, m_particleCount * sizeof(float) * 4);
                ds->update();
            }
        }

        auto rec = std::make_shared<ParticleRenderExecutor>(
            renderPipeline, m_particleRenderLayout, m_globalDescSet, particleDescSet, m_particleCount);
        rec->setVSParams(m_particleParams);
        tagIt->second.executor = rec;
        auto passIt = m_tagToPassNode.find("ParticleDraw");
        if (passIt != m_tagToPassNode.end()) passIt->second->setPassExecutor(0, rec);

        LOG_INFO("Particle render pipeline ready ({} points)", m_particleCount);
    }

    // ──── Draw Item Distribution ──────────────────────────────────

    void DeferredRenderPath::distributeDrawItems(const Scene::AnalysisSceneResult& sceneData) {
        for (auto& [tag, target] : m_tagToSubpass) target.executor->clearDrawItems();
        std::string defaultTag = m_config.front().subpasses.front().tag;
        for (auto& item : sceneData.drawItems) {
            std::string tag = item->passTag.empty() ? defaultTag : item->passTag;
            auto it = m_tagToSubpass.find(tag);
            if (it != m_tagToSubpass.end()) it->second.executor->addDrawItem(item);
            else LOG_ERROR("No subpass for tag '{}'", tag);
        }
    }

    void DeferredRenderPath::updateMaterialTextures(const Scene::AnalysisSceneResult& sceneData) {
        if (!m_renderGraph || m_textureIdMap.empty()) { LOG_WARN("RG not ready for textures"); return; }
        RHI::TextureHandle swapchainPhys;
        auto it = m_textureIdMap.find("Swapchain");
        if (it != m_textureIdMap.end()) swapchainPhys = m_renderGraph->getPhysicalTextureHandle(it->second);

        for (auto& material : sceneData.materials) {
            if (!material) continue;
            for (const auto& [texName, dep] : material->getTextureDependencies()) {
                if (texName == "Swapchain") { LOG_ERROR("Material depends on Swapchain!"); continue; }
                auto texIt = m_textureIdMap.find(texName);
                if (texIt == m_textureIdMap.end()) continue;
                auto phys = m_renderGraph->getPhysicalTextureHandle(texIt->second);
                if (!phys.isValid() || (swapchainPhys.isValid() && phys == swapchainPhys)) continue;
                switch (dep.type) {
                case Assets::ResourceDependencyType::Sampler:
                    material->setTexture(dep.set, dep.binding, phys, m_defaultSampler); break;
                case Assets::ResourceDependencyType::InputAttachment:
                    material->setInputAttachment(dep.set, dep.binding, phys, RHI::ImageLayout::ShaderReadOnly); break;
                }
            }
        }
    }

    void DeferredRenderPath::prepareAllPipelines(const Scene::AnalysisSceneResult& sceneData) {
        for (auto& [tag, target] : m_tagToSubpass) {
            auto& items = target.executor->getDrawItems();
            if (items.empty()) continue;
            std::unordered_set<uint32_t> usedIndices;
            for (auto& item : items)
                if (item->pipelineIndex < sceneData.PSO.size()) usedIndices.insert(item->pipelineIndex);
            std::unordered_map<uint32_t, RHI::PipelineHandle> mapping;
            for (uint32_t idx : usedIndices) {
                const auto& pso = sceneData.PSO[idx];
                auto pipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
                    m_resMgr.get(), *pso, target.renderPass, target.subpassIndex);
                if (pipeline.isValid()) mapping[idx] = pipeline;
            }
            target.executor->setPipelineMapping(std::move(mapping));
        }
    }

} // namespace StarryEngine
