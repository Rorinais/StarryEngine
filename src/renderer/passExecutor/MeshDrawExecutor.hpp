// MeshDrawExecutor.hpp
#pragma once
#include "IPassExecutor.hpp"
#include "../graph/PassNode.hpp"
#include "../backend/vulkan/VulkanRHI.hpp"
#include "../../scene/Scene.hpp"
#include "../../core/base.hpp"

namespace StarryEngine {
    class MeshDrawExecutor : public IPassExecutor {
    public:
        MeshDrawExecutor() = default;

        void clearDrawItems() override { m_drawItems.clear(); }
        const std::vector<std::shared_ptr<Scene::DrawItem>>& getDrawItems() override { return m_drawItems; }
        void setDrawItems(const std::vector<std::shared_ptr<Scene::DrawItem>>& items) override { m_drawItems = items; }

        // 新的管线映射设置
        void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>& mapping) override {
            m_pipelineMapping = mapping;
        }

        void addDrawItem(std::shared_ptr<Scene::DrawItem> item) override {
            m_drawItems.push_back(item);
        }

        void execute(RHI::RHICommandEncoder* encoder,const RenderContext& rctx,const PassContext& pctx,uint32_t subpassIndex) override {
            for (const auto& item : m_drawItems) {
                // 处理过程式绘制
                if (item->type == Scene::DrawItemType::Procedural) {
                    auto it = m_pipelineMapping.find(item->pipelineIndex);
                    if (it == m_pipelineMapping.end()) {
                        LOG_ERROR("No pipeline found for index {}", item->pipelineIndex);
                    }
                    auto pipeline = pctx.getResourceManager()->getPipeline(it->second);
                    encoder->bindPipeline(pipeline);

                    auto layout = pctx.getResourceManager()->getPipelineLayout(pipeline->getLayout());
                    for (auto& [set, handle] : item->descriptorSets) {
                        encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, layout, set, { handle }, {});
                    }
                    encoder->draw(item->vertexCount, item->instanceCount, item->firstVertex, item->firstInstance);
                }

                auto obj = item->object.lock();
                if (!obj) LOG_WARN("DrawItem object expired");

                auto it = m_pipelineMapping.find(item->pipelineIndex);
                if (it == m_pipelineMapping.end()) {
                    LOG_ERROR("No pipeline found for index {}", item->pipelineIndex);
                }
                auto pipeline = pctx.getResourceManager()->getPipeline(it->second);
                if (!pipeline) {
                    LOG_ERROR("Failed to get pipeline from handle");
                }
                encoder->bindPipeline(pipeline);

                auto pipelineLayout = pctx.getResourceManager()->getPipelineLayout(pipeline->getLayout());
                for (const auto& [setIndex, setHandle] : item->descriptorSets) {
                    encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics,pipelineLayout, setIndex, { setHandle }, {});
                }

                // 非实例化时 push 变换矩阵
                if (!item->isInstanced) {
                    encoder->pushConstants(pipelineLayout, RHI::ShaderStage::Vertex,0, sizeof(glm::mat4), &obj->transform);
                }

                // 绑定顶点/索引缓冲
                std::vector<RHI::RHIBuffer*> vertexBuffers;
                std::vector<uint64_t> offsets;
                vertexBuffers.push_back(pctx.getResourceManager()->getBuffer(item->vertexBuffer));
                offsets.push_back(0);
                if (item->isInstanced && item->instanceBuffer.isValid()) {
                    vertexBuffers.push_back(pctx.getResourceManager()->getBuffer(item->instanceBuffer));
                    offsets.push_back(0);
                }
                encoder->bindVertexBuffers(0, vertexBuffers, offsets);
                encoder->bindIndexBuffer(pctx.getResourceManager()->getBuffer(item->indexBuffer), 0, RHI::IndexType::UInt32);

                if (item->isInstanced) {
                    encoder->drawIndexed(item->indexCount, item->instanceCount, item->indexOffset, 0, 0);
                }else {
                    encoder->drawIndexed(item->indexCount, 1, item->indexOffset, 0, 0);
                }
            }
        }

    private:
        std::vector<std::shared_ptr<Scene::DrawItem>> m_drawItems;
        std::unordered_map<uint32_t, RHI::PipelineHandle> m_pipelineMapping; // pipelineIndex -> 管线句柄
    };
}