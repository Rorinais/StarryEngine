#include "RenderPass.hpp"
#include "../vulkanCore/LogicalDevice.hpp" 
#include <stdexcept>

namespace StarryEngine {
	Subpass::Subpass() {}

	Subpass::~Subpass() {}

	void Subpass::addInputAttachment(const VkAttachmentReference& ref) {
		mInputAttachments.push_back(ref);
	}

	void Subpass::addColorAttachment(const VkAttachmentReference& ref) {
		mColorAttachments.push_back(ref);
	}

	void Subpass::addResolveAttachment(const VkAttachmentReference& ref) {
		mResolveAttachments.push_back(ref);
	}

	void Subpass::setDepthStencilAttachment(const VkAttachmentReference& ref) {
		mDepthStencilAttachment = ref;
	}

	void Subpass::biuldSubpassDescription(VkPipelineBindPoint bindPoint) {
		if(mColorAttachments.empty() && mInputAttachments.empty()) {
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

	RenderPass::RenderPass(std::shared_ptr<LogicalDevice> logicalDevice):mLogicalDevice(logicalDevice) {}

	RenderPass::~RenderPass() {
		if(mRenderPass != VK_NULL_HANDLE) {
			vkDestroyRenderPass(mLogicalDevice->getHandle(), mRenderPass, nullptr);
		}
	}
	
	void RenderPass::addAttachment(const VkAttachmentDescription& attachment) {
		mAttachments.push_back(attachment);
	}

	void RenderPass::addSubpass(const Subpass& subpass) {
		mSubpasses.push_back(subpass);
	}

	void RenderPass::addDependency(const VkSubpassDependency& dependency) {
		mDependencies.push_back(dependency);
	}

	void RenderPass::buildRenderPass() {
		std::vector<VkSubpassDescription> subpassDescriptions;
		for (const auto& subpass : mSubpasses) {
			subpassDescriptions.push_back(subpass.getSubpassDescription());
		}

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(mAttachments.size());
		renderPassInfo.pAttachments = mAttachments.data();
		renderPassInfo.subpassCount = static_cast<uint32_t>(mSubpasses.size());
		renderPassInfo.pSubpasses = subpassDescriptions.data();
		renderPassInfo.dependencyCount = static_cast<uint32_t>(mDependencies.size());
		renderPassInfo.pDependencies = mDependencies.data();

		if (vkCreateRenderPass(mLogicalDevice->getHandle(), &renderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}
	}
}