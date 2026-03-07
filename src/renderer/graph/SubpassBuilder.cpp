#include "SubpassBuilder.hpp"

namespace StarryEngine::RenderGraph {

    SubpassBuilder::SubpassBuilder(std::string name)
        : m_subpassName(std::move(name)) {
    }

    SubpassBuilder& SubpassBuilder::addColorAttachmentRef(const std::string& key, RHI::ImageLayout layout) {
        m_colorAttachmentKeys.push_back(key);
        m_attachmentLayouts[key] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addInputAttachmentRef(const std::string& key, RHI::ImageLayout layout) {
        m_inputAttachmentKeys.push_back(key);
        m_attachmentLayouts[key] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addResolveAttachmentRef(const std::string& key, RHI::ImageLayout layout) {
        m_resolveAttachmentKeys.push_back(key);
        m_attachmentLayouts[key] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addDepthStencilAttachmentRef(const std::string& key, RHI::ImageLayout layout) {
        m_depthStencilAttachmentKey = key;
        m_attachmentLayouts[key] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addPreserveAttachmentRef(const std::string& key) {
        m_preserveAttachmentKeys.push_back(key);
        return *this;
    }

    SubpassBuilder& SubpassBuilder::setPipelineName(const std::string& pipelineName) {
        m_pipelineName = pipelineName;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::setPipelineDescription(const RHI::GraphicsPipelineDesc& desc) {
        m_pipelineDesc = desc;
        m_hasPipeline = true;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::setNoPipeline() {
        m_hasPipeline = false;
        m_pipelineDesc = RHI::GraphicsPipelineDesc{}; // 重置为默认
        return *this;
    }

    SubpassBuilder& SubpassBuilder::setRecorder(ISubpassRecorder* recorder) {
        m_recorder = recorder;
        return *this;
    }

    RHI::SubpassDesc SubpassBuilder::buildSubpassDesc(const std::unordered_map<std::string, uint32_t>& keyToIndexMap) const {
        RHI::SubpassDesc desc;
        desc.depthStencilAttachment = { ATTACHMENT_UNUSED, RHI::ImageLayout::Undefined };

        for (const auto& key : m_colorAttachmentKeys) {
            auto it = keyToIndexMap.find(key);
            if (it == keyToIndexMap.end()) throw std::runtime_error("Color attachment key not found: " + key);
            auto layoutIt = m_attachmentLayouts.find(key);
            if (layoutIt == m_attachmentLayouts.end()) throw std::runtime_error("Layout missing for color attachment: " + key);
            desc.colorAttachments.push_back({ it->second, layoutIt->second });
        }

        if (m_depthStencilAttachmentKey) {
            auto it = keyToIndexMap.find(*m_depthStencilAttachmentKey);
            if (it == keyToIndexMap.end()) throw std::runtime_error("Depth attachment key not found: " + *m_depthStencilAttachmentKey);
            auto layoutIt = m_attachmentLayouts.find(*m_depthStencilAttachmentKey);
            if (layoutIt == m_attachmentLayouts.end()) throw std::runtime_error("Layout missing for depth attachment: " + *m_depthStencilAttachmentKey);
            desc.depthStencilAttachment = { it->second, layoutIt->second };
        }

        for (const auto& key : m_inputAttachmentKeys) {
            auto it = keyToIndexMap.find(key);
            if (it == keyToIndexMap.end()) throw std::runtime_error("Input attachment key not found: " + key);
            auto layoutIt = m_attachmentLayouts.find(key);
            if (layoutIt == m_attachmentLayouts.end()) throw std::runtime_error("Layout missing for input attachment: " + key);
            desc.inputAttachments.push_back({ it->second, layoutIt->second });
        }

        for (const auto& key : m_resolveAttachmentKeys) {
            auto it = keyToIndexMap.find(key);
            if (it == keyToIndexMap.end()) throw std::runtime_error("Resolve attachment key not found: " + key);
            auto layoutIt = m_attachmentLayouts.find(key);
            if (layoutIt == m_attachmentLayouts.end()) throw std::runtime_error("Layout missing for resolve attachment: " + key);
            desc.resolveAttachments.push_back({ it->second, layoutIt->second });
        }

        for (const auto& key : m_preserveAttachmentKeys) {
            auto it = keyToIndexMap.find(key);
            if (it == keyToIndexMap.end()) throw std::runtime_error("Preserve attachment key not found: " + key);
            desc.preserveAttachments.push_back(it->second);
        }
        return desc;
    }

} // namespace StarryEngine::RenderGraph