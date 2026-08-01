#pragma once
#include "IPassExecutor.hpp"
#include "../../scene/Scene.hpp"
#include "../RenderTypes.hpp"

namespace StarryEngine {

    // 全屏 blit：fullscreen.vert + copy.frag → SceneColor → Swapchain
    class PresentationExecutor : public IPassExecutor {
    public:
        PresentationExecutor(RHI::PipelineHandle pipeline = {},
                             RHI::PipelineLayoutHandle layout = {},
                             RHI::DescriptorSetHandle globalSet = {},
                             RHI::DescriptorSetHandle sceneColorSet = {})
            : m_pipeline(pipeline), m_layout(layout),
              m_globalSet(globalSet), m_sceneColorSet(sceneColorSet) {}

        void setPipeline(RHI::PipelineHandle p) { m_pipeline = p; }
        void setLayout(RHI::PipelineLayoutHandle l) { m_layout = l; }
        void setSceneColorSet(RHI::DescriptorSetHandle s) { m_sceneColorSet = s; }

        void clearDrawItems() override {}
        void setDrawItems(const std::vector<std::shared_ptr<DrawItem>>&) override {}
        const std::vector<std::shared_ptr<DrawItem>>& getDrawItems() override { return m_empty; }
        void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>&) override {}
        void addDrawItem(std::shared_ptr<DrawItem>) override {}

        void execute(RHI::RHICommandEncoder* encoder, const RenderContext&,
                     const PassContext& pctx, uint32_t) override;

    private:
        RHI::PipelineHandle m_pipeline;
        RHI::PipelineLayoutHandle m_layout;
        RHI::DescriptorSetHandle m_globalSet, m_sceneColorSet;
        std::vector<std::shared_ptr<DrawItem>> m_empty;
    };

} // namespace StarryEngine
