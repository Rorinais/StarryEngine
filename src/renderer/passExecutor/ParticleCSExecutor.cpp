#include "ParticleCSExecutor.hpp"

namespace StarryEngine {

    void ParticleCSExecutor::execute(RHI::RHICommandEncoder* encoder, const RenderContext& rctx,
                                     const PassContext& pctx, uint32_t) {
        auto resMgr = pctx.getResourceManager();
        auto* plo = resMgr->getPipelineLayout(m_layout);
        if (plo && m_descSet.isValid())
            encoder->bindDescriptorSets(RHI::PipelineBindPoint::Compute, plo, 0, {m_descSet}, {});
        float dt = rctx.deltaTime > 0.0f ? rctx.deltaTime : 0.016f;
        uint32_t n = m_particleCount;
        struct CS_PC { float dt; uint32_t n; float g, smin, smax, life, sxz, sf, sa, ey, td, tt; } pc;
        pc = {dt, n, m_params.gravity, m_params.speedMin, m_params.speedMax,
              m_params.lifetime, m_params.spreadXZ, m_params.swayFreq, m_params.swayAmp,
              m_params.emitterY, m_params.topDiffuse, m_params.topThreshold};
        encoder->pushConstants(plo, RHI::ShaderStage::Compute, 0, sizeof(pc), &pc);
    }

} // namespace StarryEngine
