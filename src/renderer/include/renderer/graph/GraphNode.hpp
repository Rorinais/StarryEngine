#pragma once
#include <string>
#include <vector>
#include <memory>
#include <set>
#include <unordered_map>
#include <functional>
#include <renderer/graph/Types.hpp>
#include <renderer/graph/SubpassBuilder.hpp>
#include <renderer/passExecutor/IPassExecutor.hpp>

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

    struct BufferTransition {
        int32_t srcPassIdx;
        uint32_t dstPassIdx;
        BufferId bufferId;
        RHI::PipelineStage srcStage;
        RHI::PipelineStage dstStage;
        RHI::AccessFlag srcAccess;
        RHI::AccessFlag dstAccess;
    };

    enum class PassType { Graphics, Compute };

    // ── 渲染图节点抽象：双模渲染后端 ──
    // PassNode（传统 render pass + framebuffer）与 RenderNode（动态渲染 VK_KHR_dynamic_rendering）
    // 共同实现本接口；RenderGraph 按设备能力创建对应类型。
    // pass 层（GraphicsPass/ShadowPass/ComputePass）只依赖本接口 + addSubpass（两节点都以
    // SubpassBuilder 形式提供"段"，动态路径在编译时将其 executors 扁平化）。
    class GraphNode {
    public:
        explicit GraphNode(std::string name, PassType type = PassType::Graphics)
            : m_name(std::move(name)), m_type(type) {}
        virtual ~GraphNode() = default;

        PassType getType() const { return m_type; }

        // ── Graphics Pass 接口 ──
        virtual std::string addColorOutput(TextureId texId, const AttachmentParams& params = AttachmentParams()) = 0;
        virtual std::string addDepthOutput(TextureId texId, const AttachmentParams& params = AttachmentParams()) = 0;
        virtual std::string addInput(TextureId texId, const AttachmentParams& params = AttachmentParams()) = 0;
        virtual std::string addResolve(TextureId texId, const AttachmentParams& params = AttachmentParams()) = 0;
        virtual std::string addPreserve(TextureId texId) = 0;
        virtual SubpassBuilder& addSubpass(const std::string& subpassName) = 0;
        virtual void setPassExecutor(uint32_t index, std::shared_ptr<StarryEngine::IPassExecutor> rec) {
            if (index < m_passExecutors.size()) m_passExecutors[index] = std::move(rec);
        }
        virtual void setRenderArea(uint32_t width, uint32_t height) { m_width = width; m_height = height; }

        // ── Compute Pass 接口 ──
        virtual void addReadTexture(TextureId t) { m_readTextures.insert(t); }
        virtual void addWriteTexture(TextureId t) { m_writeTextures.insert(t); m_computeWriteLayouts[t] = RHI::ImageLayout::General; }
        virtual void addReadBuffer(BufferId b) { m_readBuffers.insert(b); }
        virtual void addWriteBuffer(BufferId b) { m_writeBuffers.insert(b); }
        virtual void setComputeExecutor(std::shared_ptr<StarryEngine::IPassExecutor> r) { m_computeRecorder = std::move(r); }

        // 启用/禁用
        virtual void setEnabled(bool e) { m_enabled = e; }
        virtual bool isEnabled() const { return m_enabled; }

        virtual const std::set<TextureId>& getReadTextures() const { return m_readTextures; }
        virtual const std::set<TextureId>& getWriteTextures() const { return m_writeTextures; }
        virtual const std::set<BufferId>& getReadBuffers() const { return m_readBuffers; }
        virtual const std::set<BufferId>& getWriteBuffers() const { return m_writeBuffers; }

        virtual bool compile(std::shared_ptr<RHI::ResourceManager> resMgr,
            const std::unordered_map<TextureId, PhysicalTextureInfo>& texMap,
            const std::unordered_map<TextureId, RHI::TextureDesc>& texDescMap,
            const std::unordered_map<BufferId, RHI::BufferHandle>& bufMap) = 0;

        // 传统 render pass 执行（PassNode 覆盖；RenderNode no-op）
        virtual void execute(RHI::RHICommandEncoder*,
            const RenderContext&, uint32_t,
            RHI::FramebufferHandle, uint32_t = 0) {}

        // 动态渲染执行（RenderNode 覆盖；PassNode no-op）
        virtual void executeDynamic(RHI::RHICommandEncoder*,
            const RenderContext&, uint32_t,
            const std::vector<void*>&, uint32_t = 0, void* = nullptr) {}

        // 并行录制：secondary 侧 pass 主体（不含 begin/end，由主缓冲框架负责）
        virtual void recordBody(RHI::RHICommandEncoder* encoder,
            const RenderContext& context,
            uint32_t frameIndex,
            RHI::FramebufferHandle framebuffer,
            uint32_t subpassIndex,
            uint32_t frameSlot = 0) = 0;

        // ── 统一执行入口 ──
        // 串行：execute / executeDynamic（compute 与 graphics 差异在节点内部处理）。
        // 并行：executeSecondariesOnPrimary 在主缓冲执行已录好的 secondary——
        //   compute：无渲染通道，直接 executeCommands；
        //   graphics：PassNode 做 beginPass→exec→end，RenderNode 做 beginRendering→exec→end。
        // 这样 RenderGraph 无需区分 pass 类型，compute/graphics 差异收敛到节点内部。
        virtual void executeSecondariesOnPrimary(RHI::RHICommandEncoder* encoder,
            RHI::FramebufferHandle framebuffer,
            const std::vector<void*>& attachmentViews,
            void* depthView,
            const std::vector<void*>& secondaries) = 0;

        // ── 查询接口 ──
        virtual const std::string& getName() const = 0;
        virtual RHI::RenderPassHandle getRenderPassHandle() const = 0;
        virtual uint32_t getWidth() const = 0;
        virtual uint32_t getHeight() const = 0;
        virtual const std::vector<std::string>& getAttachmentNames() const = 0;
        virtual TextureId getTextureIdForAttachmentKey(const std::string& key) const = 0;
        virtual std::pair<RHI::ImageLayout, RHI::ImageLayout> getTextureLayout(TextureId texId) const = 0;
        virtual uint32_t getSubpassCount() const = 0;
        virtual RHI::ImageLayout getFinalLayout(TextureId texId) const = 0;
        virtual void addDependency(const RHI::SubpassDependency& dep) = 0;
        virtual void resolveInferredAttachments(const std::function<bool(TextureId)>& isFirstWriter) = 0;

        // 动态路径：颜色附件 key 列表（收集附件视图用）
        virtual const std::vector<std::string>& getColorAttachmentNames() const = 0;
        // 动态路径：深度附件 key 列表（纯深度 pass 如 ShadowPass）
        virtual const std::vector<std::string>& getDepthAttachmentNames() const = 0;

        // 动态路径：并行录制主缓冲需要 loadOp/clear 值（与 getColorAttachmentNames 对齐）
        virtual const std::vector<RHI::AttachmentLoadOp>& getColorLoadOps() const {
            static const std::vector<RHI::AttachmentLoadOp> empty;
            return empty;
        }
        virtual const std::vector<RHI::AttachmentLoadOp>& getDepthLoadOps() const {
            static const std::vector<RHI::AttachmentLoadOp> empty;
            return empty;
        }
        virtual const std::vector<RHI::ClearValue>& getClearValues() const {
            static const std::vector<RHI::ClearValue> empty;
            return empty;
        }

        static bool isDepthFormat(RHI::Format format);

    protected:
        PassType m_type = PassType::Graphics;

        // ── PassNode / RenderNode 公共状态 ──
        std::string m_name;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::shared_ptr<StarryEngine::IPassExecutor> m_computeRecorder;
        std::vector<std::shared_ptr<StarryEngine::IPassExecutor>> m_passExecutors;

        std::set<TextureId> m_readTextures;
        std::set<TextureId> m_writeTextures;
        std::set<BufferId> m_readBuffers;
        std::set<BufferId> m_writeBuffers;

        std::unordered_map<std::string, TextureId> m_keyToTexId;
        std::unordered_map<std::string, TextureId> m_attachmentKeyToTexId;
        std::unordered_map<std::string, AttachmentParams> m_keyToParams;
        std::unordered_map<TextureId, std::string> m_colorOutputKeyByTex;
        std::unordered_map<TextureId, std::string> m_depthOutputKeyByTex;
        std::unordered_map<TextureId, RHI::ImageLayout> m_computeWriteLayouts;
        std::unordered_map<TextureId, RHI::ImageLayout> m_finalLayouts;

        bool m_enabled = true;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
    };

} // namespace StarryEngine::RenderGraph
