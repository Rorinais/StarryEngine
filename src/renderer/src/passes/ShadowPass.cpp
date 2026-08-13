#include <renderer/passes/ShadowPass.hpp>
#include <assets/material/MaterialTemplate.hpp>
#include <logging/Logger.hpp>
#include <unordered_set>

namespace StarryEngine {

    bool ShadowPass::configure(RenderGraph::RenderGraph& graph,
                               std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap,
                               uint32_t /*width*/, uint32_t /*height*/,
                               std::shared_ptr<RHI::ResourceManager> /*resMgr*/,
                               RHI::DescriptorSetLayoutHandle /*globalSetLayout*/) {
        m_passNode = graph.addGraphicsPassNode(m_name);
        // 渲染区域 = 阴影贴图分辨率（与窗口尺寸无关）
        m_passNode->setRenderArea(kShadowMapSize, kShadowMapSize);

        RenderGraph::AttachmentParams depth;
        depth.clearDepth = 1.0f;
        depth.initialLayout = RHI::ImageLayout::Undefined;
        depth.finalLayout = RHI::ImageLayout::ShaderReadOnly;

        auto tid = texIdMap.find("ShadowMap");
        if (tid == texIdMap.end()) {
            LOG_ERROR("[{}] 缺少 'ShadowMap' 纹理（demo 须 addTextureDesc 注册）", m_name);
            return false;
        }
        auto key = m_passNode->addDepthOutput(tid->second, depth);

        auto& sb = m_passNode->addSubpass(m_name + "_Depth");
        sb.setTag(m_name + "_Depth");
        sb.addDepthStencilAttachmentRef(key);
        m_executor = std::make_shared<SceneDrawExecutor>();
        sb.setExecutor(m_executor);
        return true;
    }

    std::vector<PassSubpassInfo> ShadowPass::getSubpasses() const {
        std::vector<PassSubpassInfo> result;
        result.push_back({m_name + "_Depth", 0, m_executor, m_passNode});
        return result;
    }

    bool ShadowPass::ensureShaders(const std::shared_ptr<RHI::ResourceManager>& resMgr) {
        if (m_shadowVS.isValid() && m_shadowSkinnedVS.isValid() && m_shadowFS.isValid()) return true;
        Assets::ShaderLoader loader(resMgr);
        auto vertInfo = loader.loadFromFile("assets/shaders/shadow/shadow_depth.vert", RHI::ShaderStage::Vertex);
        auto skinnedInfo = loader.loadFromFile("assets/shaders/shadow/shadow_depth_skinned.vert", RHI::ShaderStage::Vertex);
        auto fragInfo = loader.loadFromFile("assets/shaders/shadow/shadow_depth.frag", RHI::ShaderStage::Fragment);
        if (!vertInfo || !skinnedInfo || !fragInfo) {
            LOG_ERROR("[{}] 阴影 shader 加载失败", m_name);
            return false;
        }
        m_shadowVS = vertInfo->module;
        m_shadowSkinnedVS = skinnedInfo->module;
        m_shadowFS = fragInfo->module;
        return m_shadowVS.isValid() && m_shadowSkinnedVS.isValid() && m_shadowFS.isValid();
    }

    bool ShadowPass::isSkinnedInput(const GraphicsPipelineState& pso) {
        bool hasBoneIndices = false, hasBoneWeights = false;
        for (const auto& attr : pso.vertexInput.attributes) {
            if (attr.location == 4) hasBoneIndices = true;
            else if (attr.location == 5) hasBoneWeights = true;
        }
        return hasBoneIndices && hasBoneWeights;
    }

    GraphicsPipelineState ShadowPass::buildShadowPSO(const GraphicsPipelineState& src) {
        GraphicsPipelineState pso = src;
        pso.vertexShader = isSkinnedInput(src) ? m_shadowSkinnedVS : m_shadowVS;
        pso.fragmentShader = m_shadowFS;
        // depth-only：只写深度
        pso.depthTestEnable = true;
        pso.depthWriteEnable = true;
        pso.depthCompareOp = RHI::CompareOp::Less;
        pso.attachments = {};
        return pso;
    }

    void ShadowPass::onSceneData(const AnalysisSceneResult& sceneData,const IPass::CompileContext& ctx,const std::string& /*defaultTag*/) {
        if (!m_passNode) return;
        RHI::RenderPassHandle rp = m_passNode->getRenderPassHandle();
        if (!rp.isValid()) return;

        if (!ensureShaders(ctx.resMgr)) return;

        m_executor->clearDrawItems();
        std::unordered_set<uint32_t> usedIndices;
        for (auto& item : sceneData.drawItems) {
            if (item->type != DrawItemType::Mesh) continue;
            if (item->isInstanced) continue;  
            m_executor->addDrawItem(item);
            if (item->pipelineIndex < sceneData.PSO.size())
                usedIndices.insert(item->pipelineIndex);
        }

        std::unordered_map<uint32_t, RHI::PipelineHandle> mapping;
        for (uint32_t idx : usedIndices) {
            GraphicsPipelineState shadow = buildShadowPSO(*sceneData.PSO[idx]);
            auto pipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
                ctx.resMgr.get(), shadow, rp, 0);
            if (pipeline.isValid()) mapping[idx] = pipeline;
        }
        m_executor->setPipelineMapping(std::move(mapping));
        LOG_INFO("[{}] shadow pipelines: {}（{} draw items）", m_name, mapping.size(), m_executor->getDrawItems().size());
    }

} // namespace StarryEngine
