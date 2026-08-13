#include <renderer/passes/GraphicsPass.hpp>
#include <assets/material/MaterialTemplate.hpp>
#include <logging/Logger.hpp>
#include <unordered_set>

namespace StarryEngine {

    bool GraphicsPass::configure(RenderGraph::RenderGraph& graph,
                                  std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap,
                                  uint32_t width, uint32_t height,
                                  std::shared_ptr<RHI::ResourceManager> /*resMgr*/,
                                  RHI::DescriptorSetLayoutHandle /*globalSetLayout*/) {
        m_passNode = graph.addGraphicsPassNode(m_name);
        m_passNode->setRenderArea(width, height);

        // 附件 key 去重（同一纹理只注册一次到 PassNode）
        std::unordered_map<RenderGraph::TextureId, std::string> colorKeys, depthKeys,inputKeys, resolveKeys, preserveKeys;

        for (size_t idx = 0; idx < m_subpasses.size(); ++idx) {
            const auto& sp = m_subpasses[idx];
            auto& sb = m_passNode->addSubpass(sp.tag);
            sb.setTag(sp.tag);

            // 颜色
            std::vector<std::string> ck;
            for (auto& att : sp.colorAttachments) {
                auto tid = texIdMap.at(att.textureName);
                auto& key = colorKeys[tid];
                if (key.empty()) key = m_passNode->addColorOutput(tid, att.params);
                ck.push_back(key);
            }
            // 深度
            std::string dk;
            if (sp.depthAttachment) {
                auto tid = texIdMap.at(sp.depthAttachment->textureName);
                auto& key = depthKeys[tid];
                if (key.empty()) key = m_passNode->addDepthOutput(tid, sp.depthAttachment->params);
                dk = key;
            }
            // 输入
            std::vector<std::string> ik;
            for (auto& att : sp.inputAttachments) {
                auto tid = texIdMap.at(att.textureName);
                auto& key = inputKeys[tid];
                if (key.empty()) key = m_passNode->addInput(tid, att.params);
                ik.push_back(key);
            }
            // 解析
            for (auto& att : sp.resolveAttachments) {
                auto tid = texIdMap.at(att.textureName);
                auto& key = resolveKeys[tid];
                if (key.empty()) key = m_passNode->addResolve(tid, att.params);
            }
            // 保留
            for (auto& texName : sp.preserveAttachments) {
                auto tid = texIdMap.at(texName);
                auto& key = preserveKeys[tid];
                if (key.empty()) key = m_passNode->addPreserve(tid);
            }

            for (auto& k : ck) sb.addColorAttachmentRef(k);
            if (!dk.empty()) sb.addDepthStencilAttachmentRef(dk);
            for (auto& k : ik) sb.addInputAttachmentRef(k);

            sb.setExecutor(sp.executor);
        }
        
        resolveReadTextures(m_passNode, texIdMap);
        return true;
    }

    std::vector<PassSubpassInfo> GraphicsPass::getSubpasses() const {
        std::vector<PassSubpassInfo> result;
        uint32_t idx = 0;
        for (auto& sp : m_subpasses) {
            result.push_back({sp.tag, idx, sp.executor, m_passNode});
            ++idx;
        }
        return result;
    }

    // 场景数据更新：分发 draw items + 构建网格管线（原 DeferredRenderPath::distributeDrawItems
    // + prepareAllPipelines，统一收进 pass 内）
    void GraphicsPass::onSceneData(const AnalysisSceneResult& sceneData,
                                   const IPass::CompileContext& ctx,
                                   const std::string& defaultTag) {
        if (!m_passNode) return;
        RHI::RenderPassHandle rp = m_passNode->getRenderPassHandle();
        if (!rp.isValid()) return;

        // 1. 清空 + 按 subpass tag 分发 draw items（无 tag 走 defaultTag）
        for (auto& sp : m_subpasses) sp.executor->clearDrawItems();
        for (auto& item : sceneData.drawItems) {
            std::string tag = item->passTag.empty() ? defaultTag : item->passTag;
            for (auto& sp : m_subpasses) {
                if (sp.tag == tag) { sp.executor->addDrawItem(item); break; }
            }
        }

        // 2. 对每个 subpass，从 sceneData.PSO + 本 pass 的 render pass 构建管线映射
        for (uint32_t i = 0; i < m_subpasses.size(); ++i) {
            auto& items = m_subpasses[i].executor->getDrawItems();
            if (items.empty()) continue;
            std::unordered_set<uint32_t> usedIndices;
            for (auto& item : items)
                if (item->pipelineIndex < sceneData.PSO.size()) usedIndices.insert(item->pipelineIndex);
            std::unordered_map<uint32_t, RHI::PipelineHandle> mapping;
            for (uint32_t idx : usedIndices) {
                auto pipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
                    ctx.resMgr.get(), *sceneData.PSO[idx], rp, i);
                if (pipeline.isValid()) mapping[idx] = pipeline;
            }
            m_subpasses[i].executor->setPipelineMapping(std::move(mapping));
        }
    }

} // namespace StarryEngine
