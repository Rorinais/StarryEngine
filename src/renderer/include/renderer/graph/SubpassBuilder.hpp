#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <stdexcept>
#include <renderer/interface/RHIEnums.hpp>
#include <renderer/interface/RHIHandles.hpp>
#include <renderer/interface/RHIStructs.hpp>
#include <renderer/passExecutor/IPassExecutor.hpp>

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

        SubpassBuilder& setExecutor(std::shared_ptr<StarryEngine::IPassExecutor> executor);
        std::shared_ptr<StarryEngine::IPassExecutor> getExecutor() const { return m_recorder; }

        RHI::SubpassDesc buildSubpassDesc(const std::unordered_map<std::string, uint32_t>& keyToIndexMap) const;

        const std::vector<std::string>& getColorAttachmentNames() const { return m_colorAttachmentKeys; }
        const std::vector<std::string>& getInputAttachmentNames() const { return m_inputAttachmentKeys; }
        const std::vector<std::string>& getResolveAttachmentNames() const { return m_resolveAttachmentKeys; }
        const std::optional<std::string>& getDepthStencilAttachmentName() const { return m_depthStencilAttachmentKey; }
        const std::vector<std::string>& getPreserveAttachmentNames() const { return m_preserveAttachmentKeys; }
        const std::string& getSubpassName() const { return m_subpassName; }

        SubpassBuilder& setLayer(int layer) {
            m_layer = layer;
            return *this;
        }
        SubpassBuilder& setQueue(int queue) {
            m_queue = queue;
            return *this;
        }
        int getLayer() const { return m_layer; }
        int getQueue() const { return m_queue; }

    private:
        std::string m_subpassName;

        std::vector<std::string> m_colorAttachmentKeys;
        std::vector<std::string> m_inputAttachmentKeys;
        std::vector<std::string> m_resolveAttachmentKeys;
        std::vector<std::string> m_preserveAttachmentKeys;
        std::optional<std::string> m_depthStencilAttachmentKey;

        std::shared_ptr<StarryEngine::IPassExecutor> m_recorder;
        std::unordered_map<std::string, RHI::ImageLayout> m_attachmentLayouts;
        int m_layer = -1;
        int m_queue = 0;
    };

} // namespace StarryEngine::RenderGraph