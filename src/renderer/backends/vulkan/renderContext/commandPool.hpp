#pragma once
#include"../VulkanCore/LogicalDevice.hpp"
namespace StarryEngine {
	class CommandPool {
	public:
		using Ptr = std::shared_ptr<CommandPool>;
		static Ptr create(const LogicalDevice::Ptr& logicalDevice, VkCommandPoolCreateFlagBits flag = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) {
			return std::make_shared<CommandPool>(logicalDevice, flag);
		}

		CommandPool(const LogicalDevice::Ptr& logicalDevice, VkCommandPoolCreateFlagBits flag = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		~CommandPool();

		VkCommandPool getHandle() { return mCommandPool; }
	private:
		LogicalDevice::Ptr mLogicalDevice;
		VkCommandPool mCommandPool = VK_NULL_HANDLE;
	};
}

