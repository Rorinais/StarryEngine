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

    // 物理纹理信息，包含句柄和所有视图（普通纹理1个，交换链多个）
    struct PhysicalTextureInfo {
        RHI::TextureHandle handle;
        std::vector<void*> views; 
    };

    class PassNode {
    public:
        explicit PassNode(const std::string& name);
        ~PassNode();

        // 获取 RenderPassBuilder 进行附件和 Subpass 配置
        RenderPassBuilder& getBuilder() { return m_builder; }

        // 便捷添加 Subpass（返回 SubpassBuilder 引用）
        SubpassBuilder& addSubpass(const std::string& subpassName);

        // 设置清除值（按附件键）
        void setClearColor(const std::string& key, const RHI::Color& color);
        void setClearDepthStencil(const std::string& key, float depth, uint32_t stencil);

        // 设置渲染区域尺寸
        void setRenderArea(uint32_t width, uint32_t height) { m_width = width; m_height = height; }

        // 绑定附件键到纹理ID
        void bindAttachment(const std::string& key, TextureId textureId);
        TextureId getBoundTextureId(const std::string& key) const;

        // --- 资源使用查询（供 RenderGraph 依赖分析）---
        const std::set<TextureId>& getReadTextures() const { return m_readTextures; }
        const std::set<TextureId>& getWriteTextures() const { return m_writeTextures; }
        const std::set<BufferId>& getReadBuffers() const { return m_readBuffers; }
        const std::set<BufferId>& getWriteBuffers() const { return m_writeBuffers; }

        // 编译阶段（由 RenderGraph 调用）
        bool compile(std::shared_ptr<RHI::ResourceManager> resMgr,
            const std::unordered_map<TextureId, PhysicalTextureInfo>& texMap,
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

        void collectResourceUsage();  // 无参数版本，使用内部绑定

    private:
        std::string m_name;
        RenderPassBuilder m_builder;
        std::unique_ptr<RenderPassBuildResult> m_cachedBuildResult;

        // 资源使用（用于依赖分析）
        std::set<TextureId> m_readTextures;
        std::set<TextureId> m_writeTextures;
        std::set<BufferId> m_readBuffers;
        std::set<BufferId> m_writeBuffers;

        // 键绑定映射
        std::unordered_map<std::string, TextureId> m_attachmentBindings;

        // 编译后数据
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        uint32_t m_width = 0;
        uint32_t m_height = 0;

        RHI::RenderPassHandle m_renderPassHandle;
        std::vector<RHI::PipelineHandle> m_pipelines;          // 每个 Subpass 一个 Pipeline
        std::vector<ISubpassRenderer*> m_subpassRenderers;      // 原始指针
        std::vector<RHI::ClearValue> m_clearValues;             // 按附件索引
        std::unordered_map<std::string, uint32_t> m_attachmentNameToIndex;
        std::unordered_map<std::string, RHI::ClearValue> m_clearValueMap;
    };

} // namespace StarryEngine::RenderGraph