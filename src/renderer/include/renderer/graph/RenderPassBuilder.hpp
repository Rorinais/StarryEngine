#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <set>
#include <string>
#include <stdexcept>
#include <renderer/graph/SubpassBuilder.hpp>

namespace StarryEngine::RenderGraph {

    struct RenderPassBuildResult {
        std::string name;
        RHI::RenderPassDesc renderPassDesc;
        std::vector<std::string> attachmentNames;
        std::unordered_map<std::string, uint32_t> attachmentNameToIndex;
        std::vector<std::shared_ptr<StarryEngine::IPassExecutor>> passExecutors;
    };

    class RenderPassBuilder {
    public:
        explicit RenderPassBuilder(std::string name);
        ~RenderPassBuilder() = default;

        RenderPassBuilder& addAttachment(const std::string& key, const RHI::AttachmentDesc& attachment);

        SubpassBuilder& addSubpass(SubpassBuilder&& subpassBuilder);

        RenderPassBuilder& addDependency(const RHI::SubpassDependency& dependency);

        RenderPassBuilder& registerColorAttachment(const std::string& key,
            RHI::Format format,
            RHI::ImageLayout finalLayout = RHI::ImageLayout::ColorAttachment,
            RHI::AttachmentLoadOp loadOp = RHI::AttachmentLoadOp::Clear,
            RHI::AttachmentStoreOp storeOp = RHI::AttachmentStoreOp::Store,
            RHI::ImageLayout initialLayout = RHI::ImageLayout::Undefined);

        RenderPassBuilder& registerDepthAttachment(const std::string& key,
            RHI::Format format,
            RHI::AttachmentLoadOp loadOp = RHI::AttachmentLoadOp::Clear,
            RHI::AttachmentStoreOp storeOp = RHI::AttachmentStoreOp::DontCare,
            RHI::ImageLayout initialLayout = RHI::ImageLayout::Undefined,
            RHI::ImageLayout finalLayout = RHI::ImageLayout::DepthStencilAttachment);

        RenderPassBuilder& registerResolveAttachment(const std::string& key,
            RHI::Format format,
            RHI::ImageLayout finalLayout = RHI::ImageLayout::ColorAttachment);

        RenderPassBuilder& registerInputAttachment(const std::string& key,
            RHI::Format format,
            RHI::ImageLayout finalLayout = RHI::ImageLayout::ShaderReadOnly,
            RHI::ImageLayout initialLayout = RHI::ImageLayout::Undefined,
            RHI::AttachmentLoadOp loadOp = RHI::AttachmentLoadOp::Load,
            RHI::AttachmentStoreOp storeOp = RHI::AttachmentStoreOp::Store);

        void updateAttachmentFormat(uint32_t index, RHI::Format newFormat);
        // 声明式附件：编译前由渲染图推断 loadOp/storeOp/布局后覆盖
        void updateAttachmentParams(uint32_t index, RHI::AttachmentLoadOp loadOp,
            RHI::AttachmentStoreOp storeOp, RHI::ImageLayout initialLayout,
            RHI::ImageLayout finalLayout);

        std::unique_ptr<RenderPassBuildResult> build(bool autoDependencies = true);

        const std::unordered_map<std::string, uint32_t>& getAttachmentIndices() const { return m_attachmentIndices; }
        const std::vector<RHI::SubpassDependency>& getAutoDependencies() const { return m_autoDependencies; }
        const std::vector<SubpassBuilder>& getSubpassBuilders() const { return m_subpassBuilders; }
    private:
        std::string m_name;
        std::vector<RHI::AttachmentDesc> m_attachments;
        std::vector<SubpassBuilder> m_subpassBuilders;
        std::vector<RHI::SubpassDependency> m_manualDependencies;
        std::vector<RHI::SubpassDependency> m_autoDependencies;

        std::unordered_map<std::string, uint32_t> m_attachmentIndices;
        std::vector<std::string> m_attachmentNames;

        struct AttachmentUsage {
            std::set<uint32_t> writingSubpasses;
            std::set<uint32_t> readingSubpasses;
            RHI::ImageLayout initialLayout;
            RHI::ImageLayout finalLayout;
        };
        std::unordered_map<std::string, AttachmentUsage> m_attachmentUsage;

        void analyzeAttachmentUsage();
        void generateDependenciesFromUsage();
        void generateDependenciesForAttachment(const std::string& key, const AttachmentUsage& usage);
        void addColorReadAfterWriteDependency(uint32_t src, uint32_t dst);
        void addDepthReadAfterWriteDependency(uint32_t src, uint32_t dst);
        void addColorWriteAfterWriteDependency(uint32_t first, uint32_t second);
        void addDepthWriteAfterWriteDependency(uint32_t first, uint32_t second);
        void generateExternalDependencies(const AttachmentUsage& usage, bool isDepthStencil);

        bool isDepthStencilFormat(RHI::Format format) const;
        std::vector<RHI::SubpassDependency> mergeDependencies() const;
    };

} // namespace StarryEngine::RenderGraph