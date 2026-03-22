// PassNode.hpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <set>
#include <optional>
#include "Types.hpp"
#include "RenderPassBuilder.hpp"
#include "SubpassBuilder.hpp"

namespace StarryEngine::RenderGraph {

    class ISubpassRecorder;

    struct PhysicalTextureInfo {
        RHI::TextureHandle handle;
        std::vector<void*> views;
    };

    struct LayoutTransition {
        uint32_t srcPassIdx;
        uint32_t dstPassIdx;
        TextureId texId;
        RHI::ImageLayout srcLayout;
        RHI::ImageLayout dstLayout;
    };

    class PassNode {
    public:
        explicit PassNode(const std::string& name);
        ~PassNode();

        std::string addColorOutput(TextureId texId, const AttachmentParams& params = AttachmentParams());
        std::string addDepthOutput(TextureId texId, const AttachmentParams& params = AttachmentParams());
        std::string addInput(TextureId texId, const AttachmentParams& params = AttachmentParams());
        std::string addResolve(TextureId texId, const AttachmentParams& params = AttachmentParams());
        std::string addPreserve(TextureId texId);

        SubpassBuilder& addSubpass(const std::string& subpassName);
        void setRenderArea(uint32_t width, uint32_t height) { m_width = width; m_height = height; }

        const std::set<TextureId>& getReadTextures() const { return m_readTextures; }
        const std::set<TextureId>& getWriteTextures() const { return m_writeTextures; }
        const std::set<BufferId>& getReadBuffers() const { return m_readBuffers; }
        const std::set<BufferId>& getWriteBuffers() const { return m_writeBuffers; }

        bool compile(std::shared_ptr<RHI::ResourceManager> resMgr,
            const std::unordered_map<TextureId, PhysicalTextureInfo>& texMap,
            const std::unordered_map<TextureId, RHI::TextureDesc>& texDescMap,
            const std::unordered_map<BufferId, RHI::BufferHandle>& bufMap);

        void execute(RHI::RHICommandEncoder* encoder,
            const RenderContext& context,
            uint32_t frameIndex,
            RHI::FramebufferHandle framebuffer);

        // 查询接口
        const std::string& getName() const { return m_name; }
        RHI::RenderPassHandle getRenderPassHandle() const { return m_renderPassHandle; }
        uint32_t getWidth() const { return m_width; }
        uint32_t getHeight() const { return m_height; }
        const std::vector<std::string>& getAttachmentNames() const;
        TextureId getTextureIdForAttachmentKey(const std::string& key) const;
        std::pair<RHI::ImageLayout, RHI::ImageLayout> getTextureLayout(TextureId texId) const;
        RHI::ImageLayout getFinalLayout(TextureId texId) const { return m_finalLayouts.at(texId); }
        void addDependency(const RHI::SubpassDependency& dep);

    private:
        std::string m_name;
        RenderPassBuilder m_builder;
        std::unique_ptr<RenderPassBuildResult> m_cachedBuildResult;

        std::set<TextureId> m_readTextures;
        std::set<TextureId> m_writeTextures;
        std::set<BufferId> m_readBuffers;
        std::set<BufferId> m_writeBuffers;

        std::unordered_map<std::string, TextureId> m_keyToTexId;
        std::unordered_map<std::string, TextureId> m_attachmentKeyToTexId;
        std::unordered_map<std::string, AttachmentParams> m_keyToParams;

        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        uint32_t m_width = 0;
        uint32_t m_height = 0;

        RHI::RenderPassHandle m_renderPassHandle;
        std::vector<RHI::ClearValue> m_clearValues;
        std::vector<std::shared_ptr<StarryEngine::ISubpassRecorder>> m_subpassRecorders;
        std::unordered_map<std::string, uint32_t> m_attachmentNameToIndex;
        std::unordered_map<std::string, RHI::ClearValue> m_clearValueMap;

        uint32_t m_nextAttachmentKey = 1;

        std::unordered_map<TextureId, RHI::ImageLayout> m_finalLayouts;

        static bool isDepthFormat(RHI::Format format);
    };

} // namespace StarryEngine::RenderGraph