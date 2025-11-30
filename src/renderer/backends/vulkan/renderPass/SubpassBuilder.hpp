#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <memory>
#include "Subpass.hpp"


namespace StarryEngine {

    class SubpassBuilder {
    public:
        SubpassBuilder(std::string name) {
            mSubpassName = std::move(name);
        }
        ~SubpassBuilder() = default;

        // 使用字符串名称添加附件
        SubpassBuilder& addColorAttachment(const std::string& name,
            VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        SubpassBuilder& addInputAttachment(const std::string& name,
            VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        SubpassBuilder& addResolveAttachment(const std::string& name,
            VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        SubpassBuilder& setDepthStencilAttachment(const std::string& name,
            VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        SubpassBuilder& setPipelineName(const std::string& pipelineName);

        SubpassBuilder& addPreserveAttachment(const std::string& name);

        // 构建 Subpass（需要名称到索引的映射）
        std::unique_ptr<Subpass> build(const std::unordered_map<std::string, uint32_t>& nameToIndexMap,
            VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

        // 获取使用的附件名称（用于验证）
        const std::vector<std::string>& getColorAttachmentNames() const { return mColorAttachmentNames; }
        const std::vector<std::string>& getInputAttachmentNames() const { return mInputAttachmentNames; }
        const std::vector<std::string>& getResolveAttachmentNames() const { return mResolveAttachmentNames; }
        const std::optional<std::string>& getDepthStencilAttachmentName() const { return mDepthStencilAttachmentName; }
        const std::vector<std::string>& getPreserveAttachmentNames() const { return mPreserveAttachmentNames; }
        const std::string& getPipelineName() const { return mPipelineName; }
		const std::string& getSubpassName() const { return mSubpassName; }
    private:
        struct AttachmentInfo {
            std::string name;
            VkImageLayout layout;
        };

		std::string mSubpassName;
        std::string mPipelineName;

        std::vector<std::string> mColorAttachmentNames;
        std::vector<std::string> mInputAttachmentNames;
        std::vector<std::string> mResolveAttachmentNames;
        std::vector<std::string> mPreserveAttachmentNames;
        std::optional<std::string> mDepthStencilAttachmentName;

        std::unordered_map<std::string, VkImageLayout> mAttachmentLayouts;
    };
} // namespace StarryEngine