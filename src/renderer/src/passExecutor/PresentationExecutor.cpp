#include <renderer/passExecutor/PresentationExecutor.hpp>
#include <renderer/graph/RenderGraph.hpp>
#include <assets/loader/ShaderLoader.hpp>
#include <assets/material/MaterialTemplate.hpp>
#include <logging/Logger.hpp>

namespace StarryEngine {

    PresentationExecutor::~PresentationExecutor() {
        if (m_resMgr) {
            if (m_pool.isValid()) m_resMgr->destroy(m_pool);
            if (m_sampler.isValid()) m_resMgr->destroy(m_sampler);
        }
    }

    // 自建呈现管线（原 BaseRenderPath::ensurePresentationShaders + preparePresentationPipeline）
    void PresentationExecutor::onPrepare(const ExecutorPrepareContext& ctx) {
        m_resMgr = ctx.resMgr;
        if (!m_resMgr || !ctx.renderGraph || !ctx.globalSetLayout.isValid()) return;

        Assets::ShaderLoader loader(m_resMgr);
        auto vsInfo = loader.loadFromFile("assets/shaders/deferred/fullscreen.vert", RHI::ShaderStage::Vertex);
        auto fsInfo = loader.loadFromFile("assets/shaders/deferred/copy.frag", RHI::ShaderStage::Fragment);
        if (!vsInfo || !fsInfo) { LOG_ERROR("[Presentation] shaders failed"); return; }

        // set1：SceneColor 采样
        RHI::DescriptorSetLayoutDesc set1;
        set1.bindings = {{0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment}};
        m_sceneColorLayout = m_resMgr->createDescriptorSetLayout(set1);

        RHI::PipelineLayoutDesc plDesc;
        plDesc.descriptorSetLayouts = { ctx.globalSetLayout, m_sceneColorLayout };
        m_layout = m_resMgr->createPipelineLayout(plDesc);

        GraphicsPipelineState pso;
        pso.vertexShader = vsInfo->module;
        pso.fragmentShader = fsInfo->module;
        pso.layout = m_layout;
        pso.cullMode = RHI::CullMode::None;
        pso.depthTestEnable = false; pso.depthWriteEnable = false;
        pso.topology = RHI::PrimitiveTopology::TriangleList;
        pso.dynamicStates = { RHI::DynamicState::Viewport, RHI::DynamicState::Scissor };
        pso.vertexInput = {};
        RHI::BlendAttachmentState blend; blend.blendEnable = false;
        pso.attachments = { blend };
        m_pipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
            m_resMgr.get(), pso, ctx.renderPass, ctx.subpassIndex);

        m_globalSet = ctx.globalDescSet;

        // SceneColor 描述符集（presentation 阶段采样 SceneColor 输出）
        RHI::SamplerDesc sampDesc;
        sampDesc.minFilter = RHI::SamplerFilter::Linear;
        sampDesc.magFilter = RHI::SamplerFilter::Linear;
        m_sampler = m_resMgr->createSampler(sampDesc);

        auto scId = ctx.renderGraph->getTextureId("SceneColor");
        RHI::TextureHandle phys = ctx.renderGraph->getPhysicalTextureHandle(scId);
        auto* texObj = m_resMgr->getTexture(phys);
        auto* samplerObj = m_resMgr->getSampler(m_sampler);
        if (texObj && samplerObj) {
            RHI::DescriptorPoolDesc poolDesc;
            poolDesc.maxSets = 1;
            poolDesc.poolSizes = {{RHI::DescriptorType::CombinedImageSampler, 1}};
            m_pool = m_resMgr->createDescriptorPool(poolDesc);

            RHI::DescriptorSetDesc setDesc;
            setDesc.descriptorSetLayout = m_sceneColorLayout;
            setDesc.descriptorPool = m_pool;
            m_sceneColorSet = m_resMgr->createDescriptorSet(setDesc);
            if (m_sceneColorSet.isValid()) {
                auto* descSet = m_resMgr->getDescriptorSet(m_sceneColorSet);
                if (descSet) {
                    descSet->writeTexture(0, 0, texObj, samplerObj, RHI::ImageLayout::ShaderReadOnly);
                    descSet->update();
                }
            }
        }
        LOG_INFO("[Presentation] executor ready");
    }

    void PresentationExecutor::execute(RHI::RHICommandEncoder* encoder, const RenderContext&,
                                       const PassContext& pctx, uint32_t) {
        if (!m_pipeline.isValid()) return;
        auto resMgr = pctx.getResourceManager();
        auto* pipeline = resMgr->getPipeline(m_pipeline);
        if (!pipeline) return;
        encoder->bindPipeline(pipeline);
        auto* playout = resMgr->getPipelineLayout(pipeline->getLayout());
        if (playout) {
            if (m_globalSet.isValid())
                encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, playout, 0, {m_globalSet}, {});
            if (m_sceneColorSet.isValid())
                encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, playout, 1, {m_sceneColorSet}, {});
        }
        encoder->draw(3, 1, 0, 0);
    }

} // namespace StarryEngine
