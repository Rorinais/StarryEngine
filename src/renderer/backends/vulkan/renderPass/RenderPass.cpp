#include "RenderPass.hpp"
#include "../vulkanCore/LogicalDevice.hpp" 
#include <stdexcept>

namespace StarryEngine {
	RenderPass::RenderPass(std::shared_ptr<LogicalDevice> logicalDevice):mLogicalDevice(logicalDevice) {}

	RenderPass::~RenderPass() {
		if(mRenderPass != VK_NULL_HANDLE) {
			vkDestroyRenderPass(mLogicalDevice->getHandle(), mRenderPass, nullptr);
		}
	}
	
	void RenderPass::addAttachment(const VkAttachmentDescription& attachment) {
		mAttachments.push_back(attachment);
	}

	void RenderPass::addSubpass(std::unique_ptr<Subpass> subpass) {
		// 接收所有权，转移 unique_ptr
		mSubpasses.push_back(std::move(subpass));
	}

	void RenderPass::addDependency(const VkSubpassDependency& dependency) {
		mDependencies.push_back(dependency);
	}

	void RenderPass::buildRenderPass() {
		std::vector<VkSubpassDescription> subpassDescriptions;
		for (const auto& subpass : mSubpasses) {
			subpassDescriptions.push_back(subpass->getSubpassDescription());
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