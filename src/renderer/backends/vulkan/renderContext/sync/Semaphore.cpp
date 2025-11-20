#include"Semaphore.hpp"
namespace StarryEngine {

	Semaphore::Semaphore(const LogicalDevice::Ptr& logicalDevice) :mLogicalDevice(logicalDevice) {
		VkSemaphoreCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;

		if (vkCreateSemaphore(mLogicalDevice->getHandle(), &createInfo, nullptr, &mSemaphore) != VK_SUCCESS) {
			throw std::runtime_error("failed to create semaphore!");
		}
	}

	Semaphore::~Semaphore() {
		if (mSemaphore != VK_NULL_HANDLE) {
			vkDestroySemaphore(mLogicalDevice->getHandle(), mSemaphore, nullptr);
		}
	}

}