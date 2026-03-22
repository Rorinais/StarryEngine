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
#include "../subpassRecorder/ISubpassRecorder.hpp"

namespace StarryEngine::RenderGraph {

    static constexpr uint32_t SUBPASS_EXTERNAL = ~0U;
    static constexpr uint32_t ATTACHMENT_UNUSED = std::numeric_limits<uint32_t>::max();

    class SubpassBuilder {
    public:
        explicit SubpassBuilder(std::string name);
        ~SubpassBuilder() = default;

        SubpassBuilder& addColorAttachmentRef(const std::string& key,
            RHI::ImageLayout layout = RHI::ImageLayout::ColorAttachment);
        SubpassBuilder& addInputAttachmentRef(const std::string& key,
            RHI::ImageLayout layout = RHI::ImageLayout::ShaderReadOnly);
        SubpassBuilder& addResolveAttachmentRef(const std::string& key,
            RHI::ImageLayout layout = RHI::ImageLayout::ColorAttachment);
        SubpassBuilder& addDepthStencilAttachmentRef(const std::string& key,
            RHI::ImageLayout layout = RHI::ImageLayout::DepthStencilAttachment);
        SubpassBuilder& addPreserveAttachmentRef(const std::string& key);

        SubpassBuilder& setRecorder(std::shared_ptr<StarryEngine::ISubpassRecorder> recorder);
        std::shared_ptr<StarryEngine::ISubpassRecorder> getRecorder() const { return m_recorder; }

        RHI::SubpassDesc buildSubpassDesc(const std::unordered_map<std::string, uint32_t>& keyToIndexMap) const;

        const std::vector<std::string>& getColorAttachmentNames() const { return m_colorAttachmentKeys; }
        const std::vector<std::string>& getInputAttachmentNames() const { return m_inputAttachmentKeys; }
        const std::vector<std::string>& getResolveAttachmentNames() const { return m_resolveAttachmentKeys; }
        const std::optional<std::string>& getDepthStencilAttachmentName() const { return m_depthStencilAttachmentKey; }
        const std::vector<std::string>& getPreserveAttachmentNames() const { return m_preserveAttachmentKeys; }
        const std::string& getSubpassName() const { return m_subpassName; }

    private:
        std::string m_subpassName;

        std::vector<std::string> m_colorAttachmentKeys;
        std::vector<std::string> m_inputAttachmentKeys;
        std::vector<std::string> m_resolveAttachmentKeys;
        std::vector<std::string> m_preserveAttachmentKeys;
        std::optional<std::string> m_depthStencilAttachmentKey;

        std::shared_ptr<StarryEngine::ISubpassRecorder> m_recorder;
        std::unordered_map<std::string, RHI::ImageLayout> m_attachmentLayouts;
    };

} // namespace StarryEngine::RenderGraph