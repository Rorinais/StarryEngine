#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <set>
#include <string>
#include <stdexcept>
#include "SubpassBuilder.hpp"

namespace StarryEngine::RenderGraph {

    // 构建结果结构体，存储最终描述和辅助信息
    struct RenderPassBuildResult {
        std::string name;
        RHI::RenderPassDesc renderPassDesc;                       // 最终的 RenderPass 描述
        std::unordered_map<std::string, uint32_t> pipelineNameToSubpassIndexMap;
        std::vector<std::string> attachmentNames;                 // 附件名称（按索引顺序）
        std::unordered_map<std::string, uint32_t> attachmentNameToIndex; // 名称到索引映射
        std::vector<RHI::GraphicsPipelineDesc> pipelineDescriptions;      // 每个 Subpass 的 Pipeline 描述
        std::vector<ISubpassRenderer*> subpassRenderers;          // 每个 Subpass 的 Renderer 指针
    };

    class RenderPassBuilder {
    public:
        explicit RenderPassBuilder(std::string name);
        ~RenderPassBuilder() = default;

        // 使用字符串名称添加附件
        RenderPassBuilder& addAttachment(const std::string& name, const RHI::AttachmentDesc& attachment);

        // 添加子流程构建器（注意：传入右值，返回 SubpassBuilder 引用以便继续配置）
        SubpassBuilder& addSubpass(SubpassBuilder&& subpassBuilder);

        // 手动添加子流程依赖
        RenderPassBuilder& addDependency(const RHI::SubpassDependency& dependency);

        // 便捷方法：添加颜色附件
        RenderPassBuilder& registerColorAttachment(const std::string& name,
            RHI::Format format,
            RHI::ImageLayout finalLayout = RHI::ImageLayout::ColorAttachment,
            RHI::AttachmentLoadOp loadOp = RHI::AttachmentLoadOp::Clear,
            RHI::AttachmentStoreOp storeOp = RHI::AttachmentStoreOp::Store,
            RHI::ImageLayout initialLayout = RHI::ImageLayout::Undefined);

        // 便捷方法：添加深度附件
        RenderPassBuilder& registerDepthAttachment(const std::string& name,
            RHI::Format format,
            RHI::AttachmentLoadOp loadOp = RHI::AttachmentLoadOp::Clear,
            RHI::AttachmentStoreOp storeOp = RHI::AttachmentStoreOp::DontCare,
            RHI::ImageLayout initialLayout = RHI::ImageLayout::Undefined,
            RHI::ImageLayout finalLayout = RHI::ImageLayout::DepthStencilAttachment);

        // 便捷方法：添加解析附件
        RenderPassBuilder& registerResolveAttachment(const std::string& name,
            RHI::Format format,
            RHI::ImageLayout finalLayout = RHI::ImageLayout::ColorAttachment);

        // 便捷方法：添加输入附件
        RenderPassBuilder& registerInputAttachment(const std::string& name,
            RHI::Format format,
            RHI::ImageLayout finalLayout = RHI::ImageLayout::ShaderReadOnly,
            RHI::ImageLayout initialLayout = RHI::ImageLayout::Undefined,
            RHI::AttachmentLoadOp loadOp = RHI::AttachmentLoadOp::Load,
            RHI::AttachmentStoreOp storeOp = RHI::AttachmentStoreOp::Store);

        // 构建 RenderPass，返回包含更多信息的结果
        std::unique_ptr<RenderPassBuildResult> build(bool autoDependencies = true);

        // 获取附件索引映射（仅用于调试）
        const std::unordered_map<std::string, uint32_t>& getAttachmentIndices() const { return m_attachmentIndices; }

        // 获取推导出的依赖关系
        const std::vector<RHI::SubpassDependency>& getAutoDependencies() const { return m_autoDependencies; }

        const std::vector<SubpassBuilder>& getSubpassBuilders() const { return m_subpassBuilders; }

    private:
        std::string m_name;
        std::vector<RHI::AttachmentDesc> m_attachments;
        std::vector<SubpassBuilder> m_subpassBuilders;
        std::vector<RHI::SubpassDependency> m_manualDependencies;
        std::vector<RHI::SubpassDependency> m_autoDependencies;

        // 名称到索引的映射
        std::unordered_map<std::string, uint32_t> m_attachmentIndices;
        std::vector<std::string> m_attachmentNames;

        // 附件使用分析（用于依赖推导）
        struct AttachmentUsage {
            std::set<uint32_t> writingSubpasses;  // 写入该附件的子流程
            std::set<uint32_t> readingSubpasses;  // 读取该附件的子流程
            RHI::ImageLayout initialLayout;
            RHI::ImageLayout finalLayout;
        };
        std::unordered_map<std::string, AttachmentUsage> m_attachmentUsage;

        // 依赖推导方法
        void analyzeAttachmentUsage();
        void generateDependenciesFromUsage();
        void generateDependenciesForAttachment(const std::string& name, const AttachmentUsage& usage);
        void addColorReadAfterWriteDependency(uint32_t src, uint32_t dst);
        void addDepthReadAfterWriteDependency(uint32_t src, uint32_t dst);
        void addColorWriteAfterWriteDependency(uint32_t first, uint32_t second);
        void addDepthWriteAfterWriteDependency(uint32_t first, uint32_t second);
        void generateExternalDependencies(const AttachmentUsage& usage, bool isDepthStencil);

        // 判断附件是否为深度模板格式
        bool isDepthStencilFormat(RHI::Format format) const;

        // 依赖合并
        std::vector<RHI::SubpassDependency> mergeDependencies() const;
    };

} // namespace StarryEngine::RenderGraph