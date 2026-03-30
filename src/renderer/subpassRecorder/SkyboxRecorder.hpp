#pragma once
#include "ISubpassRecorder.hpp"
#include "../../assets/Assets.hpp"

namespace StarryEngine {
    class SkyboxRecorder : public ISubpassRecorder {
    public:
        void clearDrawItems() override { m_drawItems.clear(); }
        void setDrawItems(const std::vector<std::shared_ptr<Scene::DrawItem>>& items) override { m_drawItems = items; }
        const std::vector<std::shared_ptr<Scene::DrawItem>>& getDrawItems() override { return m_drawItems; }

        void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>& mapping) override {
            m_pipelineMapping = mapping;
        }

        void recordCommands(RHI::RHICommandEncoder* encoder,
            const RenderContext& rctx,
            const PassContext& pctx,
            uint32_t subpassIndex) override {

            for (auto& item : m_drawItems) {
                // 过程式绘制
                if (item->type != Scene::DrawItemType::Procedural) continue;

                auto it = m_pipelineMapping.find(item->pipelineIndex);
                if (it == m_pipelineMapping.end()) {
                    LOG_ERROR("No pipeline found for index {}", item->pipelineIndex);
                    continue;
                }
                auto pipeline = pctx.getResourceManager()->getPipeline(it->second);
                if (!pipeline) continue;
                encoder->bindPipeline(pipeline);

                auto pipelineLayout = pctx.getResourceManager()->getPipelineLayout(pipeline->getLayout());
                for (auto& [set, handle] : item->descriptorSets) {
                    encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics,
                        pipelineLayout, set, { handle }, {});
                }

                encoder->draw(item->vertexCount, item->instanceCount, item->firstVertex, item->firstInstance);
            }
        }

    private:
        std::vector<std::shared_ptr<Scene::DrawItem>> m_drawItems;
        std::unordered_map<uint32_t, RHI::PipelineHandle> m_pipelineMapping;
    };

}