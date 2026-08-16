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
#include <renderer/graph/RenderPassBuilder.hpp>
#include <renderer/graph/SubpassBuilder.hpp>

namespace StarryEngine::RenderGraph {

    class IPassExecutor;

    struct PhysicalTextureInfo {
        RHI::TextureHandle handle;
        std::vector<void*> views;
    };

    struct LayoutTransition {
        int32_t srcPassIdx;  
        uint32_t dstPassIdx;
        TextureId texId;
        RHI::ImageLayout srcLayout;
        RHI::ImageLayout dstLayout;
        RHI::PipelineStage srcStage;
        RHI::PipelineStage dstStage;
        RHI::AccessFlag srcAccess;
        RHI::AccessFlag dstAccess;
        uint32_t aspectMask;
    };

    enum class PassType { Graphics, Compute };

    class PassNode {
    public:
        explicit PassNode(const std::string& name, PassType type = PassType::Graphics);
        ~PassNode();

        PassType getType() const { return m_type; }

        // ── Graphics Pass 接口 ──
        std::string addColorOutput(TextureId texId, const AttachmentParams& params = AttachmentParams());
        std::string addDepthOutput(TextureId texId, const AttachmentParams& params = AttachmentParams());
        std::string addInput(TextureId texId, const AttachmentParams& params = AttachmentParams());
        std::string addResolve(TextureId texId, const AttachmentParams& params = AttachmentParams());
        std::string addPreserve(TextureId texId);
        SubpassBuilder& addSubpass(const std::string& subpassName);
        void setPassExecutor(uint32_t index, std::shared_ptr<StarryEngine::IPassExecutor> rec) {
            if (index < m_passExecutors.size()) m_passExecutors[index] = std::move(rec);
        }
        void setRenderArea(uint32_t width, uint32_t height) { m_width = width; m_height = height; }

        // ── Compute Pass 接口 ──
        void addReadTexture(TextureId t)  { m_readTextures.insert(t); }
        void addWriteTexture(TextureId t) { m_writeTextures.insert(t); m_computeWriteLayouts[t] = RHI::ImageLayout::General; }
        void addReadBuffer(BufferId b)    { m_readBuffers.insert(b); }
        void addWriteBuffer(BufferId b)   { m_writeBuffers.insert(b); }
        // 执行全部交给 executor（绑管线 + 描述符 + push + dispatch），PassNode 只做结构
        void setComputeExecutor(std::shared_ptr<StarryEngine::IPassExecutor> r) { m_computeRecorder = std::move(r); }

        // Pass 启用/禁用
        void setEnabled(bool e) { m_enabled = e; }
        bool isEnabled() const { return m_enabled; }

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
            RHI::FramebufferHandle framebuffer,
            uint32_t frameSlot = 0);

        void beginPassOnPrimary(RHI::RHICommandEncoder* encoder,
            RHI::FramebufferHandle framebuffer,
            RHI::SubpassContents contents);
        void nextSubpassOnPrimary(RHI::RHICommandEncoder* encoder, RHI::SubpassContents contents);
        void endPassOnPrimary(RHI::RHICommandEncoder* encoder);
        void recordBody(RHI::RHICommandEncoder* encoder,
            const RenderContext& context,
            uint32_t frameIndex,
            RHI::FramebufferHandle framebuffer,
            uint32_t subpassIndex,
            uint32_t frameSlot = 0);

        // 查询接口
        const std::string& getName() const { return m_name; }
        RHI::RenderPassHandle getRenderPassHandle() const { return m_renderPassHandle; }
        uint32_t getWidth() const { return m_width; }
        uint32_t getHeight() const { return m_height; }
        const std::vector<std::string>& getAttachmentNames() const;
        TextureId getTextureIdForAttachmentKey(const std::string& key) const;
        std::pair<RHI::ImageLayout, RHI::ImageLayout> getTextureLayout(TextureId texId) const;
        // 当前已添加的 subpass 数量（供共享 pass 时确定新 subpass 的索引）
        uint32_t getSubpassCount() const { return static_cast<uint32_t>(m_builder.getSubpassBuilders().size()); }
        RHI::ImageLayout getFinalLayout(TextureId texId) const { return m_finalLayouts.at(texId); }
        void addDependency(const RHI::SubpassDependency& dep);

        void resolveInferredAttachments(const std::function<bool(TextureId)>& isFirstWriter);

        static bool isDepthFormat(RHI::Format format);

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
        // 颜色附件去重：texId → 已注册的颜色附件 key（同纹理+同参数复用，支持多 subpass 共享）
        std::unordered_map<TextureId, std::string> m_colorOutputKeyByTex;
        // 深度附件：texId → key（声明式附件推断需要区分颜色/深度以决定默认布局）
        std::unordered_map<TextureId, std::string> m_depthOutputKeyByTex;

        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        uint32_t m_width = 0;
        uint32_t m_height = 0;

        RHI::RenderPassHandle m_renderPassHandle;
        std::vector<RHI::ClearValue> m_clearValues;
        std::vector<std::shared_ptr<StarryEngine::IPassExecutor>> m_passExecutors;
        std::unordered_map<std::string, uint32_t> m_attachmentNameToIndex;
        std::unordered_map<std::string, RHI::ClearValue> m_clearValueMap;

        uint32_t m_nextAttachmentKey = 1;

        std::unordered_map<TextureId, RHI::ImageLayout> m_finalLayouts;

        bool m_enabled = true;
        PassType m_type = PassType::Graphics;

        // ── Compute 专用 ──
        std::shared_ptr<StarryEngine::IPassExecutor> m_computeRecorder;
        std::unordered_map<TextureId, RHI::ImageLayout> m_computeWriteLayouts;


    };

} // namespace StarryEngine::RenderGraph