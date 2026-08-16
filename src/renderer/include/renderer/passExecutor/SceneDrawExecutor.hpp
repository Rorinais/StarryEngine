#pragma once
#include <renderer/passExecutor/IPassExecutor.hpp>
#include <renderer/graph/PassNode.hpp>
#include <renderer/interface/vulkan/VulkanRHI.hpp>
#include <scene/Scene.hpp>
#include <renderer/RenderTypes.hpp>
#include <core/base.hpp>

namespace StarryEngine {
    class SceneDrawExecutor : public IPassExecutor {
    public:
        SceneDrawExecutor() = default;

        void clearDrawItems() override { m_drawItems.clear(); }
        const std::vector<std::shared_ptr<DrawItem>>& getDrawItems() override { return m_drawItems; }
        void setDrawItems(const std::vector<std::shared_ptr<DrawItem>>& items) override { m_drawItems = items; }

        void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>& mapping) override {
            m_pipelineMapping = mapping;
        }

        void addDrawItem(std::shared_ptr<DrawItem> item) override {
            m_drawItems.push_back(item);
        }

        void execute(RHI::RHICommandEncoder* encoder,const RenderContext& rctx,const PassContext& pctx,uint32_t subpassIndex) override {
            auto resMgr = pctx.getResourceManager();
            uint32_t slot = pctx.getFrameSlot();
            if (slot >= RHI::kMaxFramesInFlight) slot = 0;
            for (const auto& item : m_drawItems) {
                auto it = m_pipelineMapping.find(item->pipelineIndex);
                if (it == m_pipelineMapping.end()) {
                    LOG_ERROR("No pipeline found for index {}", item->pipelineIndex);
                    continue;
                }
                auto* pipeline = resMgr->getPipeline(it->second);
                if (!pipeline) { LOG_ERROR("Failed to get pipeline from handle"); continue; }
                encoder->bindGraphicPipeline(it->second);

                RHI::PipelineLayoutHandle pipelineLayout = pipeline->getLayout();
                if (!pipelineLayout.isValid()) { LOG_ERROR("Failed to get pipeline layout"); continue; }
                const auto& slotSets = item->descriptorSets[slot < RHI::kMaxFramesInFlight ? slot : 0];
                for (const auto& [setIndex, setHandle] : slotSets) {
                    encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, pipelineLayout, setIndex, { setHandle }, {});
                }

                if (item->type == DrawItemType::Procedural) {
                    encoder->draw(item->vertexCount, item->instanceCount, item->firstVertex, item->firstInstance);
                    continue;
                }

                auto obj = item->object.lock();
                if (!obj) { LOG_WARN("DrawItem object expired"); continue; }

                if (!item->isInstanced) {
                    encoder->pushConstants(pipelineLayout, RHI::ShaderStage::Vertex, 0, sizeof(glm::mat4), &obj->transform);
                }

                std::vector<RHI::BufferHandle> vertexBuffers;
                std::vector<uint64_t> offsets;
                vertexBuffers.push_back(item->vertexBuffer);
                offsets.push_back(0);
                if (item->isInstanced && item->instanceBuffer[slot].isValid()) {
                    vertexBuffers.push_back(item->instanceBuffer[slot]);
                    offsets.push_back(0);
                }
                encoder->bindVertexBuffers(0, vertexBuffers, offsets);
                encoder->bindIndexBuffer(item->indexBuffer, 0, RHI::IndexType::UInt32);

                if (item->isInstanced) {
                    encoder->drawIndexed(item->indexCount, item->instanceCount, item->indexOffset, 0, 0);
                } else {
                    encoder->drawIndexed(item->indexCount, 1, item->indexOffset, 0, 0);
                }
            }
        }

    private:
        std::vector<std::shared_ptr<DrawItem>> m_drawItems;
        std::unordered_map<uint32_t, RHI::PipelineHandle> m_pipelineMapping; // pipelineIndex -> 管线句柄
    };
}