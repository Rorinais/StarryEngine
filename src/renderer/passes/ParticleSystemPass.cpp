#include "ParticleSystemPass.hpp"
#include "../passExecutor/CopyToSwapchainExecutor.hpp"
#include "../passExecutor/ParticleCSExecutor.hpp"
#include "../passExecutor/ParticleRenderExecutor.hpp"
#include "../RenderTypes.hpp"
#include "../../assets/loader/ShaderLoader.hpp"
#include "../../assets/material/MaterialTemplate.hpp"
#include "../../logging/Logger.hpp"
#include <algorithm>

namespace StarryEngine {

    // 默认初始数据生成器
    static void defaultInitParticle(uint32_t i, float* data) {
        auto rnd = [](uint32_t s) { return float((s * 2654435761u) & 0xFFFF) / 65535.0f; };
        data[0] = (rnd(i * 3 + 1) - 0.5f) * 0.8f;   // x
        data[1] = rnd(i * 3 + 2) * 3.0f - 3.0f;      // y
        data[2] = (rnd(i * 3 + 3) - 0.5f) * 0.8f;    // z
        data[3] = rnd(i * 7 + 4) * 0.95f;             // lifetime
    }

    ParticleSystemPass::ParticleSystemPass(const ParticleSystemDesc& desc)
        : m_desc(desc) {
        if (!m_desc.initParticle)
            m_desc.initParticle = defaultInitParticle;
    }

    void ParticleSystemPass::destroyResources(std::shared_ptr<RHI::ResourceManager> resMgr) {
        auto destroy = [&](auto& h) { if (h.isValid()) { resMgr->destroy(h); h = {}; } };
        destroy(m_particleVS); destroy(m_particleFS); destroy(m_particleCS);
        destroy(m_particleCSDescLayout); destroy(m_particleCSLayout);
        destroy(m_particleCSPipeline); destroy(m_particleRenderLayout);
        destroy(m_particleRenderDescLayout); destroy(m_particleRenderSet1Layout);
        destroy(m_particleCSPool); destroy(m_particleRenderPool);
    }

    void ParticleSystemPass::loadShaders(std::shared_ptr<RHI::ResourceManager> resMgr) {
        Assets::ShaderLoader loader(resMgr);

        auto csInfo = loader.loadFromFile(m_desc.computeShader, RHI::ShaderStage::Compute);
        if (!csInfo || !csInfo->module.isValid()) {
            LOG_WARN("[{}] Compute shader failed: {}", m_desc.name, m_desc.computeShader);
            return;
        }
        m_particleCS = csInfo->module;

        auto vsInfo = loader.loadFromFile(m_desc.vertexShader, RHI::ShaderStage::Vertex);
        auto fsInfo = loader.loadFromFile(m_desc.fragmentShader, RHI::ShaderStage::Fragment);
        if (!vsInfo || !fsInfo) {
            LOG_WARN("[{}] Render shaders failed", m_desc.name);
            return;
        }
        m_particleVS = vsInfo->module;
        m_particleFS = fsInfo->module;
    }

    void ParticleSystemPass::createComputeResources(std::shared_ptr<RHI::ResourceManager> resMgr) {
        RHI::DescriptorSetLayoutDesc csLayoutDesc;
        csLayoutDesc.bindings = {{0, RHI::DescriptorType::StorageBuffer, 1, RHI::ShaderStage::Compute}};
        m_particleCSDescLayout = resMgr->createDescriptorSetLayout(csLayoutDesc);

        RHI::PipelineLayoutDesc csPlDesc;
        csPlDesc.descriptorSetLayouts = { m_particleCSDescLayout };
        csPlDesc.pushConstants = {{RHI::ShaderStage::Compute, 0, 48}};
        m_particleCSLayout = resMgr->createPipelineLayout(csPlDesc);

        RHI::ComputePipelineDesc cpDesc;
        cpDesc.computeShader = m_particleCS;
        cpDesc.pipelineLayoutHandle = m_particleCSLayout;
        m_particleCSPipeline = resMgr->createComputePipeline(cpDesc);
    }

    void ParticleSystemPass::createRenderPipelineLayout(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        RHI::DescriptorSetLayoutHandle globalSetLayout)
    {
        RHI::DescriptorSetLayoutDesc renderLayout1;
        renderLayout1.bindings = {{0, RHI::DescriptorType::StorageBuffer, 1, RHI::ShaderStage::Vertex}};
        m_particleRenderDescLayout = resMgr->createDescriptorSetLayout(renderLayout1);

        RHI::PipelineLayoutDesc renderPlDesc;
        renderPlDesc.descriptorSetLayouts = { globalSetLayout, m_particleRenderDescLayout };
        renderPlDesc.pushConstants = {{RHI::ShaderStage::Vertex, 64, 56}};
        m_particleRenderLayout = resMgr->createPipelineLayout(renderPlDesc);
    }

    // ──── IPass ────────────────────────────────────────────────────

    bool ParticleSystemPass::configure(
        RenderGraph::RenderGraph& graph,
        std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap,
        uint32_t width, uint32_t height,
        std::shared_ptr<RHI::ResourceManager> resMgr,
        RHI::DescriptorSetLayoutHandle globalSetLayout)
    {
        if (!globalSetLayout.isValid()) return false;

        destroyResources(resMgr);
        loadShaders(resMgr);
        if (!m_particleCS.isValid() || !m_particleVS.isValid()) return false;

        createComputeResources(resMgr);
        createRenderPipelineLayout(resMgr, globalSetLayout);

        // 粒子存储 buffer
        std::string bufName = m_desc.name + "_Buffer";
        RHI::BufferDesc bufDesc;
        bufDesc.size = m_desc.particleCount * m_desc.perParticleFloats * sizeof(float);
        bufDesc.type = RHI::BufferType::Storage;
        bufDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
        bufDesc.allowUpdate = true;
        m_particleBufferId = graph.createVirtualBuffer(bufDesc, bufName);

        // Compute pass node
        std::string csPassName = m_desc.name + "_Update";
        m_csPassNode = graph.addComputePassNode(csPassName);
        m_csPassNode->addWriteBuffer(m_particleBufferId);
        m_csPassNode->setComputePipeline(m_particleCSPipeline);
        uint32_t threadGroupsX = (m_desc.particleCount + 255) / 256;
        m_csPassNode->setDispatchSize(threadGroupsX, 1, 1);

        // Graphics pass node
        std::string renderPassName = m_desc.name + "_Render";
        m_renderPassNode = graph.addGraphicsPassNode(renderPassName);
        m_renderPassNode->setRenderArea(width, height);
        m_renderPassNode->addReadBuffer(m_particleBufferId);

        RenderGraph::AttachmentParams colorParams;
        colorParams.loadOp = RHI::AttachmentLoadOp::Load;
        colorParams.storeOp = RHI::AttachmentStoreOp::Store;
        colorParams.initialLayout = RHI::ImageLayout::ShaderReadOnly;
        colorParams.finalLayout = RHI::ImageLayout::ShaderReadOnly;
        auto scId = graph.getTextureId("SceneColor");
        std::string colorKey = m_renderPassNode->addColorOutput(scId, colorParams);

        std::string subpassTag = m_desc.name + "_Draw";
        auto& subpass = m_renderPassNode->addSubpass(subpassTag);
        subpass.addColorAttachmentRef(colorKey);
        subpass.setTag(subpassTag);

        m_currentExecutor = std::make_shared<CopyToSwapchainExecutor>();
        subpass.setExecutor(m_currentExecutor);

        LOG_INFO("[{}] Configured: {} particles, buffer={}", m_desc.name, m_desc.particleCount, bufName);
        return true;
    }

    std::vector<PassSubpassInfo> ParticleSystemPass::getSubpasses() const {
        if (!m_currentExecutor || !m_renderPassNode) return {};
        std::string tag = m_desc.name + "_Draw";
        return {{tag, 0, m_currentExecutor, m_renderPassNode}};
    }

    void ParticleSystemPass::onAfterCompile(const CompileContext& ctx) {
        if (!m_renderPassNode) return;
        auto resMgr = ctx.resMgr;
        auto renderGraph = ctx.renderGraph;
        auto globalDescSet = ctx.globalDescSet;

        if (m_particleCSPool.isValid())    { resMgr->destroy(m_particleCSPool);    m_particleCSPool    = {}; }
        if (m_particleRenderPool.isValid()) { resMgr->destroy(m_particleRenderPool); m_particleRenderPool = {}; }

        auto physBuf = renderGraph->getPhysicalBuffer(m_particleBufferId);
        if (!physBuf.isValid()) return;

        // 填充 buffer 初始数据
        auto* bufObj = resMgr->getBuffer(physBuf);
        if (bufObj) {
            uint32_t stride = m_desc.perParticleFloats;
            std::vector<float> init(m_desc.particleCount * stride, 0.0f);
            for (uint32_t i = 0; i < m_desc.particleCount; ++i) {
                m_desc.initParticle(i, init.data() + i * stride);
            }
            bufObj->update(init.data(), init.size() * sizeof(float), 0);
        }

        // Compute descriptor pool + set
        RHI::DescriptorPoolDesc csPoolDesc;
        csPoolDesc.maxSets = 1;
        csPoolDesc.poolSizes = {{RHI::DescriptorType::StorageBuffer, 1}};
        m_particleCSPool = resMgr->createDescriptorPool(csPoolDesc);
        RHI::DescriptorSetDesc csSetDesc;
        csSetDesc.descriptorSetLayout = m_particleCSDescLayout;
        csSetDesc.descriptorPool = m_particleCSPool;
        auto csDescSet = resMgr->createDescriptorSet(csSetDesc);
        if (csDescSet.isValid()) {
            auto* ds = resMgr->getDescriptorSet(csDescSet);
            ds->writeBuffer(0, 0, resMgr->getBuffer(physBuf), 0,
                            m_desc.particleCount * m_desc.perParticleFloats * sizeof(float));
            ds->update();
        }

        std::string csPassName = m_desc.name + "_Update";
        for (auto& p : renderGraph->getPasses()) {
            if (p->getName() == csPassName) {
                p->setComputeExecutor(std::make_shared<ParticleCSExecutor>(
                    m_particleCSLayout, csDescSet, m_desc.particleCount, m_desc.params));
                break;
            }
        }

        RHI::RenderPassHandle rp = m_renderPassNode->getRenderPassHandle();
        if (!rp.isValid()) return;

        GraphicsPipelineState pso;
        pso.vertexShader = m_particleVS;
        pso.fragmentShader = m_particleFS;
        pso.layout = m_particleRenderLayout;
        pso.cullMode = RHI::CullMode::None;
        pso.depthTestEnable = false;
        pso.depthWriteEnable = false;
        pso.topology = RHI::PrimitiveTopology::PointList;
        pso.dynamicStates = {RHI::DynamicState::Viewport, RHI::DynamicState::Scissor};
        pso.vertexInput = {};
        RHI::BlendAttachmentState blend;
        blend.blendEnable = true;
        blend.srcColorBlendFactor = RHI::BlendFactor::SrcAlpha;
        blend.dstColorBlendFactor = RHI::BlendFactor::OneMinusSrcAlpha;
        pso.attachments = {blend};

        auto renderPipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
            resMgr.get(), pso, rp, 0);
        if (!renderPipeline.isValid()) { LOG_ERROR("[{}] Particle pipeline failed", m_desc.name); return; }

        // Render descriptor pool + set
        RHI::DescriptorSetHandle particleDescSet;
        if (physBuf.isValid()) {
            RHI::DescriptorSetLayoutDesc set1Desc;
            set1Desc.bindings = {{0, RHI::DescriptorType::StorageBuffer, 1, RHI::ShaderStage::Vertex}};
            m_particleRenderSet1Layout = resMgr->createDescriptorSetLayout(set1Desc);
            RHI::DescriptorPoolDesc poolDesc;
            poolDesc.maxSets = 2;
            poolDesc.poolSizes = {{RHI::DescriptorType::UniformBuffer, 1},
                                  {RHI::DescriptorType::StorageBuffer, 1}};
            m_particleRenderPool = resMgr->createDescriptorPool(poolDesc);
            RHI::DescriptorSetDesc dsDesc1;
            dsDesc1.descriptorSetLayout = m_particleRenderSet1Layout;
            dsDesc1.descriptorPool = m_particleRenderPool;
            particleDescSet = resMgr->createDescriptorSet(dsDesc1);
            if (particleDescSet.isValid()) {
                auto* ds = resMgr->getDescriptorSet(particleDescSet);
                ds->writeBuffer(0, 0, resMgr->getBuffer(physBuf), 0,
                                m_desc.particleCount * m_desc.perParticleFloats * sizeof(float));
                ds->update();
            }
        }

        auto rec = std::make_shared<ParticleRenderExecutor>(
            renderPipeline, m_particleRenderLayout, globalDescSet, particleDescSet,
            m_desc.particleCount);
        rec->setVSParams(m_desc.params);
        m_currentExecutor = rec;
        m_renderPassNode->setPassExecutor(0, rec);

        LOG_INFO("[{}] Ready ({} points)", m_desc.name, m_desc.particleCount);
    }

} // namespace StarryEngine
