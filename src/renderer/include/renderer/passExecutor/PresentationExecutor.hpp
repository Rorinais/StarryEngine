#pragma once
#include <renderer/passExecutor/IPassExecutor.hpp>
#include <scene/Scene.hpp>
#include <renderer/RenderTypes.hpp>

namespace StarryEngine {

    // 全屏 blit：fullscreen.vert + copy.frag → SceneColor → Swapchain
    // 自包含 executor：在 onPrepare 里构建自己的管线 + SceneColor 描述符集，
    // 不依赖 render path 注入（原 BaseRenderPath::preparePresentationPipeline 已收进这里）
    class PresentationExecutor : public IPassExecutor {
    public:
        PresentationExecutor() = default;
        ~PresentationExecutor() override;

        void clearDrawItems() override {}
        void setDrawItems(const std::vector<std::shared_ptr<DrawItem>>&) override {}
        const std::vector<std::shared_ptr<DrawItem>>& getDrawItems() override { return m_empty; }
        void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>&) override {}
        void addDrawItem(std::shared_ptr<DrawItem>) override {}

        void onPrepare(const ExecutorPrepareContext& ctx) override;

        void execute(RHI::RHICommandEncoder* encoder, const RenderContext&,
                     const PassContext& pctx, uint32_t) override;

    private:
        RHI::PipelineHandle m_pipeline;
        RHI::PipelineLayoutHandle m_layout;
        std::vector<RHI::DescriptorSetHandle> m_globalSets;   // per-slot（ADR-6）
        RHI::DescriptorSetHandle m_sceneColorSet;
        RHI::DescriptorSetLayoutHandle m_sceneColorLayout;
        RHI::DescriptorPoolHandle m_pool;
        RHI::SamplerHandle m_sampler;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::vector<std::shared_ptr<DrawItem>> m_empty;
    };

} // namespace StarryEngine
