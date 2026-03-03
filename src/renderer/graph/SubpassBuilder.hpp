#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <stdexcept>
#include "../interface/RHI_ENUMS.hpp"
#include "../interface/RHI_HANDLES_SYSTEM.hpp"
#include "../interface/RHI_STRUCTS_DESC.hpp"
#include "../subpassRenderer/ISubpassRenderer.hpp"

namespace StarryEngine::RenderGraph {

    // 外部子流程索引常量
    static constexpr uint32_t SUBPASS_EXTERNAL = ~0U;
    static constexpr uint32_t ATTACHMENT_UNUSED = std::numeric_limits<uint32_t>::max();

    class SubpassBuilder {
    public:
        explicit SubpassBuilder(std::string name);
        ~SubpassBuilder() = default;

        // 使用字符串名称添加附件
        SubpassBuilder& addColorAttachmentRef(const std::string& name,
            RHI::ImageLayout layout = RHI::ImageLayout::ColorAttachment);
        SubpassBuilder& addInputAttachmentRef(const std::string& name,
            RHI::ImageLayout layout = RHI::ImageLayout::ShaderReadOnly);
        SubpassBuilder& addResolveAttachmentRef(const std::string& name,
            RHI::ImageLayout layout = RHI::ImageLayout::ColorAttachment);
        SubpassBuilder& addDepthStencilAttachmentRef(const std::string& name,
            RHI::ImageLayout layout = RHI::ImageLayout::DepthStencilAttachment);
        SubpassBuilder& addPreserveAttachmentRef(const std::string& name);

        SubpassBuilder& setPipelineName(const std::string& pipelineName);
        SubpassBuilder& setPipelineDescription(const RHI::GraphicsPipelineDesc& desc) {
            m_pipelineDesc = desc;
            return *this;
        }

        SubpassBuilder& setRenderer(ISubpassRenderer* renderer) {
            m_renderer = renderer;
            return *this;
        }

        ISubpassRenderer* getRenderer() const { return m_renderer; }
        const RHI::GraphicsPipelineDesc& getPipelineDescription() const { return m_pipelineDesc; }

        // 构建子流程描述（需要名称到索引的映射）
        RHI::SubpassDesc buildSubpassDesc(const std::unordered_map<std::string, uint32_t>& nameToIndexMap) const;

        // 获取附件名称列表（用于验证）
        const std::vector<std::string>& getColorAttachmentNames() const { return m_colorAttachmentNames; }
        const std::vector<std::string>& getInputAttachmentNames() const { return m_inputAttachmentNames; }
        const std::vector<std::string>& getResolveAttachmentNames() const { return m_resolveAttachmentNames; }
        const std::optional<std::string>& getDepthStencilAttachmentName() const { return m_depthStencilAttachmentName; }
        const std::vector<std::string>& getPreserveAttachmentNames() const { return m_preserveAttachmentNames; }
        const std::string& getPipelineName() const { return m_pipelineName; }
        const std::string& getSubpassName() const { return m_subpassName; }

    private:
        std::string m_subpassName;
        std::string m_pipelineName;

        std::vector<std::string> m_colorAttachmentNames;
        std::vector<std::string> m_inputAttachmentNames;
        std::vector<std::string> m_resolveAttachmentNames;
        std::vector<std::string> m_preserveAttachmentNames;
        std::optional<std::string> m_depthStencilAttachmentName;

        RHI::GraphicsPipelineDesc m_pipelineDesc;
        ISubpassRenderer* m_renderer = nullptr;

        std::unordered_map<std::string, RHI::ImageLayout> m_attachmentLayouts;
    };

} // namespace StarryEngine::RenderGraph