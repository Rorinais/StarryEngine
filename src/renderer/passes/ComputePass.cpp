#include "../passExecutor/ComputeExecutor.hpp"
#include "ComputePass.hpp"
#include "../../assets/loader/ShaderLoader.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine {

    // ── 声明：建 compute 节点 + 读/写资源 + dispatch ──
    bool ComputePass::configure(RenderGraph::RenderGraph& graph,
                                std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap,
                                uint32_t /*width*/, uint32_t /*height*/,
                                std::shared_ptr<RHI::ResourceManager> /*resMgr*/,
                                RHI::DescriptorSetLayoutHandle /*globalSetLayout*/) {
        m_passNode = graph.addComputePassNode(m_desc.name);

        for (auto& r : m_desc.resources) {
            switch (r.type) {
            case RHI::DescriptorType::StorageBuffer: {
                auto id = graph.getBufferId(r.resourceName);
                if (r.write) m_passNode->addWriteBuffer(id);
                else m_passNode->addReadBuffer(id);
                break;
            }
            case RHI::DescriptorType::StorageImage: {
                auto id = graph.getTextureId(r.resourceName);
                if (r.write) m_passNode->addWriteTexture(id);
                else m_passNode->addReadTexture(id);
                break;
            }
            case RHI::DescriptorType::CombinedImageSampler: {
                auto id = graph.getTextureId(r.resourceName);
                m_passNode->addReadTexture(id);
                break;
            }
            default:
                LOG_WARN("[{}] unsupported resource type {}", m_desc.name, static_cast<int>(r.type));
                break;
            }
        }
        return true;
    }

    // ── 管线：shader → 布局 → 描述符 → executor ──
    void ComputePass::onAfterCompile(const CompileContext& ctx) {
        auto resMgr = ctx.resMgr;
        if (!resMgr || !m_passNode || !ctx.renderGraph) return;

        Assets::ShaderLoader loader(resMgr);
        auto csInfo = loader.loadFromFile(m_desc.shader, RHI::ShaderStage::Compute);
        if (!csInfo || !csInfo->module.isValid()) {
            LOG_ERROR("[{}] compute shader failed: {}", m_desc.name, m_desc.shader);
            return;
        }
        m_shader = csInfo->module;

        // set 0 布局：合并所有资源的 binding
        RHI::DescriptorSetLayoutDesc setDesc;
        for (auto& r : m_desc.resources)
            setDesc.bindings.push_back({ r.binding, r.type, 1, RHI::ShaderStage::Compute });
        m_descLayout = resMgr->createDescriptorSetLayout(setDesc);

        RHI::PipelineLayoutDesc plDesc;
        plDesc.descriptorSetLayouts = { m_descLayout };
        if (m_desc.pushConstantSize > 0)
            plDesc.pushConstants = { { RHI::ShaderStage::Compute, 0, m_desc.pushConstantSize } };
        m_pipelineLayout = resMgr->createPipelineLayout(plDesc);

        RHI::ComputePipelineDesc cpDesc;
        cpDesc.computeShader = m_shader;
        cpDesc.pipelineLayoutHandle = m_pipelineLayout;
        m_pipeline = resMgr->createComputePipeline(cpDesc);

        // 描述符池 + set：绑定声明的资源
        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 1;
        for (auto& r : m_desc.resources)
            poolDesc.poolSizes.push_back({ r.type, 1 });
        m_pool = resMgr->createDescriptorPool(poolDesc);

        RHI::DescriptorSetDesc dsDesc;
        dsDesc.descriptorSetLayout = m_descLayout;
        dsDesc.descriptorPool = m_pool;
        m_descSet = resMgr->createDescriptorSet(dsDesc);
        if (m_descSet.isValid()) {
            auto* ds = resMgr->getDescriptorSet(m_descSet);
            for (auto& r : m_desc.resources) {
                switch (r.type) {
                case RHI::DescriptorType::StorageBuffer: {
                    auto phys = ctx.renderGraph->getPhysicalBuffer(ctx.renderGraph->getBufferId(r.resourceName));
                    ds->writeBuffer(r.binding, 0, resMgr->getBuffer(phys), 0, r.bufferSize);
                    break;
                }
                case RHI::DescriptorType::StorageImage: {
                    auto phys = ctx.renderGraph->getPhysicalTextureHandle(ctx.renderGraph->getTextureId(r.resourceName));
                    ds->writeTexture(r.binding, 0, resMgr->getTexture(phys), nullptr, RHI::ImageLayout::General);
                    break;
                }
                case RHI::DescriptorType::CombinedImageSampler: {
                    auto phys = ctx.renderGraph->getPhysicalTextureHandle(ctx.renderGraph->getTextureId(r.resourceName));
                    if (!m_sampler.isValid()) {
                        RHI::SamplerDesc sd;
                        sd.minFilter = RHI::SamplerFilter::Linear;
                        sd.magFilter = RHI::SamplerFilter::Linear;
                        m_sampler = resMgr->createSampler(sd);
                    }
                    ds->writeTexture(r.binding, 0, resMgr->getTexture(phys), resMgr->getSampler(m_sampler),
                                     RHI::ImageLayout::ShaderReadOnly);
                    break;
                }
                default:
                    break;
                }
            }
            ds->update();
        }

        m_passNode->setComputeExecutor(std::make_shared<ComputeExecutor>(
            m_pipeline, m_pipelineLayout, m_descSet,
            m_desc.dispatchX, m_desc.dispatchY, m_desc.dispatchZ,
            m_desc.pushConstantSize, m_desc.fillPushConstants));

        LOG_INFO("[{}] ComputePass ready", m_desc.name);
    }

} // namespace StarryEngine
