// PassNode.hpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <set>
#include <optional>
#include <functional>
#include <renderer/graph/Types.hpp>
#include <renderer/graph/GraphNode.hpp>
#include <renderer/graph/RenderPassBuilder.hpp>
#include <renderer/graph/SubpassBuilder.hpp>

namespace StarryEngine::RenderGraph {

    class PassNode : public GraphNode {
    public:
        explicit PassNode(const std::string& name, PassType type = PassType::Graphics);
        ~PassNode() override;

        // ── Graphics Pass 接口 ──
        std::string addColorOutput(TextureId texId, const AttachmentParams& params = AttachmentParams());
        std::string addDepthOutput(TextureId texId, const AttachmentParams& params = AttachmentParams());
        std::string addInput(TextureId texId, const AttachmentParams& params = AttachmentParams());
        std::string addResolve(TextureId texId, const AttachmentParams& params = AttachmentParams());
        std::string addPreserve(TextureId texId);
        SubpassBuilder& addSubpass(const std::string& subpassName);
        bool compile(std::shared_ptr<RHI::ResourceManager> resMgr,
            const std::unordered_map<TextureId, PhysicalTextureInfo>& texMap,
            const std::unordered_map<TextureId, RHI::TextureDesc>& texDescMap,
            const std::unordered_map<BufferId, RHI::BufferHandle>& bufMap);

        void execute(RHI::RHICommandEncoder* encoder,
            const RenderContext& context,
            uint32_t frameIndex,
            RHI::FramebufferHandle framebuffer,
            uint32_t frameSlot = 0) override;

        void recordBody(RHI::RHICommandEncoder* encoder,
            const RenderContext& context,
            uint32_t frameIndex,
            RHI::FramebufferHandle framebuffer,
            uint32_t subpassIndex,
            uint32_t frameSlot = 0);

        // 统一并行入口：compute 直接执行已录 secondary；graphics 走传统 render pass 框架
        void executeSecondariesOnPrimary(RHI::RHICommandEncoder* encoder,
            RHI::FramebufferHandle framebuffer,
            const std::vector<void*>& /*attachmentViews*/,
            void* /*depthView*/,
            const std::vector<void*>& secondaries) override;

        // 查询接口
        const std::string& getName() const { return m_name; }
        RHI::RenderPassHandle getRenderPassHandle() const { return m_renderPassHandle; }
        uint32_t getWidth() const { return m_width; }
        uint32_t getHeight() const { return m_height; }
        const std::vector<std::string>& getAttachmentNames() const;
        const std::vector<std::string>& getColorAttachmentNames() const override;
        const std::vector<std::string>& getDepthAttachmentNames() const override;
        TextureId getTextureIdForAttachmentKey(const std::string& key) const;
        std::pair<RHI::ImageLayout, RHI::ImageLayout> getTextureLayout(TextureId texId) const;
        // 当前已添加的 subpass 数量（供共享 pass 时确定新 subpass 的索引）
        uint32_t getSubpassCount() const { return static_cast<uint32_t>(m_builder.getSubpassBuilders().size()); }
        RHI::ImageLayout getFinalLayout(TextureId texId) const { return m_finalLayouts.at(texId); }
        void addDependency(const RHI::SubpassDependency& dep);

        void resolveInferredAttachments(const std::function<bool(TextureId)>& isFirstWriter);

        static bool isDepthFormat(RHI::Format format);

    private:
        // 并行主缓冲框架（仅 executeSecondariesOnPrimary 内部使用）
        void beginPassOnPrimary(RHI::RHICommandEncoder* encoder,
            RHI::FramebufferHandle framebuffer,
            RHI::SubpassContents contents);
        void nextSubpassOnPrimary(RHI::RHICommandEncoder* encoder, RHI::SubpassContents contents);
        void endPassOnPrimary(RHI::RHICommandEncoder* encoder);

        RenderPassBuilder m_builder;
        RHI::RenderPassHandle m_renderPassHandle;
        std::unique_ptr<RenderPassBuildResult> m_cachedBuildResult;

        std::unordered_map<std::string, uint32_t> m_attachmentNameToIndex;
        std::unordered_map<std::string, RHI::ClearValue> m_clearValueMap;
        mutable std::vector<std::string> m_colorKeysCache;
        mutable std::vector<std::string> m_depthKeysCache;
        std::vector<RHI::ClearValue> m_clearValues;

        uint32_t m_nextAttachmentKey = 1;
    };

} // namespace StarryEngine::RenderGraph