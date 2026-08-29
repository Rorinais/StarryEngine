#pragma once
#include <renderer/passExecutor/IPassExecutor.hpp>
#include <scene/Scene.hpp>
#include <renderer/RenderTypes.hpp>

namespace StarryEngine {

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

        // 呈现输入纹理名（默认 SceneColor；软光追+降噪时切 SceneColorFiltered）
        void setInputTextureName(const std::string& name) { m_inputTextureName = name; }

    private:
        RHI::PipelineHandle m_pipeline;
        RHI::PipelineLayoutHandle m_layout;
        std::vector<RHI::DescriptorSetHandle> m_globalSets;   
        RHI::DescriptorSetHandle m_sceneColorSet;
        RHI::DescriptorSetLayoutHandle m_sceneColorLayout;
        RHI::DescriptorPoolHandle m_pool;
        RHI::SamplerHandle m_sampler;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::string m_inputTextureName = "SceneColor";
        std::vector<std::shared_ptr<DrawItem>> m_empty;
    };

} // namespace StarryEngine
