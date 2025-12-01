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

		void end();

		VkCommandBuffer getHandle() { return mCommandBuffer; }
		const CommandPool::Ptr& getCommandPool() { return mCommandPool; }

	private:
		LogicalDevice::Ptr mLogicalDevice;
		CommandPool::Ptr mCommandPool;

		VkCommandBuffer mCommandBuffer = VK_NULL_HANDLE;
	};
}

