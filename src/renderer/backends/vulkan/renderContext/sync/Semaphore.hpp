#pragma once
#include"../../VulkanCore/LogicalDevice.hpp"
namespace StarryEngine {

	class Semaphore {
	public:
		using Ptr = std::shared_ptr<Semaphore>;
		static Ptr create(const LogicalDevice::Ptr& logicalDevice) {
			return std::make_shared<Semaphore>(logicalDevice);
		}

		Semaphore(const LogicalDevice::Ptr& logicalDevice);
		~Semaphore();

		VkSemaphore getHandle() { return mSemaphore; }

	private:
		LogicalDevice::Ptr mLogicalDevice;
		VkSemaphore mSemaphore = VK_NULL_HANDLE;

	};

}