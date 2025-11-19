#pragma once
#include<vulkan/vulkan.h>
#include<vector>
#include<memory>

namespace StarryEngine {
	class LogicalDevice;

	class Subpass {
	public:
		Subpass();
		~Subpass();
		void addInputAttachment(const VkAttachmentReference& ref);
		void addColorAttachment(const VkAttachmentReference& ref);
		void addResolveAttachment(const VkAttachmentReference& ref);
		void setDepthStencilAttachment(const VkAttachmentReference& ref);

		void biuldSubpassDescription(VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

		const VkSubpassDescription& getSubpassDescription() const { return mSubpassDescription; }

	private:
		VkSubpassDescription mSubpassDescription;
		std::vector<VkAttachmentReference> mInputAttachments{};
		std::vector<VkAttachmentReference> mColorAttachments{};
		std::vector<VkAttachmentReference> mResolveAttachments{};
		VkAttachmentReference mDepthStencilAttachment{};
	};

	class RenderPass {
	public:
		using Ptr = std::shared_ptr<RenderPass>;
		static Ptr create(std::shared_ptr<LogicalDevice> logicalDevice) {
			return std::make_shared<RenderPass>(logicalDevice);
		}
		RenderPass(std::shared_ptr<LogicalDevice> logicalDevice);

		~RenderPass();

		void addAttachment(const VkAttachmentDescription& attachment);

		void addSubpass(const Subpass& subpass);

		void addDependency(const VkSubpassDependency& dependency);

		void buildRenderPass();

	private:
		std::shared_ptr<LogicalDevice> mLogicalDevice;

		VkRenderPass mRenderPass = VK_NULL_HANDLE;
		std::vector<Subpass> mSubpasses{};
		std::vector<VkAttachmentDescription> mAttachments{};
		std::vector<VkSubpassDependency> mDependencies{};

	};
}