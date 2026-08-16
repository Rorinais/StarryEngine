#include <renderer/passExecutor/ParticleRenderExecutor.hpp>
#include <cstring>

namespace StarryEngine {

    ParticleRenderExecutor::ParticleRenderExecutor(
        RHI::PipelineHandle pipeline, RHI::PipelineLayoutHandle layout,
        std::vector<RHI::DescriptorSetHandle> globalSets, RHI::DescriptorSetHandle particleSet, uint32_t count)
        : m_pipeline(pipeline), m_layout(layout),
          m_globalSets(std::move(globalSets)), m_particleSet(particleSet), m_count(count) {}

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
        encoder->bindGraphicPipeline(m_pipeline);
        if (m_layout.isValid()) {
            uint32_t slot = pctx.getFrameSlot();
            RHI::DescriptorSetHandle gset = (slot < m_globalSets.size()) ? m_globalSets[slot] : RHI::DescriptorSetHandle::Null();
            if (gset.isValid())
                encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, m_layout, 0, {gset}, {});
            if (m_particleSet.isValid())
                encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, m_layout, 1, {m_particleSet}, {});
            // model(0..64) + 参数(64..120)，一次 push
            ParticleRenderPC pc;
            pc.model = m_model;
            pc.params = m_vsPC;
            encoder->pushConstants(m_layout, RHI::ShaderStage::Vertex, 0, sizeof(pc), &pc);
        }
        encoder->draw(m_count, 1, 0, 0);
    }

} // namespace StarryEngine
