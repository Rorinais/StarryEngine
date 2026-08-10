#include <renderer/passExecutor/ParticleRenderExecutor.hpp>
#include <cstring>

namespace StarryEngine {

    ParticleRenderExecutor::ParticleRenderExecutor(
        RHI::PipelineHandle pipeline, RHI::PipelineLayoutHandle layout,
        RHI::DescriptorSetHandle globalSet, RHI::DescriptorSetHandle particleSet, uint32_t count)
        : m_pipeline(pipeline), m_layout(layout),
          m_globalSet(globalSet), m_particleSet(particleSet), m_count(count) {}

    void ParticleRenderExecutor::setVSParams(const ParticleParams& p) {
        memcpy(m_vsPC.colorYoung,  p.colorYoung,  sizeof(m_vsPC.colorYoung));
        memcpy(m_vsPC.colorMiddle, p.colorMiddle, sizeof(m_vsPC.colorMiddle));
        memcpy(m_vsPC.colorOld,    p.colorOld,    sizeof(m_vsPC.colorOld));
        m_vsPC.pointSizeMin = p.pointSizeMin;
        m_vsPC.pointSizeMax = p.pointSizeMax;
    }

    void ParticleRenderExecutor::execute(RHI::RHICommandEncoder* encoder, const RenderContext&,
                                         const PassContext& pctx, uint32_t) {
        if (!m_pipeline.isValid()) return;
        auto resMgr = pctx.getResourceManager();
        auto* ppl = resMgr->getPipeline(m_pipeline);
        if (!ppl) return;
        encoder->bindPipeline(ppl);
        auto* plo = resMgr->getPipelineLayout(m_layout);
        if (plo) {
            if (m_globalSet.isValid())
                encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, plo, 0, {m_globalSet}, {});
            if (m_particleSet.isValid())
                encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, plo, 1, {m_particleSet}, {});
            // model(0..64) + 参数(64..120)，一次 push
            ParticleRenderPC pc;
            pc.model = m_model;
            pc.params = m_vsPC;
            encoder->pushConstants(plo, RHI::ShaderStage::Vertex, 0, sizeof(pc), &pc);
        }
        encoder->draw(m_count, 1, 0, 0);
    }

} // namespace StarryEngine
