#pragma once
#include "ISubpassRecorder.hpp"
#include "../graph/PassNode.hpp"
#include "../backend/vulkan/VulkanRHI.hpp"
#include "../../core/base.hpp"


namespace StarryEngine{
    class MeshDrawRecorder : public ISubpassRecorder {
    public:
        MeshDrawRecorder() = default;

        void clearDrawItems() override { m_drawItems.clear(); }

        const std::vector<std::shared_ptr<Scene::DrawItem>>& getDrawItems() override { return m_drawItems; }

        void setPipelines(const std::vector<RHI::PipelineHandle>& pipelines) override { m_pipelines = pipelines; }
        void setDrawItems(const std::vector<std::shared_ptr<Scene::DrawItem>>& items) override { m_drawItems = items; }

        void recordCommands(RHI::RHICommandEncoder* encoder,
            const RenderContext& rctx,
            const PassContext& pctx,
            uint32_t subpassIndex) override {

            for (const auto& item : m_drawItems) {
                if (item->pipelineIndex >= m_pipelines.size()) continue;

                glm::mat4 transform;
                auto obj = item->object.lock();
                if (!obj) continue; 
                transform = obj->transform;

                auto pipeline = pctx.getResourceManager()->getPipeline(m_pipelines[item->pipelineIndex]);
                encoder->bindPipeline(pipeline);

                auto pipelineLayout = pctx.getResourceManager()->getPipelineLayout(pipeline->getLayout());
                for (const auto& [setIndex, setHandle] : item->descriptorSets) {
                    encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics,pipelineLayout, setIndex, { setHandle }, {});
                }

                if (!item->isInstanced) {
                    glm::mat4 transform;
                    auto obj = item->object.lock();
                    if (!obj) continue;
                    transform = obj->transform;
                    encoder->pushConstants(pipelineLayout, RHI::ShaderStage::Vertex,0, sizeof(glm::mat4), &transform);
                }

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
                    // 实例化绘制
                    encoder->drawIndexed(item->indexCount, item->instanceCount,item->indexOffset, 0, 0);
                }
                else {
                    // 普通绘制
                    encoder->drawIndexed(item->indexCount, 1,item->indexOffset, 0, 0);
                }
            }
        }

    private:
        std::vector<std::shared_ptr<Scene::DrawItem>> m_drawItems;
        std::vector<RHI::PipelineHandle> m_pipelines;
    };

}
