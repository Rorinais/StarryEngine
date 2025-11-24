#pragma once
#include<vulkan/vulkan.h>
#include<vector>
#include<memory>

namespace StarryEngine {
	class Subpass {
	public:
		Subpass();
		~Subpass();
		Subpass& addInputAttachmentRef(const VkAttachmentReference& ref);
		Subpass& addColorAttachmentRef(const VkAttachmentReference& ref);
		Subpass& addResolveAttachmentRef(const VkAttachmentReference& ref);
		Subpass& setDepthStencilAttachmentRef(const VkAttachmentReference& ref);

		Subpass& addColorAttachmentRef(uint32_t attachmentIndex, VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		Subpass& addInputAttachmentRef(uint32_t attachmentIndex, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		Subpass& addPreserveAttachmentRef(uint32_t attachmentIndex);

		void biuldSubpassDescription(VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

		const VkSubpassDescription& getSubpassDescription() const { return mSubpassDescription; }

	private:
		VkSubpassDescription mSubpassDescription{};
		std::vector<VkAttachmentReference> mInputAttachments{};
		std::vector<VkAttachmentReference> mColorAttachments{};
		std::vector<VkAttachmentReference> mResolveAttachments{};
		std::vector<uint32_t> mPreserveAttachments{};
		VkAttachmentReference mDepthStencilAttachment{};
	};
}