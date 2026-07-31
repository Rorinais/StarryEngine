#include "GraphicsPass.hpp"

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
            auto& sb = m_passNode->addSubpass(sp.name);
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

} // namespace StarryEngine
