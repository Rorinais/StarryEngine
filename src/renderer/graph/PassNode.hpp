#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

#include "Types.hpp"
#include "renderpass/Subpass.hpp"
#include "renderpass/RenderPass.hpp"
#include "../subpassRenderer/ISubpassRenderer.hpp"

namespace StarryEngine::RenderGraph {
    class PassNode {
    public:
        explicit PassNode(const std::string& name);
        ~PassNode() = default;

        // 获取 RenderPassBuilder 进行附件和 Subpass 配置
        RenderPassBuilder& getBuilder() { return m_builder; }

        // 便捷添加 Subpass（返回 SubpassBuilder 引用）
        SubpassBuilder& addSubpass(const std::string& subpassName);

        // 设置清除值（按附件名称）
        void setClearColor(const std::string& attachmentName, const RHI::Color& color);
        void setClearDepthStencil(const std::string& attachmentName, float depth, uint32_t stencil);

        // 设置渲染区域尺寸（由 RenderGraph 传入）
        void setRenderArea(uint32_t width, uint32_t height) { m_width = width; m_height = height; }

        // --- 资源使用查询（供 RenderGraph 依赖分析）---
        const std::set<TextureId>& getReadTextures() const { return m_readTextures; }
        const std::set<TextureId>& getWriteTextures() const { return m_writeTextures; }
        const std::set<BufferId>& getReadBuffers() const { return m_readBuffers; }
        const std::set<BufferId>& getWriteBuffers() const { return m_writeBuffers; }

        // 编译阶段（由 RenderGraph 调用，在确定执行顺序后）
        bool compile(std::shared_ptr<RHI::ResourceManager> resMgr,
            const std::unordered_map<TextureId, RHI::TextureHandle>& texMap,
            const std::unordered_map<BufferId, RHI::BufferHandle>& bufMap);

        // 执行阶段
        void execute(RHI::RHICommandEncoder* encoder,
            uint32_t frameIndex,
            RHI::FramebufferHandle framebuffer);

        // 获取 Pass 名称
        const std::string& getName() const { return m_name; }
        RHI::RenderPassHandle getRenderPassHandle() const { return m_renderPassHandle; }

        void collectResourceUsage(const std::unordered_map<std::string, TextureId>& nameToTexId,
            const std::unordered_map<std::string, BufferId>& nameToBufId);

    private:
        std::string m_name;
        RenderPassBuilder m_builder;
        std::unique_ptr<RenderPassBuildResult> m_cachedBuildResult;

        // 资源使用（用于依赖分析）
        std::set<TextureId> m_readTextures;
        std::set<TextureId> m_writeTextures;
        std::set<BufferId> m_readBuffers;
        std::set<BufferId> m_writeBuffers;

        // 编译后数据
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        uint32_t m_width = 0;
        uint32_t m_height = 0;

        RHI::RenderPassHandle m_renderPassHandle;
        std::vector<RHI::PipelineHandle> m_pipelines;      // 每个 Subpass 一个 Pipeline
        std::vector<ISubpassRenderer*> m_subpassRenderers; // 原始指针
        std::vector<RHI::ClearValue> m_clearValues;        // 按附件索引
        std::unordered_map<std::string, uint32_t> m_attachmentNameToIndex;
        std::unordered_map<std::string, RHI::ClearValue> m_clearValueMap;
    };

} // namespace StarryEngine::RenderGraph