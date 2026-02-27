#pragma once
#include "../interface/RHI_ENUMS.hpp"
#include "../interface/RHI_STRUCTS_DESC.hpp"
#include "../interface/RHI_HANDLES_SYSTEM.hpp"
#include "../interface/RHI_STRUCTS_RESOURCE.hpp" 
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace StarryEngine::RenderGraph {
    struct ResourceId { uint32_t value; };
    struct SubpassId { uint32_t value; };
    struct RenderPassGroupId { uint32_t value; }; 

    class Scene;

    class RenderGraph {
    public:
        RenderGraph();  

        // ========== 资源管理 ==========
        ResourceId importTexture(const std::string& name,
            RHI::TextureHandle texture,
            RHI::ImageLayout initialLayout);
        ResourceId importBuffer(const std::string& name,
            RHI::BufferHandle buffer);

        ResourceId createTransientTexture(const std::string& name,
            const RHI::TextureDesc& desc);
        ResourceId createTransientBuffer(const std::string& name,
            const RHI::BufferDesc& desc);

        // ========== 构建 render pass 组 ==========
        // 单个独立 subpass（自动成为一个单独的 render pass）
        SubpassId addSubpass(const std::string& name);

        // 复合 render pass 组
        RenderPassGroupId beginRenderPassGroup(const std::string& name);
        SubpassId addSubpassToGroup(RenderPassGroupId group, const std::string& name);
        void endRenderPassGroup(RenderPassGroupId group);

        // ========== Subpass 配置 ==========
        // 颜色附件
        void addColorOutput(SubpassId subpass,
            ResourceId resource,
            RHI::AttachmentLoadOp loadOp,
            RHI::AttachmentStoreOp storeOp,
            const RHI::ClearValue& clearValue = {},
            const RHI::ImageSubresourceRange& range = RHI::ImageSubresourceRange{});

        // 深度/模板附件
        void setDepthStencil(SubpassId subpass,
            ResourceId resource,
            RHI::AttachmentLoadOp loadOp,
            RHI::AttachmentStoreOp storeOp,
            const RHI::ClearValue& clearValue = {},
            const RHI::ImageSubresourceRange& range = RHI::ImageSubresourceRange{});

        // 输入附件或普通只读资源
        void addInput(SubpassId subpass,
            ResourceId resource,
            RHI::ImageLayout expectedLayout,
            RHI::PipelineStageFlags stages,
            RHI::AccessFlags access,
            const RHI::ImageSubresourceRange& range = RHI::ImageSubresourceRange{});

        // 设置管线（图形或计算）
        void setPipeline(SubpassId subpass, RHI::PipelineHandle pipeline);

        // 设置绘制/调度回调（场景数据由外部传入）
        void setExecuteCallback(SubpassId subpass,
            std::function<void(RHI::RHICommandEncoder*,
                const Scene&)> callback);

        // ========== 高级依赖（可选）==========
        void addSubpassDependency(RenderPassGroupId group,
            uint32_t srcSubpass, uint32_t dstSubpass,
            RHI::PipelineStageFlags srcStageMask,
            RHI::PipelineStageFlags dstStageMask,
            RHI::AccessFlags srcAccessMask,
            RHI::AccessFlags dstAccessMask,
            bool byRegion = true);

        // ========== 编译和执行 ==========
        bool compile();
        void execute(RHI::RHICommandEncoder* encoder, const Scene& scene);

        // 窗口重建时标记失效
        void invalidate();

    private:
        // 内部实现（前向声明）
        struct Impl;
        std::unique_ptr<Impl> pImpl;
    };

} // namespace StarryEngine::RenderGraph