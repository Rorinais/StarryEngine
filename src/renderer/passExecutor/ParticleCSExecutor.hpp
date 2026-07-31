#pragma once
#include "IPassExecutor.hpp"
#include "ParticleParams.hpp"
#include "../../scene/Scene.hpp"

namespace StarryEngine {

    // 每帧 Dispatch 粒子 Compute Shader，推 deltaTime + 全部粒子参数
    class ParticleCSExecutor : public IPassExecutor {
    public:
        ParticleCSExecutor(RHI::PipelineLayoutHandle layout, RHI::DescriptorSetHandle descSet,
                           uint32_t particleCount, const ParticleParams& params)
            : m_layout(layout), m_descSet(descSet),
              m_particleCount(particleCount), m_params(params) {}

        void clearDrawItems() override {}
        void setDrawItems(const std::vector<std::shared_ptr<Scene::DrawItem>>&) override {}
        const std::vector<std::shared_ptr<Scene::DrawItem>>& getDrawItems() override { return m_empty; }
        void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>&) override {}
        void addDrawItem(std::shared_ptr<Scene::DrawItem>) override {}

        void execute(RHI::RHICommandEncoder* encoder, const RenderContext& rctx,
                     const PassContext& pctx, uint32_t) override;

        // 设置存储的 layout — 仅用于 pushConstants，descriptor binding 走原生 layout

    private:
        RHI::PipelineLayoutHandle m_layout;
        RHI::DescriptorSetHandle m_descSet;
        uint32_t m_particleCount;
        ParticleParams m_params;
        std::vector<std::shared_ptr<Scene::DrawItem>> m_empty;
    };

} // namespace StarryEngine
