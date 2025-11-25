#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <set>
#include "RenderPass.hpp"
#include "SubpassBuilder.hpp"

namespace StarryEngine {

    class RenderPassBuilder {
    public:
        RenderPassBuilder(std::string name) {
			mName = std::move(name);
        }
        RenderPassBuilder(std::shared_ptr<LogicalDevice> logicalDevice);
        ~RenderPassBuilder() = default;

        // 使用字符串名称添加附件
        RenderPassBuilder& addAttachment(const std::string& name, const VkAttachmentDescription& attachment);

        // 添加子流程（使用字符串名称引用附件）
        RenderPassBuilder& addSubpass(SubpassBuilder& subpassBuilder);

        // 手动添加子流程依赖（可选）
        RenderPassBuilder& addDependency(const VkSubpassDependency& dependency);

        // 便捷方法：添加标准附件
        RenderPassBuilder& addColorAttachment(const std::string& name,
            VkFormat format,
            VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE);

        RenderPassBuilder& addDepthAttachment(const std::string& name,
            VkFormat format,
            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE);

        RenderPassBuilder& addResolveAttachment(const std::string& name,
            VkFormat format,
            VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // 构建 RenderPass（自动推导依赖）
        std::unique_ptr<RenderPass> build(bool autoDependencies = true);

        // 获取附件索引映射
        const std::unordered_map<std::string, uint32_t>& getAttachmentIndices() const { return mAttachmentIndices; }

        // 获取推导出的依赖关系（用于调试）
        const std::vector<VkSubpassDependency>& getAutoDependencies() const { return mAutoDependencies; }

    private:
		std::string mName;
        std::shared_ptr<LogicalDevice> mLogicalDevice;
        std::vector<VkAttachmentDescription> mAttachments;
        std::vector<SubpassBuilder> mSubpassBuilders;
        std::vector<VkSubpassDependency> mManualDependencies;
        std::vector<VkSubpassDependency> mAutoDependencies;

        // 名称到索引的映射
        std::unordered_map<std::string, uint32_t> mAttachmentIndices;
        std::vector<std::string> mAttachmentNames;

        // 附件使用分析
        struct AttachmentUsage {
            std::set<uint32_t> writingSubpasses;  // 写入该附件的子流程
            std::set<uint32_t> readingSubpasses;  // 读取该附件的子流程
            VkImageLayout initialLayout;
            VkImageLayout finalLayout;
        };

        std::unordered_map<std::string, AttachmentUsage> mAttachmentUsage;

        // 新的依赖推导方法
        void analyzeAttachmentUsage();
        void generateDependenciesFromUsage();
        void generateDependenciesForAttachment(const std::string& name, const AttachmentUsage& usage);

        // 具体的依赖创建方法
        void addColorReadAfterWriteDependency(uint32_t src, uint32_t dst, const std::string& attachment);
        void addDepthReadAfterWriteDependency(uint32_t src, uint32_t dst, const std::string& attachment);
        void addColorWriteAfterWriteDependency(uint32_t first, uint32_t second, const std::string& attachment);
        void addDepthWriteAfterWriteDependency(uint32_t first, uint32_t second, const std::string& attachment);
        void generateExternalDependencies(const std::string& name, const AttachmentUsage& usage, bool isDepthStencil);
        void generateExecutionDependencies();

        // 依赖合并
        std::vector<VkSubpassDependency> mergeDependencies();
    };
} // namespace StarryEngine