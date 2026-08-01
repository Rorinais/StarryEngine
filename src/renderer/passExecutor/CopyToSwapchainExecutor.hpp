#pragma once
#include "IPassExecutor.hpp"
#include "../../assets/Assets.hpp"
#include "../RenderTypes.hpp"

namespace StarryEngine {

    class CopyToSwapchainExecutor : public IPassExecutor {
    public:
        void clearDrawItems() override { m_drawItems.clear(); }
        void setDrawItems(const std::vector<std::shared_ptr<DrawItem>>& items) override { m_drawItems = items; }
        const std::vector<std::shared_ptr<DrawItem>>& getDrawItems() override { return m_drawItems; }
        void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>& mapping) override {
            m_pipelineMapping = mapping;
        }
        void addDrawItem(std::shared_ptr<DrawItem> item) override { m_drawItems.push_back(item); }
        void execute(RHI::RHICommandEncoder* encoder, const RenderContext& rctx,
                     const PassContext& pctx, uint32_t subpassIndex) override {
            for (auto& item : m_drawItems) {
                if (item->type != DrawItemType::Procedural) continue;
                auto it = m_pipelineMapping.find(item->pipelineIndex);
                if (it == m_pipelineMapping.end()) { LOG_ERROR("No pipeline for idx {}", item->pipelineIndex); continue; }
                auto pipeline = pctx.getResourceManager()->getPipeline(it->second);
                if (!pipeline) continue;
                encoder->bindPipeline(pipeline);
                auto layout = pctx.getResourceManager()->getPipelineLayout(pipeline->getLayout());
                for (auto& [set, handle] : item->descriptorSets)
                    encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, layout, set, {handle}, {});
                encoder->draw(item->vertexCount, item->instanceCount, item->firstVertex, item->firstInstance);
            }
        }
    private:
        std::vector<std::shared_ptr<DrawItem>> m_drawItems;
        std::unordered_map<uint32_t, RHI::PipelineHandle> m_pipelineMapping;
    };

} // namespace StarryEngine
