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
        void setPassExecutor(uint32_t index, std::shared_ptr<StarryEngine::IPassExecutor> rec) override {
            if (index < m_passExecutors.size()) m_passExecutors[index] = std::move(rec);
        }
        void setRenderArea(uint32_t width, uint32_t height) override { m_width = width; m_height = height; }

        // ── Compute Pass 接口 ──
        void addReadTexture(TextureId t) override { m_readTextures.insert(t); }
        void addWriteTexture(TextureId t) override { m_writeTextures.insert(t); m_computeWriteLayouts[t] = RHI::ImageLayout::General; }
        void addReadBuffer(BufferId b) override { m_readBuffers.insert(b); }
        void addWriteBuffer(BufferId b) override { m_writeBuffers.insert(b); }
        void setComputeExecutor(std::shared_ptr<StarryEngine::IPassExecutor> r) override { m_computeRecorder = std::move(r); }

        void setEnabled(bool e) override { m_enabled = e; }
        bool isEnabled() const override { return m_enabled; }

        const std::set<TextureId>& getReadTextures() const override { return m_readTextures; }
        const std::set<TextureId>& getWriteTextures() const override { return m_writeTextures; }
        const std::set<BufferId>& getReadBuffers() const override { return m_readBuffers; }
        const std::set<BufferId>& getWriteBuffers() const override { return m_writeBuffers; }

        bool compile(std::shared_ptr<RHI::ResourceManager> resMgr,
            const std::unordered_map<TextureId, PhysicalTextureInfo>& texMap,
            const std::unordered_map<TextureId, RHI::TextureDesc>& texDescMap,
            const std::unordered_map<BufferId, RHI::BufferHandle>& bufMap) override;

        // 传统 render pass：动态节点不支持 → no-op（仅满足接口）
        void execute(RHI::RHICommandEncoder*, const RenderContext&, uint32_t,
            RHI::FramebufferHandle, uint32_t = 0) override {}

        // 动态渲染执行（串行路径）
        void executeDynamic(RHI::RHICommandEncoder* encoder,
            const RenderContext& context,
            uint32_t frameIndex,
            const std::vector<void*>& attachmentViews,
            uint32_t frameSlot = 0,
            void* depthView = nullptr) override;

        // 传统并行：no-op
        void beginPassOnPrimary(RHI::RHICommandEncoder*, RHI::FramebufferHandle, RHI::SubpassContents) override {}
        void nextSubpassOnPrimary(RHI::RHICommandEncoder*, RHI::SubpassContents) override {}
        void endPassOnPrimary(RHI::RHICommandEncoder*) override {}

        // 动态并行：主缓冲框架 + secondary 主体
        void beginRenderingOnPrimary(RHI::RHICommandEncoder* encoder,
            const std::vector<RHI::RenderingAttachmentInfo>& colorAttachments,
            const RHI::RenderingAttachmentInfo& depthAttachment,
            bool hasDepth) override;
        void endRenderingOnPrimary(RHI::RHICommandEncoder* encoder) override;
        void recordBody(RHI::RHICommandEncoder* encoder,
            const RenderContext& context,
            uint32_t frameIndex,
            RHI::FramebufferHandle framebuffer,
            uint32_t subpassIndex,
            uint32_t frameSlot = 0) override;

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
        std::string m_name;
        std::vector<SubpassBuilder> m_subpasses;
        std::vector<std::shared_ptr<StarryEngine::IPassExecutor>> m_passExecutors;
        std::shared_ptr<StarryEngine::IPassExecutor> m_computeRecorder;

        std::set<TextureId> m_readTextures;
        std::set<TextureId> m_writeTextures;
        std::set<BufferId> m_readBuffers;
        std::set<BufferId> m_writeBuffers;

        std::unordered_map<std::string, TextureId> m_keyToTexId;
        std::unordered_map<std::string, TextureId> m_attachmentKeyToTexId;
        std::unordered_map<std::string, AttachmentParams> m_keyToParams;
        std::unordered_map<TextureId, std::string> m_colorOutputKeyByTex;
        std::unordered_map<TextureId, std::string> m_depthOutputKeyByTex;
        std::unordered_map<TextureId, RHI::ImageLayout> m_inputLayouts;
        std::unordered_map<TextureId, RHI::ImageLayout> m_finalLayouts;
        std::unordered_map<TextureId, RHI::ImageLayout> m_computeWriteLayouts;

        std::vector<std::string> m_attachmentNames;
        std::vector<std::string> m_colorAttachmentKeys;   // 编译期缓存（并行线程只读）
        std::vector<std::string> m_depthAttachmentKeys;
        std::vector<RHI::AttachmentLoadOp> m_colorLoadOps;   // 与 m_colorAttachmentKeys 对齐（首写 Clear / 后续 Load）
        std::vector<RHI::AttachmentLoadOp> m_depthLoadOps;   // 与 m_depthAttachmentKeys 对齐
        std::vector<RHI::ClearValue> m_clearValues;

        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        bool m_enabled = true;
    };

} // namespace StarryEngine::RenderGraph
