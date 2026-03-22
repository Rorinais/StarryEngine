#pragma once
#include "ISubpassRecorder.hpp"
#include "../../assets/Assets.hpp"

namespace StarryEngine{

    class DeferredLightingRecorder : public ISubpassRecorder {
    public:
        DeferredLightingRecorder() = default;

        void setPipeline(RHI::PipelineHandle pipeline) override { m_pipeline = pipeline; }
        void setLightingMaterial(std::shared_ptr<Assets::MaterialInstance> material) { m_lightingMaterial = material; }
        std::shared_ptr<Assets::MaterialInstance> getMaterial() const override { return m_lightingMaterial; }

        void clearDrawItems() override {}
        const std::vector<std::shared_ptr<Scene::DrawItem>>& getDrawItems() override { return m_empty; }
        void setPipelines(const std::vector<RHI::PipelineHandle>& pipelines) override {}
        void setDrawItems(const std::vector<std::shared_ptr<Scene::DrawItem>>& items) override {}

        void recordCommands(RHI::RHICommandEncoder* encoder,
            const RenderContext& rctx,
            const PassContext& pctx,
            uint32_t subpassIndex) override {
            if (!m_lightingMaterial) return;

            if (!m_pipeline.isValid()) {
                LOG_ERROR("DeferredLightingRecorder: material has no pipeline");
                return;
            }

            auto pipeline = pctx.getResourceManager()->getPipeline(m_pipeline);
            encoder->bindPipeline(pipeline);

            auto pipelineLayout = pctx.getResourceManager()->getPipelineLayout(pipeline->getLayout());
            for (auto& [set, handle] : m_lightingMaterial->getAllSets()) {
                encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics,
                    pipelineLayout, set, { handle }, {});
            }

            encoder->draw(3, 1, 0, 0);
        }

    private:
        RHI::PipelineHandle m_pipeline;
        std::shared_ptr<Assets::MaterialInstance> m_lightingMaterial;
        std::vector<std::shared_ptr<Scene::DrawItem>> m_empty;
    };

    class CopyToSwapchainRecorder : public ISubpassRecorder {
    public:
        void setPipeline(RHI::PipelineHandle pipeline) override { m_pipeline = pipeline; }
        void setMaterial(std::shared_ptr<Assets::MaterialInstance> material) override { m_material = material; }
        std::shared_ptr<Assets::MaterialInstance> getMaterial() const override { return m_material; }

        void clearDrawItems() override {}
        const std::vector<std::shared_ptr<Scene::DrawItem>>& getDrawItems() override { return m_empty; }
        void setPipelines(const std::vector<RHI::PipelineHandle>&) override {}
        void setDrawItems(const std::vector<std::shared_ptr<Scene::DrawItem>>&) override {}

        void recordCommands(RHI::RHICommandEncoder* encoder,
            const RenderContext& rctx,
            const PassContext& pctx,
            uint32_t subpassIndex) override {
            if (!m_pipeline.isValid() || !m_material) return;
            auto pipeline = pctx.getResourceManager()->getPipeline(m_pipeline);
            encoder->bindPipeline(pipeline);

            auto layout = pctx.getResourceManager()->getPipelineLayout(pipeline->getLayout());
            for (auto& [set, handle] : m_material->getAllSets()) {
                encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, layout, set, { handle }, {});
            }

            encoder->draw(3, 1, 0, 0);
        }

    private:
        RHI::PipelineHandle m_pipeline;
        std::shared_ptr<Assets::MaterialInstance> m_material;
        std::vector<std::shared_ptr<Scene::DrawItem>> m_empty;
    };
}