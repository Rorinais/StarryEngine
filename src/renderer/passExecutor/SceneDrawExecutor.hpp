// SceneDrawExecutor.hpp
#pragma once
#include "IPassExecutor.hpp"
#include "../graph/PassNode.hpp"
#include "../backend/vulkan/VulkanRHI.hpp"
#include "../../scene/Scene.hpp"
#include "../RenderTypes.hpp"
#include "../../core/base.hpp"

namespace StarryEngine {
    class SceneDrawExecutor : public IPassExecutor {
    public:
        SceneDrawExecutor() = default;

        void clearDrawItems() override { m_drawItems.clear(); }
        const std::vector<std::shared_ptr<DrawItem>>& getDrawItems() override { return m_drawItems; }
        void setDrawItems(const std::vector<std::shared_ptr<DrawItem>>& items) override { m_drawItems = items; }

        // 新的管线映射设置
        void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>& mapping) override {
            m_pipelineMapping = mapping;
        }

        void addDrawItem(std::shared_ptr<DrawItem> item) override {
            m_drawItems.push_back(item);
        }

        // 统一 mesh + 过程式（天空盒）绘制。procedural 分支必须 continue，否则会穿透到
        // mesh 分支给不存在的顶点/索引缓冲绑定（旧 SkyboxExecutor 已合并进这里）
        void execute(RHI::RHICommandEncoder* encoder,const RenderContext& rctx,const PassContext& pctx,uint32_t subpassIndex) override {
            auto resMgr = pctx.getResourceManager();
            for (const auto& item : m_drawItems) {
                auto it = m_pipelineMapping.find(item->pipelineIndex);
                if (it == m_pipelineMapping.end()) {
                    LOG_ERROR("No pipeline found for index {}", item->pipelineIndex);
                    continue;
                }
                auto* pipeline = resMgr->getPipeline(it->second);
                if (!pipeline) { LOG_ERROR("Failed to get pipeline from handle"); continue; }
                encoder->bindPipeline(pipeline);

                auto* pipelineLayout = resMgr->getPipelineLayout(pipeline->getLayout());
                for (const auto& [setIndex, setHandle] : item->descriptorSets) {
                    encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, pipelineLayout, setIndex, { setHandle }, {});
                }

                // 过程式绘制（天空盒/全屏三角形）：纯 shader 生成顶点，无顶点/索引缓冲、无变换
                if (item->type == DrawItemType::Procedural) {
                    encoder->draw(item->vertexCount, item->instanceCount, item->firstVertex, item->firstInstance);
                    continue;
                }

                // 网格绘制
                auto obj = item->object.lock();
                if (!obj) { LOG_WARN("DrawItem object expired"); continue; }

                // 非实例化时 push 变换矩阵
                if (!item->isInstanced) {
                    encoder->pushConstants(pipelineLayout, RHI::ShaderStage::Vertex, 0, sizeof(glm::mat4), &obj->transform);
                }

                std::vector<RHI::RHIBuffer*> vertexBuffers;
                std::vector<uint64_t> offsets;
                vertexBuffers.push_back(resMgr->getBuffer(item->vertexBuffer));
                offsets.push_back(0);
                if (item->isInstanced && item->instanceBuffer.isValid()) {
                    vertexBuffers.push_back(resMgr->getBuffer(item->instanceBuffer));
                    offsets.push_back(0);
                }
                encoder->bindVertexBuffers(0, vertexBuffers, offsets);
                encoder->bindIndexBuffer(resMgr->getBuffer(item->indexBuffer), 0, RHI::IndexType::UInt32);

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