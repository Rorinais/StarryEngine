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
        // (layer,queue) 是 subpass 的唯一身份：本 pass 内重复即配置错误
        std::unordered_set<long long> usedKeys;

        // 附件纹理解析：未声明直接报错返回（不再抛 out_of_range）
        auto resolveTex = [&texIdMap, this](const std::string& texName, RenderGraph::TextureId& out) {
            auto it = texIdMap.find(texName);
            if (it == texIdMap.end()) {
                LOG_ERROR("[pass '{}'] 附件纹理 '{}' 未声明（先 RenderTextures::define / StandardPipeline::declare）",
                          m_name, texName);
                return false;
            }
            out = it->second;
            return true;
        };

        for (size_t idx = 0; idx < m_subpasses.size(); ++idx) {
            const auto& sp = m_subpasses[idx];
            if (sp.layer < 0) {
                LOG_ERROR("[pass '{}'] subpass(index={}) 未声明 layer，已跳过（无法路由）", m_name, idx);
                continue;
            }
            const long long subKey = (static_cast<long long>(sp.layer) << 32) | (sp.queue & 0xFFFFFFFFLL);
            if (!usedKeys.insert(subKey).second) {
                LOG_ERROR("[pass '{}'] (layer={}, queue={}) 重复，已跳过第二个 subpass", m_name, sp.layer, sp.queue);
                continue;
            }
            auto& sb = m_passNode->addSubpass(subpassName(sp.layer, sp.queue));
            sb.setLayer(sp.layer);
            sb.setQueue(sp.queue);

            // 颜色
            std::vector<std::string> ck;
            for (auto& att : sp.colorAttachments) {
                RenderGraph::TextureId tid{};
                if (!resolveTex(att.textureName, tid)) return false;
                auto& key = colorKeys[tid];
                if (key.empty()) key = m_passNode->addColorOutput(tid, att.params);
                ck.push_back(key);
            }
            // 深度
            std::string dk;
            if (sp.depthAttachment) {
                RenderGraph::TextureId tid{};
                if (!resolveTex(sp.depthAttachment->textureName, tid)) return false;
                auto& key = depthKeys[tid];
                if (key.empty()) key = m_passNode->addDepthOutput(tid, sp.depthAttachment->params);
                dk = key;
            }
            // 输入
            std::vector<std::string> ik;
            for (auto& att : sp.inputAttachments) {
                RenderGraph::TextureId tid{};
                if (!resolveTex(att.textureName, tid)) return false;
                auto& key = inputKeys[tid];
                if (key.empty()) key = m_passNode->addInput(tid, att.params);
                ik.push_back(key);
            }
            // 解析
            for (auto& att : sp.resolveAttachments) {
                RenderGraph::TextureId tid{};
                if (!resolveTex(att.textureName, tid)) return false;
                auto& key = resolveKeys[tid];
                if (key.empty()) key = m_passNode->addResolve(tid, att.params);
            }
            // 保留
            for (auto& texName : sp.preserveAttachments) {
                RenderGraph::TextureId tid{};
                if (!resolveTex(texName, tid)) return false;
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
            result.push_back({sp.layer, sp.queue, idx, sp.executor, m_passNode});
            ++idx;
        }
        return result;
    }

    void GraphicsPass::onSceneData(const AnalysisSceneResult& sceneData,
                                   const IPass::CompileContext& ctx) {
        if (!m_passNode) return;
        RHI::RenderPassHandle rp = m_passNode->getRenderPassHandle();
        const bool dynamicMode = !rp.isValid();
        if (!dynamicMode && !rp.isValid()) return;

        for (auto& sp : m_subpasses) {
            if (sp.executor) sp.executor->clearDrawItems();
        }
        // 分派：优先按 (layer, queue) 键匹配本 pass 的 subpass；未声明层的 item 退回 tag/默认段
        for (auto& item : sceneData.drawItems) {
            bool routed = false;
            if (item->renderLayer >= 0) {
                const long long key = (static_cast<long long>(item->renderLayer) << 32) | (item->renderQueue & 0xFFFFFFFFLL);
                for (auto& sp : m_subpasses) {
                    const long long spKey = (static_cast<long long>(sp.layer) << 32) | (sp.queue & 0xFFFFFFFFLL);
                    if (sp.executor && sp.layer >= 0 && spKey == key) {
                        sp.executor->addDrawItem(item); routed = true; break;
                    }
                }
            }
            (void)routed;   // 未声明层已在路径层统一归入 (Opaque,0)，不会走到这里
        }

        for (uint32_t i = 0; i < m_subpasses.size(); ++i) {
            if (!m_subpasses[i].executor) continue;
            auto& items = m_subpasses[i].executor->getDrawItems();
            if (items.empty()) continue;
            std::unordered_set<uint32_t> usedIndices;
            for (auto& item : items)
                if (item->pipelineIndex < sceneData.PSO.size()) usedIndices.insert(item->pipelineIndex);
            std::unordered_map<uint32_t, RHI::PipelineHandle> mapping;
            for (uint32_t idx : usedIndices) {
                auto pipeline = Assets::PipelineCache::getOrCreateGraphicsPipeline(
                    ctx.resMgr.get(), *sceneData.PSO[idx], rp, i,
                    RHI::Format::RGBA16_Float,            // SceneColor（动态渲染颜色格式）
                    RHI::Format::D24_UNorm_S8_UInt);      // Depth（动态渲染深度格式）
                if (pipeline.isValid()) mapping[idx] = pipeline;
            }
            m_subpasses[i].executor->setPipelineMapping(std::move(mapping));
        }

        
    }

} // namespace StarryEngine
