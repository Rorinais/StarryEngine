#pragma once
#include "Subpass.hpp"
#include "../../subpassRenderer/ISubpassRenderer.hpp"

namespace StarryEngine::RenderGraph {
    class RenderPass {
    public:
        RenderPass();
        ~RenderPass() = default;

        // 添加附件描述
        void addAttachment(const RHI::AttachmentDesc& attachment);

        // 添加子流程（接收 Subpass 对象的所有权）
        void addSubpass(std::unique_ptr<Subpass> subpass);

        // 添加子流程依赖
        void addDependency(const RHI::SubpassDependency& dependency);

        // 获取构建完成的 RenderPassDesc（用于创建底层 RenderPass 对象）
        RHI::RenderPassDesc getRenderPassDesc() const;

    private:
        std::vector<RHI::AttachmentDesc> mAttachments;
        std::vector<std::unique_ptr<Subpass>> mSubpasses;
        std::vector<RHI::SubpassDependency> mDependencies;
    };

    struct RenderPassBuildResult {
        std::string name;
        std::unique_ptr<RenderPass> renderPass;
        std::unordered_map<std::string, uint32_t> pipelineNameToSubpassIndexMap;

        // 新增字段
        std::vector<std::string> attachmentNames;                       // 附件名称（按索引顺序）
        std::unordered_map<std::string, uint32_t> attachmentNameToIndex; // 名称到索引映射
        std::vector<RHI::GraphicsPipelineDesc> pipelineDescriptions;           // 每个 Subpass 的 Pipeline 描述
        std::vector<ISubpassRenderer*> subpassRenderers;                 // 每个 Subpass 的 Renderer 指针
    };

    class RenderPassBuilder {
    public:
        explicit RenderPassBuilder(std::string name);
        ~RenderPassBuilder() = default;

        // 使用字符串名称添加附件
        RenderPassBuilder& addAttachment(const std::string& name, const RHI::AttachmentDesc& attachment);

        // 添加子流程构建器
        SubpassBuilder& addSubpass(SubpassBuilder&& subpassBuilder);

        // 手动添加子流程依赖
        RenderPassBuilder& addDependency(const RHI::SubpassDependency& dependency);

        // 便捷方法：添加颜色附件
        RenderPassBuilder& addColorAttachment(const std::string& name,
            RHI::Format format,
            RHI::ImageLayout finalLayout = RHI::ImageLayout::ColorAttachment,
            RHI::AttachmentLoadOp loadOp = RHI::AttachmentLoadOp::Clear,
            RHI::AttachmentStoreOp storeOp = RHI::AttachmentStoreOp::Store);

        // 便捷方法：添加深度附件
        RenderPassBuilder& addDepthAttachment(const std::string& name,
            RHI::Format format,
            RHI::AttachmentLoadOp loadOp = RHI::AttachmentLoadOp::Clear,
            RHI::AttachmentStoreOp storeOp = RHI::AttachmentStoreOp::DontCare);

        // 便捷方法：添加解析附件
        RenderPassBuilder& addResolveAttachment(const std::string& name,
            RHI::Format format,
            RHI::ImageLayout finalLayout = RHI::ImageLayout::ColorAttachment);

        RenderPassBuilder& addInputAttachment(const std::string& name,
            RHI::Format format,
            RHI::ImageLayout finalLayout = RHI::ImageLayout::ShaderReadOnly,
            RHI::ImageLayout initialLayout = RHI::ImageLayout::Undefined,
            RHI::AttachmentLoadOp loadOp = RHI::AttachmentLoadOp::Load,
            RHI::AttachmentStoreOp storeOp = RHI::AttachmentStoreOp::Store);

        // 构建 RenderPass，返回包含更多信息的结果
        std::unique_ptr<RenderPassBuildResult> build(bool autoDependencies = true);

        // 获取附件索引映射（仅用于调试）
        const std::unordered_map<std::string, uint32_t>& getAttachmentIndices() const { return mAttachmentIndices; }

        // 获取推导出的依赖关系（供 RenderGraph 使用）
        const std::vector<RHI::SubpassDependency>& getAutoDependencies() const { return mAutoDependencies; }

        const std::vector<SubpassBuilder>& getSubpassBuilders() const { return mSubpassBuilders; }

    private:
        std::string mName;
        std::vector<RHI::AttachmentDesc> mAttachments;
        std::vector<SubpassBuilder> mSubpassBuilders;
        std::vector<RHI::SubpassDependency> mManualDependencies;
        std::vector<RHI::SubpassDependency> mAutoDependencies;

        // 名称到索引的映射
        std::unordered_map<std::string, uint32_t> mAttachmentIndices;
        std::vector<std::string> mAttachmentNames;

        // 附件使用分析（用于依赖推导）
        struct AttachmentUsage {
            std::set<uint32_t> writingSubpasses;  // 写入该附件的子流程
            std::set<uint32_t> readingSubpasses;  // 读取该附件的子流程
            RHI::ImageLayout initialLayout;
            RHI::ImageLayout finalLayout;
        };
        std::unordered_map<std::string, AttachmentUsage> mAttachmentUsage;

        // 依赖推导方法
        void analyzeAttachmentUsage();
        void generateDependenciesFromUsage();
        void generateDependenciesForAttachment(const std::string& name, const AttachmentUsage& usage);
        void addColorReadAfterWriteDependency(uint32_t src, uint32_t dst);
        void addDepthReadAfterWriteDependency(uint32_t src, uint32_t dst);
        void addColorWriteAfterWriteDependency(uint32_t first, uint32_t second);
        void addDepthWriteAfterWriteDependency(uint32_t first, uint32_t second);
        void generateExternalDependencies(const AttachmentUsage& usage, bool isDepthStencil);
        void generateExecutionDependencies();

        // 判断附件是否为深度模板格式
        bool isDepthStencilFormat(RHI::Format format) const;

        // 依赖合并
        std::vector<RHI::SubpassDependency> mergeDependencies() const;
    };
}