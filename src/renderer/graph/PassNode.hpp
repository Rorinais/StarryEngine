#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <set>
#include <optional>
#include "Types.hpp"
#include "RenderPassBuilder.hpp"

namespace StarryEngine::RenderGraph {
    class ISubpassRenderer;

    struct PhysicalTextureInfo {
        RHI::TextureHandle handle;
        std::vector<void*> views;
    };

    // 附件配置类，支持链式设置
    class AttachmentConfig {
    public:
        AttachmentConfig& setClearColor(const RHI::Color& c) { m_clearColor = c; return *this; }
        AttachmentConfig& setClearDepth(float d) { m_clearDepth = d; return *this; }
        AttachmentConfig& setClearStencil(uint32_t s) { m_clearStencil = s; return *this; }
        AttachmentConfig& setLoadOp(RHI::AttachmentLoadOp op) { m_loadOp = op; return *this; }
        AttachmentConfig& setStoreOp(RHI::AttachmentStoreOp op) { m_storeOp = op; return *this; }
        AttachmentConfig& setStencilLoadOp(RHI::AttachmentLoadOp op) { m_stencilLoadOp = op; return *this; }
        AttachmentConfig& setStencilStoreOp(RHI::AttachmentStoreOp op) { m_stencilStoreOp = op; return *this; }
        AttachmentConfig& setInitialLayout(RHI::ImageLayout layout) { m_initialLayout = layout; return *this; }
        AttachmentConfig& setFinalLayout(RHI::ImageLayout layout) { m_finalLayout = layout; return *this; }

        const std::optional<RHI::Color>& getClearColor() const { return m_clearColor; }
        std::optional<float> getClearDepth() const { return m_clearDepth; }
        std::optional<uint32_t> getClearStencil() const { return m_clearStencil; }
        std::optional<RHI::AttachmentLoadOp> getLoadOp() const { return m_loadOp; }
        std::optional<RHI::AttachmentStoreOp> getStoreOp() const { return m_storeOp; }
        std::optional<RHI::AttachmentLoadOp> getStencilLoadOp() const { return m_stencilLoadOp; }
        std::optional<RHI::AttachmentStoreOp> getStencilStoreOp() const { return m_stencilStoreOp; }
        std::optional<RHI::ImageLayout> getInitialLayout() const { return m_initialLayout; }
        std::optional<RHI::ImageLayout> getFinalLayout() const { return m_finalLayout; }

    private:
        std::optional<RHI::Color> m_clearColor;
        std::optional<float> m_clearDepth;
        std::optional<uint32_t> m_clearStencil;
        std::optional<RHI::AttachmentLoadOp> m_loadOp;
        std::optional<RHI::AttachmentStoreOp> m_storeOp;
        std::optional<RHI::AttachmentLoadOp> m_stencilLoadOp;
        std::optional<RHI::AttachmentStoreOp> m_stencilStoreOp;
        std::optional<RHI::ImageLayout> m_initialLayout;
        std::optional<RHI::ImageLayout> m_finalLayout;
    };

    // 附件请求类型
    enum class AttachmentRequestType {
        ColorOutput,
        DepthOutput,
        Input,
        Resolve,
        Preserve
    };

    struct AttachmentRequest {
        TextureId texId;
        AttachmentRequestType type;
        AttachmentConfig config;
        std::string key;  // 编译时生成
    };

    // 前向声明
    class PassNode;

    // 子通道代理类，用于通过纹理 ID 添加附件
    class SubpassBuilderProxy {
    public:
        SubpassBuilderProxy(PassNode& pass, SubpassBuilder& builder) : m_pass(pass), m_builder(builder) {}

        SubpassBuilderProxy& addColorAttachment(TextureId tex, RHI::ImageLayout layout = RHI::ImageLayout::ColorAttachment);
        SubpassBuilderProxy& addInputAttachment(TextureId tex, RHI::ImageLayout layout = RHI::ImageLayout::ShaderReadOnly);
        SubpassBuilderProxy& addDepthStencilAttachment(TextureId tex, RHI::ImageLayout layout = RHI::ImageLayout::DepthStencilAttachment);
        SubpassBuilderProxy& addResolveAttachment(TextureId tex, RHI::ImageLayout layout = RHI::ImageLayout::ColorAttachment);
        SubpassBuilderProxy& addPreserveAttachment(TextureId tex);

        SubpassBuilderProxy& setPipelineName(const std::string& name) { m_builder.setPipelineName(name); return *this; }
        SubpassBuilderProxy& setPipelineDescription(const RHI::GraphicsPipelineDesc& desc) { m_builder.setPipelineDescription(desc); return *this; }
        SubpassBuilderProxy& setRenderer(ISubpassRenderer* renderer) { m_builder.setRenderer(renderer); return *this; }

    private:
        PassNode& m_pass;
        SubpassBuilder& m_builder;
    };

    class PassNode {
    public:
        explicit PassNode(const std::string& name);
        ~PassNode();

        // ----- 原有 API（保持不变）-----
        RenderPassBuilder& getBuilder() { return m_builder; }
        SubpassBuilder& addSubpass(const std::string& subpassName);  // 返回原有 SubpassBuilder（基于键）
        void setClearColor(const std::string& key, const RHI::Color& color);
        void setClearDepthStencil(const std::string& key, float depth, uint32_t stencil);
        void setRenderArea(uint32_t width, uint32_t height) { m_width = width; m_height = height; }
        void bindAttachment(const std::string& key, TextureId textureId);
        TextureId getBoundTextureId(const std::string& key) const;

        // ----- 新增：基于纹理 ID 的简洁 API -----
        AttachmentConfig& addColorOutput(TextureId texId);
        AttachmentConfig& addDepthOutput(TextureId texId);
        AttachmentConfig& addInput(TextureId texId);
        AttachmentConfig& addResolve(TextureId texId);
        AttachmentConfig& addPreserve(TextureId texId);

        // 返回子通道代理（内部将纹理 ID 转换为键）
        SubpassBuilderProxy addSubpassProxy(const std::string& subpassName);

        // 内部使用：根据纹理 ID 获取生成的键
        std::string getKeyForTexture(TextureId texId) const;

        // ----- 资源使用查询（供依赖分析）-----
        const std::set<TextureId>& getReadTextures() const { return m_readTextures; }
        const std::set<TextureId>& getWriteTextures() const { return m_writeTextures; }
        const std::set<BufferId>& getReadBuffers() const { return m_readBuffers; }
        const std::set<BufferId>& getWriteBuffers() const { return m_writeBuffers; }

        // 编译阶段（由 RenderGraph 调用）
        bool compile(std::shared_ptr<RHI::ResourceManager> resMgr,
            const std::unordered_map<TextureId, PhysicalTextureInfo>& texMap,
            const std::unordered_map<TextureId, RHI::TextureDesc>& texDescMap,  // 新增纹理描述映射
            const std::unordered_map<BufferId, RHI::BufferHandle>& bufMap);

        // 执行阶段
        void execute(RHI::RHICommandEncoder* encoder,
            uint32_t frameIndex,
            RHI::FramebufferHandle framebuffer);

        // 获取 Pass 信息
        const std::string& getName() const { return m_name; }
        RHI::RenderPassHandle getRenderPassHandle() const { return m_renderPassHandle; }
        const std::vector<std::string>& getAttachmentNames() const;
        uint32_t getWidth() const { return m_width; }
        uint32_t getHeight() const { return m_height; }

        // 收集资源使用（依赖分析前调用）
        void collectResourceUsage();

        // 获取所有附件请求（供编译时处理）
        const std::vector<AttachmentRequest>& getAttachmentRequests() const { return m_attachmentRequests; }

    private:
        std::string m_name;
        RenderPassBuilder m_builder;
        std::unique_ptr<RenderPassBuildResult> m_cachedBuildResult;

        // 资源使用（用于依赖分析）
        std::set<TextureId> m_readTextures;
        std::set<TextureId> m_writeTextures;
        std::set<BufferId> m_readBuffers;
        std::set<BufferId> m_writeBuffers;

        // 键绑定映射（原有）
        std::unordered_map<std::string, TextureId> m_attachmentBindings;

        // 新增：纹理 ID 到请求索引的映射
        std::unordered_map<TextureId, size_t> m_texToRequestIndex;
        std::vector<AttachmentRequest> m_attachmentRequests;

        // 编译后数据
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        uint32_t m_width = 0;
        uint32_t m_height = 0;

        RHI::RenderPassHandle m_renderPassHandle;
        std::vector<RHI::PipelineHandle> m_pipelines;
        std::vector<ISubpassRenderer*> m_subpassRenderers;
        std::vector<RHI::ClearValue> m_clearValues;
        std::unordered_map<std::string, uint32_t> m_attachmentNameToIndex;
        std::unordered_map<std::string, RHI::ClearValue> m_clearValueMap;
    };

    // 子通道代理类实现（内联）
    inline SubpassBuilderProxy& SubpassBuilderProxy::addColorAttachment(TextureId tex, RHI::ImageLayout layout) {
        m_builder.addColorAttachmentRef(m_pass.getKeyForTexture(tex), layout);
        return *this;
    }
    inline SubpassBuilderProxy& SubpassBuilderProxy::addInputAttachment(TextureId tex, RHI::ImageLayout layout) {
        m_builder.addInputAttachmentRef(m_pass.getKeyForTexture(tex), layout);
        return *this;
    }
    inline SubpassBuilderProxy& SubpassBuilderProxy::addDepthStencilAttachment(TextureId tex, RHI::ImageLayout layout) {
        m_builder.addDepthStencilAttachmentRef(m_pass.getKeyForTexture(tex), layout);
        return *this;
    }
    inline SubpassBuilderProxy& SubpassBuilderProxy::addResolveAttachment(TextureId tex, RHI::ImageLayout layout) {
        m_builder.addResolveAttachmentRef(m_pass.getKeyForTexture(tex), layout);
        return *this;
    }
    inline SubpassBuilderProxy& SubpassBuilderProxy::addPreserveAttachment(TextureId tex) {
        m_builder.addPreserveAttachmentRef(m_pass.getKeyForTexture(tex));
        return *this;
    }

} // namespace StarryEngine::RenderGraph