#pragma once
#include"ISubpassRenderer.hpp"
#include"../interface/RHI_RESOURCE_FACTORY.hpp"

namespace StarryEngine::RenderGraph {
    class GBufferRenderer : public ISubpassRenderer {
    public:
        GBufferRenderer(std::shared_ptr<Geometry> geometry,
            std::shared_ptr<Material> material,
            std::shared_ptr<RHI::ResourceManager> resMgr)
            : mGeometry(geometry), mMaterial(material), mResMgr(resMgr) {
        }

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

    private:
        std::shared_ptr<Geometry> mGeometry;
        std::shared_ptr<Material> mMaterial;
        std::shared_ptr<RHI::ResourceManager> mResMgr;
    };

    class PostProcessRenderer : public ISubpassRenderer {
    public:
        PostProcessRenderer(RHI::ShaderHandle vs, RHI::ShaderHandle fs,
            RHI::PipelineLayoutHandle layout,
            RHI::DescriptorSetHandle descSet,
            std::shared_ptr<RHI::ResourceManager> resMgr)
            : m_vs(vs), m_fs(fs), m_layout(layout), m_descSet(descSet), m_resMgr(resMgr) {
        }

        void recordCommands(RHI::RHICommandEncoder* encoder,
            const PassContext& pctx,
            uint32_t subpassIndex,
            uint32_t frameIndex) override {
            auto pipeline = pctx.getPipeline(subpassIndex);
            encoder->bindPipeline(m_resMgr->getPipeline(pipeline));

            encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics,
                m_resMgr->getPipelineLayout(m_layout),
                0, // setIndex 固定为 0（因为只有一个描述符集）
                { m_descSet }, {});

            encoder->draw(3, 1, 0, 0); // 全屏三角形
        }

    private:
        RHI::ShaderHandle m_vs, m_fs;
        RHI::PipelineLayoutHandle m_layout;
        RHI::DescriptorSetHandle m_descSet;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
    };
}
