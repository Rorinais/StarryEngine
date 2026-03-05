#include "SubpassBuilder.hpp"

namespace StarryEngine::RenderGraph {

    SubpassBuilder::SubpassBuilder(std::string name)
        : m_subpassName(std::move(name)) {
    }

    SubpassBuilder& SubpassBuilder::addColorAttachmentRef(const std::string& name, RHI::ImageLayout layout) {
        m_colorAttachmentNames.push_back(name);
        m_attachmentLayouts[name] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addInputAttachmentRef(const std::string& name, RHI::ImageLayout layout) {
        m_inputAttachmentNames.push_back(name);
        m_attachmentLayouts[name] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addResolveAttachmentRef(const std::string& name, RHI::ImageLayout layout) {
        m_resolveAttachmentNames.push_back(name);
        m_attachmentLayouts[name] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addDepthStencilAttachmentRef(const std::string& name, RHI::ImageLayout layout) {
        m_depthStencilAttachmentName = name;
        m_attachmentLayouts[name] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addPreserveAttachmentRef(const std::string& name) {
        m_preserveAttachmentNames.push_back(name);
        return *this;
    }

    SubpassBuilder& SubpassBuilder::setPipelineName(const std::string& pipelineName) {
        m_pipelineName = pipelineName;
        return *this;
    }

    RHI::SubpassDesc SubpassBuilder::buildSubpassDesc(const std::unordered_map<std::string, uint32_t>& nameToIndexMap) const {
        RHI::SubpassDesc desc;
        // 显式初始化深度附件为未使用
        desc.depthStencilAttachment = { ATTACHMENT_UNUSED, RHI::ImageLayout::Undefined };

        // 颜色附件
        for (const auto& name : m_colorAttachmentNames) {
            auto it = nameToIndexMap.find(name);
            if (it == nameToIndexMap.end()) {
                throw std::runtime_error("Color attachment not found: " + name);
            }
            auto layoutIt = m_attachmentLayouts.find(name);
            if (layoutIt == m_attachmentLayouts.end()) {
                throw std::runtime_error("Layout missing for color attachment: " + name);
            }
            desc.colorAttachments.push_back({ it->second, layoutIt->second });
        }

        if (m_depthStencilAttachmentName) {
            auto it = nameToIndexMap.find(*m_depthStencilAttachmentName);
            auto layoutIt = m_attachmentLayouts.find(*m_depthStencilAttachmentName);
            desc.depthStencilAttachment = { it->second, layoutIt->second };
        }

        // 输入附件
        for (const auto& name : m_inputAttachmentNames) {
            auto it = nameToIndexMap.find(name);
            if (it == nameToIndexMap.end()) {
                throw std::runtime_error("Input attachment not found: " + name);
            }
            auto layoutIt = m_attachmentLayouts.find(name);
            if (layoutIt == m_attachmentLayouts.end()) {
                throw std::runtime_error("Layout missing for input attachment: " + name);
            }
            desc.inputAttachments.push_back({ it->second, layoutIt->second });
        }

        // 解析附件
        for (const auto& name : m_resolveAttachmentNames) {
            auto it = nameToIndexMap.find(name);
            if (it == nameToIndexMap.end()) {
                throw std::runtime_error("Resolve attachment not found: " + name);
            }
            auto layoutIt = m_attachmentLayouts.find(name);
            if (layoutIt == m_attachmentLayouts.end()) {
                throw std::runtime_error("Layout missing for resolve attachment: " + name);
            }
            desc.resolveAttachments.push_back({ it->second, layoutIt->second });
        }

        // 深度模板附件（如果设置了）
        if (m_depthStencilAttachmentName) {
            auto it = nameToIndexMap.find(*m_depthStencilAttachmentName);
            if (it == nameToIndexMap.end()) {
                throw std::runtime_error("Depth/stencil attachment not found: " + *m_depthStencilAttachmentName);
            }
            auto layoutIt = m_attachmentLayouts.find(*m_depthStencilAttachmentName);
            if (layoutIt == m_attachmentLayouts.end()) {
                throw std::runtime_error("Layout missing for depth/stencil attachment: " + *m_depthStencilAttachmentName);
            }
            desc.depthStencilAttachment = { it->second, layoutIt->second };
        }

        // 保留附件
        for (const auto& name : m_preserveAttachmentNames) {
            auto it = nameToIndexMap.find(name);
            if (it == nameToIndexMap.end()) {
                throw std::runtime_error("Preserve attachment not found: " + name);
            }
            desc.preserveAttachments.push_back(it->second);
        }

        return desc;
    }

} // namespace StarryEngine::RenderGraph