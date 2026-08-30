#pragma once
#include <renderer/graph/GraphNode.hpp>
#include <renderer/graph/SubpassBuilder.hpp>
#include <renderer/passExecutor/IPassExecutor.hpp>

namespace StarryEngine::RenderGraph {

    // ── 动态渲染节点（VK_KHR_dynamic_rendering）──
    // 无 VkRenderPass / VkFramebuffer 对象：执行时用 vkCmdBeginRendering + 附件视图。
    // pass 层仍通过 addSubpass（SubpassBuilder）声明段——动态路径编译时把各段 executors
    // 扁平化为 m_passExecutors，在同一个动态渲染通道内按序执行（无 subpass 概念）。
    class RenderNode : public GraphNode {
    public:
        explicit RenderNode(const std::string& name, PassType type = PassType::Graphics);
        ~RenderNode() override;

        // ── Graphics Pass 接口 ──
        std::string addColorOutput(TextureId texId, const AttachmentParams& params = AttachmentParams()) override;
        std::string addDepthOutput(TextureId texId, const AttachmentParams& params = AttachmentParams()) override;
        std::string addInput(TextureId texId, const AttachmentParams& params = AttachmentParams()) override;
        std::string addResolve(TextureId texId, const AttachmentParams& params = AttachmentParams()) override { return {}; }
        std::string addPreserve(TextureId texId) override { return {}; }
        SubpassBuilder& addSubpass(const std::string& subpassName) override {
            m_subpasses.emplace_back(subpassName);
            return m_subpasses.back();
        }
        bool compile(std::shared_ptr<RHI::ResourceManager> resMgr,
            const std::unordered_map<TextureId, PhysicalTextureInfo>& texMap,
            const std::unordered_map<TextureId, RHI::TextureDesc>& texDescMap,
            const std::unordered_map<BufferId, RHI::BufferHandle>& bufMap) override;

        // 动态渲染执行（串行路径）
        void executeDynamic(RHI::RHICommandEncoder* encoder,
            const RenderContext& context,
            uint32_t frameIndex,
            const std::vector<void*>& attachmentViews,
            uint32_t frameSlot = 0,
            void* depthView = nullptr) override;

        void recordBody(RHI::RHICommandEncoder* encoder,
            const RenderContext& context,
            uint32_t frameIndex,
            RHI::FramebufferHandle framebuffer,
            uint32_t subpassIndex,
            uint32_t frameSlot = 0) override;

        // 统一并行入口：compute 直接执行已录 secondary；graphics 走动态 beginRendering 框架
        void executeSecondariesOnPrimary(RHI::RHICommandEncoder* encoder,
            RHI::FramebufferHandle /*framebuffer*/,
            const std::vector<void*>& attachmentViews,
            void* depthView,
            const std::vector<void*>& secondaries) override;

        // ── 查询接口 ──
        const std::string& getName() const override { return m_name; }
        RHI::RenderPassHandle getRenderPassHandle() const override { return RHI::RenderPassHandle::Null(); }
        uint32_t getWidth() const override { return m_width; }
        uint32_t getHeight() const override { return m_height; }
        const std::vector<std::string>& getAttachmentNames() const override { return m_attachmentNames; }
        const std::vector<std::string>& getColorAttachmentNames() const override;
        const std::vector<std::string>& getDepthAttachmentNames() const override;
        const std::vector<RHI::AttachmentLoadOp>& getColorLoadOps() const override { return m_colorLoadOps; }
        const std::vector<RHI::AttachmentLoadOp>& getDepthLoadOps() const override { return m_depthLoadOps; }
        const std::vector<RHI::ClearValue>& getClearValues() const override { return m_clearValues; }
        TextureId getTextureIdForAttachmentKey(const std::string& key) const override;
        std::pair<RHI::ImageLayout, RHI::ImageLayout> getTextureLayout(TextureId texId) const override;
        uint32_t getSubpassCount() const override { return static_cast<uint32_t>(m_subpasses.size()); }
        RHI::ImageLayout getFinalLayout(TextureId texId) const override { return m_finalLayouts.at(texId); }
        void addDependency(const RHI::SubpassDependency&) override {}
        void resolveInferredAttachments(const std::function<bool(TextureId)>& isFirstWriter) override;

        static bool isDepthFormat(RHI::Format format);

    private:
        // 并行主缓冲框架（仅 executeSecondariesOnPrimary 内部使用）
        void beginRenderingOnPrimary(RHI::RHICommandEncoder* encoder,
            const std::vector<RHI::RenderingAttachmentInfo>& colorAttachments,
            const RHI::RenderingAttachmentInfo& depthAttachment,
            bool hasDepth);
        void endRenderingOnPrimary(RHI::RHICommandEncoder* encoder);

        std::vector<SubpassBuilder> m_subpasses;

        std::unordered_map<TextureId, RHI::ImageLayout> m_inputLayouts;

        std::vector<std::string> m_attachmentNames;
        std::vector<std::string> m_colorAttachmentKeys;
        std::vector<std::string> m_depthAttachmentKeys;
        std::vector<RHI::AttachmentLoadOp> m_colorLoadOps;
        std::vector<RHI::AttachmentLoadOp> m_depthLoadOps;
        std::vector<RHI::ClearValue> m_clearValues;
    };

} // namespace StarryEngine::RenderGraph
