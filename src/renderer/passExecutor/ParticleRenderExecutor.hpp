#pragma once
#include "IPassExecutor.hpp"
#include "../../scene/ParticleParams.hpp"
#include "../../scene/Scene.hpp"
#include "../RenderTypes.hpp"

namespace StarryEngine {

    struct ParticleVSPC {
        float colorYoung[4], colorMiddle[4], colorOld[4];
        float pointSizeMin, pointSizeMax;
    };

    // 渲染 push constant（与 particle.vert 一致）：model(0..64) + 参数(64..120)
    struct ParticleRenderPC {
        glm::mat4 model;
        ParticleVSPC params;
    };

    class ParticleRenderExecutor : public IPassExecutor {
    public:
        ParticleRenderExecutor(RHI::PipelineHandle pipeline, RHI::PipelineLayoutHandle layout,
                               RHI::DescriptorSetHandle globalSet, RHI::DescriptorSetHandle particleSet,
                               uint32_t count);

        void setParticleSet(RHI::DescriptorSetHandle s) { m_particleSet = s; }

        void clearDrawItems() override {}
        void setDrawItems(const std::vector<std::shared_ptr<DrawItem>>&) override {}
        const std::vector<std::shared_ptr<DrawItem>>& getDrawItems() override { return m_empty; }
        void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>&) override {}
        void addDrawItem(std::shared_ptr<DrawItem>) override {}

        void execute(RHI::RHICommandEncoder* encoder, const RenderContext&,
                     const PassContext& pctx, uint32_t) override;

        void setVSParams(const ParticleParams& p);
        // 发射器局部坐标 → 世界（push constant model）
        void setTransform(const glm::mat4& t) { m_model = t; }

    private:
        RHI::PipelineHandle m_pipeline;
        RHI::PipelineLayoutHandle m_layout;
        RHI::DescriptorSetHandle m_globalSet, m_particleSet;
        uint32_t m_count;
        glm::mat4 m_model = glm::mat4(1.0f);
        ParticleVSPC m_vsPC = {};
        std::vector<std::shared_ptr<DrawItem>> m_empty;
    };

} // namespace StarryEngine
