	#pragma once
#include "commandPool.hpp"
namespace StarryEngine {
	class CommandBuffer {
	public:
		using Ptr = std::shared_ptr<CommandBuffer>;
		static Ptr create(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool, bool asSecondary = false) {
			return std::make_shared<CommandBuffer>(logicalDevice, commandPool, asSecondary);
		}
		CommandBuffer(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool, bool asSecondary);

		~CommandBuffer();

		void reset(VkCommandBufferResetFlags flags = 0);

		void begin(VkCommandBufferUsageFlags flag = 0, const VkCommandBufferInheritanceInfo& inheritance = {});

		void beginRenderPass(const VkRenderPassBeginInfo& renderPassBeginInfo, const VkSubpassContents& subpassContents = VK_SUBPASS_CONTENTS_INLINE);

		void bindGraphicPipeline(const VkPipeline& pipeline);

		void bindDescriptorSets(const VkPipelineLayout& pipelineLayout, uint32_t firstSet, const std::vector<VkDescriptorSet>& descriptorSets);

		void bindDescriptorSets(const VkPipelineLayout& pipelineLayout, uint32_t firstSet, VkDescriptorSet descriptorSets);

		void setViewport(const VkExtent2D& extent, bool isOpenglCoord = true);

		void setScissor(const VkExtent2D& extent);

		void bindVertexBuffers(const std::vector<VkBuffer>& vertexBuffers);

		void bindIndexBuffer(const VkBuffer& indexBuffers);

		void drawIndexed(const uint32_t indexCount);

		void draw(size_t vertexCount);

		void endRenderPass();

		void end();

		void executeCommands(const std::vector<VkCommandBuffer>& commandBuffers);
		bool isRecording() const;

		VkCommandBuffer getHandle() { return mCommandBuffer; }
		const CommandPool::Ptr& getCommandPool() { return mCommandPool; }

	private:
		LogicalDevice::Ptr mLogicalDevice;
		CommandPool::Ptr mCommandPool;

		VkCommandBuffer mCommandBuffer = VK_NULL_HANDLE;
	};
}

