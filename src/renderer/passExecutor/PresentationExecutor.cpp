#include "PresentationExecutor.hpp"

namespace StarryEngine {

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
