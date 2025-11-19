#pragma once
#include"../../VulkanCore/LogicalDevice.hpp"
namespace StarryEngine {
	class Fence {
	public:
		using Ptr = std::shared_ptr<Fence>;
		static Ptr create(const LogicalDevice::Ptr& logicalDevice, bool signaled = true) {
			return std::make_shared<Fence>(logicalDevice, signaled);
		}

		Fence(const LogicalDevice::Ptr& logicalDevice, bool signaled = true);
		~Fence();

		void resetFence();

		void block(uint64_t timeout = UINT64_MAX);

		VkFence getHandle() { return mFence; }

	private:
		LogicalDevice::Ptr mLogicalDevice;
		VkFence mFence = VK_NULL_HANDLE;

	};
}