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

    static constexpr uint32_t SUBPASS_EXTERNAL = ~0U;
    static constexpr uint32_t ATTACHMENT_UNUSED = std::numeric_limits<uint32_t>::max();

    class SubpassBuilder {
    public:
        explicit SubpassBuilder(std::string name);
        ~SubpassBuilder() = default;

        // 使用键添加附件
        SubpassBuilder& addColorAttachmentRef(const std::string& key,
            RHI::ImageLayout layout = RHI::ImageLayout::ColorAttachment);
        SubpassBuilder& addInputAttachmentRef(const std::string& key,
            RHI::ImageLayout layout = RHI::ImageLayout::ShaderReadOnly);
        SubpassBuilder& addResolveAttachmentRef(const std::string& key,
            RHI::ImageLayout layout = RHI::ImageLayout::ColorAttachment);
        SubpassBuilder& addDepthStencilAttachmentRef(const std::string& key,
            RHI::ImageLayout layout = RHI::ImageLayout::DepthStencilAttachment);
        SubpassBuilder& addPreserveAttachmentRef(const std::string& key);

        SubpassBuilder& setPipelineName(const std::string& pipelineName);
        SubpassBuilder& setPipelineDescription(const RHI::GraphicsPipelineDesc& desc);
        SubpassBuilder& setRenderer(ISubpassRenderer* renderer);

        ISubpassRenderer* getRenderer() const { return m_renderer; }
        const RHI::GraphicsPipelineDesc& getPipelineDescription() const { return m_pipelineDesc; }

        // 构建子流程描述
        RHI::SubpassDesc buildSubpassDesc(const std::unordered_map<std::string, uint32_t>& keyToIndexMap) const;

        // 获取附件键列表
        const std::vector<std::string>& getColorAttachmentNames() const { return m_colorAttachmentKeys; }
        const std::vector<std::string>& getInputAttachmentNames() const { return m_inputAttachmentKeys; }
        const std::vector<std::string>& getResolveAttachmentNames() const { return m_resolveAttachmentKeys; }
        const std::optional<std::string>& getDepthStencilAttachmentName() const { return m_depthStencilAttachmentKey; }
        const std::vector<std::string>& getPreserveAttachmentNames() const { return m_preserveAttachmentKeys; }
        const std::string& getPipelineName() const { return m_pipelineName; }
        const std::string& getSubpassName() const { return m_subpassName; }

    private:
        std::string m_subpassName;
        std::string m_pipelineName;

        std::vector<std::string> m_colorAttachmentKeys;
        std::vector<std::string> m_inputAttachmentKeys;
        std::vector<std::string> m_resolveAttachmentKeys;
        std::vector<std::string> m_preserveAttachmentKeys;
        std::optional<std::string> m_depthStencilAttachmentKey;

        RHI::GraphicsPipelineDesc m_pipelineDesc;
        ISubpassRenderer* m_renderer = nullptr;

        std::unordered_map<std::string, RHI::ImageLayout> m_attachmentLayouts;
    };

} // namespace StarryEngine::RenderGraph