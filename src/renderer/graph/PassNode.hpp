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
    class ISubpassRecorder;

    struct PhysicalTextureInfo {
        RHI::TextureHandle handle;
        std::vector<void*> views;
    };

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
        std::string key;  
    };

    class PassNode;

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
        SubpassBuilderProxy& setRecorder(ISubpassRecorder* recorder) { m_builder.setRecorder(recorder); return *this; }

    private:
        PassNode& m_pass;
        SubpassBuilder& m_builder;
    };

    class PassNode {
    public:
        explicit PassNode(const std::string& name);
        ~PassNode();

        RenderPassBuilder& getBuilder() { return m_builder; }
        SubpassBuilder& addSubpass(const std::string& subpassName);  
        void setClearColor(const std::string& key, const RHI::Color& color);
        void setClearDepthStencil(const std::string& key, float depth, uint32_t stencil);
        void setRenderArea(uint32_t width, uint32_t height) { m_width = width; m_height = height; }
        void bindAttachment(const std::string& key, TextureId textureId);
        TextureId getBoundTextureId(const std::string& key) const;

        AttachmentConfig& addColorOutput(TextureId texId);
        AttachmentConfig& addDepthOutput(TextureId texId);
        AttachmentConfig& addInput(TextureId texId);
        AttachmentConfig& addResolve(TextureId texId);
        AttachmentConfig& addPreserve(TextureId texId);

        SubpassBuilderProxy addSubpassProxy(const std::string& subpassName);

        std::string getKeyForTexture(TextureId texId) const;

        const std::set<TextureId>& getReadTextures() const { return m_readTextures; }
        const std::set<TextureId>& getWriteTextures() const { return m_writeTextures; }
        const std::set<BufferId>& getReadBuffers() const { return m_readBuffers; }
        const std::set<BufferId>& getWriteBuffers() const { return m_writeBuffers; }

        bool compile(std::shared_ptr<RHI::ResourceManager> resMgr,
            const std::unordered_map<TextureId, PhysicalTextureInfo>& texMap,
            const std::unordered_map<TextureId, RHI::TextureDesc>& texDescMap,  
            const std::unordered_map<BufferId, RHI::BufferHandle>& bufMap);

        void execute(RHI::RHICommandEncoder* encoder,
            uint32_t frameIndex,
            RHI::FramebufferHandle framebuffer);

        const std::string& getName() const { return m_name; }
        RHI::RenderPassHandle getRenderPassHandle() const { return m_renderPassHandle; }
        const std::vector<std::string>& getAttachmentNames() const;
        uint32_t getWidth() const { return m_width; }
        uint32_t getHeight() const { return m_height; }

        void collectResourceUsage();

        const std::vector<AttachmentRequest>& getAttachmentRequests() const { return m_attachmentRequests; }

    private:
        std::string m_name;
        RenderPassBuilder m_builder;
        std::unique_ptr<RenderPassBuildResult> m_cachedBuildResult;

        std::set<TextureId> m_readTextures;
        std::set<TextureId> m_writeTextures;
        std::set<BufferId> m_readBuffers;
        std::set<BufferId> m_writeBuffers;

        std::unordered_map<std::string, TextureId> m_attachmentBindings;

        std::unordered_map<TextureId, size_t> m_texToRequestIndex;
        std::vector<AttachmentRequest> m_attachmentRequests;

        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        uint32_t m_width = 0;
        uint32_t m_height = 0;

        RHI::RenderPassHandle m_renderPassHandle;
        std::vector<RHI::PipelineHandle> m_pipelines;
        std::vector<ISubpassRecorder*> m_subpassRecorders;
        std::vector<RHI::ClearValue> m_clearValues;
        std::unordered_map<std::string, uint32_t> m_attachmentNameToIndex;
        std::unordered_map<std::string, RHI::ClearValue> m_clearValueMap;
    };

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