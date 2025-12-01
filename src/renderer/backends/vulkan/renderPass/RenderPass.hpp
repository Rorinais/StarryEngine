#pragma once
#include"Subpass.hpp"

namespace StarryEngine {
	class LogicalDevice;

	class RenderPass {
	public:
		using Ptr = std::shared_ptr<RenderPass>;
		static Ptr create(std::shared_ptr<LogicalDevice> logicalDevice) {
			return std::make_shared<RenderPass>(logicalDevice);
		}
		RenderPass(std::shared_ptr<LogicalDevice> logicalDevice);

		~RenderPass();

		void addAttachment(const VkAttachmentDescription& attachment);

		void addSubpass(std::unique_ptr<Subpass> subpass);

		void addDependency(const VkSubpassDependency& dependency);

		void buildRenderPass();

		VkRenderPass getHandle() const { return mRenderPass; }

	private:
		std::shared_ptr<LogicalDevice> mLogicalDevice;

		VkRenderPass mRenderPass = VK_NULL_HANDLE;
		std::vector<std::unique_ptr<Subpass>> mSubpasses{};
		std::vector<VkAttachmentDescription> mAttachments{};
		std::vector<VkSubpassDependency> mDependencies{};

	};
}