#include "SubPass.hpp"
#include <stdexcept>

namespace StarryEngine {
	Subpass::Subpass() {}

	Subpass::~Subpass() {}

	Subpass& Subpass::addInputAttachmentRef(const VkAttachmentReference& ref) {
		mInputAttachments.push_back(ref);
		return *this;
	}

	Subpass& Subpass::addColorAttachmentRef(const VkAttachmentReference& ref) {
		mColorAttachments.push_back(ref);
		return *this;
	}

	Subpass& Subpass::addResolveAttachmentRef(const VkAttachmentReference& ref) {
		mResolveAttachments.push_back(ref);
		return *this;
	}

	Subpass& Subpass::setDepthStencilAttachmentRef(const VkAttachmentReference& ref) {
		mDepthStencilAttachment = ref;
		return *this;
	}

	Subpass& Subpass::addColorAttachmentRef(uint32_t attachmentIndex, VkImageLayout layout) {
		VkAttachmentReference ref{};
		ref.attachment = attachmentIndex;
		ref.layout = layout;
		return addColorAttachmentRef(ref);
	}

	Subpass& Subpass::addInputAttachmentRef(uint32_t attachmentIndex, VkImageLayout layout) {
		VkAttachmentReference ref{};
		ref.attachment = attachmentIndex;
		ref.layout = layout;
		return addInputAttachmentRef(ref);
	}

	Subpass& Subpass::addPreserveAttachmentRef(uint32_t attachmentIndex) {
		mPreserveAttachments.push_back(attachmentIndex);
		return *this;
	}

	void Subpass::biuldSubpassDescription(VkPipelineBindPoint bindPoint) {
		if (mColorAttachments.empty() && mInputAttachments.empty()) {
			throw std::runtime_error("Subpass must have at least one color or input attachment.");
		}

		mSubpassDescription.pipelineBindPoint = bindPoint;

		mSubpassDescription.colorAttachmentCount = static_cast<uint32_t>(mColorAttachments.size());
		mSubpassDescription.pColorAttachments = mColorAttachments.data();

		mSubpassDescription.inputAttachmentCount = static_cast<uint32_t>(mInputAttachments.size());
		mSubpassDescription.pInputAttachments = mInputAttachments.data();

		mSubpassDescription.pResolveAttachments = mResolveAttachments.empty() ? nullptr : mResolveAttachments.data();

		mSubpassDescription.pDepthStencilAttachment = (mDepthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED) ? &mDepthStencilAttachment : nullptr;
	}
}