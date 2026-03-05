#pragma once
#include"ISubpassRenderer.hpp"
#include "../graph/PassNode.hpp"

namespace StarryEngine::RenderGraph {
    class GBufferRenderer : public ISubpassRenderer {
    public:
        using ISubpassRenderer::ISubpassRenderer;
        void recordCommands(RHI::RHICommandEncoder* encoder,
            const PassContext& pctx,
            uint32_t subpassIndex,
            uint32_t frameIndex) {

            // 1. 获取当前 Subpass 的 Pipeline
            auto pipeline = pctx.getPipeline(subpassIndex);
            if (!pipeline.isValid()) return;
            encoder->bindPipeline(mResMgr->getPipeline(pipeline));

            // 2. 绑定材质的描述符集
            auto descSet = mMaterial->getDescriptorSet();
            auto pipelineLayoutHandle = mMaterial->getPipelineLayout();
            auto* pipelineLayout = mResMgr->getPipelineLayout(pipelineLayoutHandle);
            uint32_t setIndex = mMaterial->getSetIndex();

            encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics,
                pipelineLayout,
                setIndex,
                { descSet },
                {});

            // 3. 绑定所有顶点缓冲区（每个 binding 单独绑定）
            auto bindings = mGeometry->getBindings();
            for (uint32_t binding : bindings) {
                auto vbHandle = mGeometry->getVertexBufferHandle(binding);
                if (!vbHandle.isValid()) {
                    std::cerr << "[GBufferRenderer] Missing vertex buffer for binding " << binding << std::endl;
                    continue;
                }
                encoder->bindVertexBuffers(binding,
                    { mResMgr->getBuffer(vbHandle) },
                    { 0 });
            }

            // 4. 绑定索引缓冲区
            auto ibHandle = mGeometry->getIndexBufferHandle();
            if (!ibHandle.isValid()) {
                std::cerr << "[GBufferRenderer] Missing index buffer" << std::endl;
                return;
            }
            encoder->bindIndexBuffer(mResMgr->getBuffer(ibHandle),
                0,
                RHI::IndexType::UInt32);

            // 5. 绘制
            encoder->drawIndexed(mGeometry->getIndexCount(), 1, 0, 0, 0);
        }
        
    };

    class PostProcessRenderer : public ISubpassRenderer {
    public:
        using ISubpassRenderer::ISubpassRenderer;

        void recordCommands(RHI::RHICommandEncoder* encoder,
            const PassContext& pctx,
            uint32_t subpassIndex,
            uint32_t frameIndex) override {
            // 绘制全屏三角形
            auto pipeline = pctx.getPipeline(subpassIndex);
            if (!pipeline.isValid()) return;
            encoder->bindPipeline(mResMgr->getPipeline(pipeline));

            auto descSet = mMaterial->getDescriptorSet();
            auto pipelineLayoutHandle = mMaterial->getPipelineLayout();
            auto* pipelineLayout = mResMgr->getPipelineLayout(pipelineLayoutHandle);
            encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics,
                pipelineLayout,
                mMaterial->getSetIndex(),
                { descSet }, {});

            encoder->draw(3, 1, 0, 0);
        }

        void updateInputAttachment(uint32_t binding, RHI::TextureHandle texture, RHI::ImageLayout layout = RHI::ImageLayout::ShaderReadOnly) {
            mMaterial->updateInputAttachment(binding, texture, layout);
        }
    };

    class GridRenderer : public ISubpassRenderer {
    public:
        using ISubpassRenderer::ISubpassRenderer;

        void recordCommands(RHI::RHICommandEncoder* encoder,
            const PassContext& pctx,
            uint32_t subpassIndex,
            uint32_t frameIndex) override {
            auto pipeline = pctx.getPipeline(subpassIndex);
            if (!pipeline.isValid()) return;
            encoder->bindPipeline(mResMgr->getPipeline(pipeline));

            // 绑定描述符集（如果有 UniformBuffer）
            auto descSet = mMaterial->getDescriptorSet();
            auto pipelineLayoutHandle = mMaterial->getPipelineLayout();
            auto* pipelineLayout = mResMgr->getPipelineLayout(pipelineLayoutHandle);
            encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics,
                pipelineLayout,
                mMaterial->getSetIndex(),
                { descSet }, {});

            // 绑定顶点缓冲区
            auto bindings = mGeometry->getBindings();
            for (uint32_t binding : bindings) {
                auto vbHandle = mGeometry->getVertexBufferHandle(binding);
                if (!vbHandle.isValid()) continue;
                encoder->bindVertexBuffers(binding,
                    { mResMgr->getBuffer(vbHandle) },
                    { 0 });
            }

            // 绑定索引缓冲区
            auto ibHandle = mGeometry->getIndexBufferHandle();
            if (ibHandle.isValid()) {
                encoder->bindIndexBuffer(mResMgr->getBuffer(ibHandle),
                    0,
                    RHI::IndexType::UInt32);
                encoder->drawIndexed(mGeometry->getIndexCount(), 1, 0, 0, 0);
            }
            else {
                // 如果没有索引缓冲区，直接绘制顶点数量（假设顶点数据为线列表）
                encoder->draw(mGeometry->getVertexCount(), 1, 0, 0);
            }
        }
    };
}
