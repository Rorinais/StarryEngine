#include"Semaphore.hpp"
namespace StarryEngine {

	Semaphore::Semaphore(VkDevice device) :mDevice(device) {
		VkSemaphoreCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;

		if (vkCreateSemaphore(mDevice, &createInfo, nullptr, &mSemaphore) != VK_SUCCESS) {
			throw std::runtime_error("failed to create semaphore!");
		}
	}

	Semaphore::~Semaphore() {
		if (mSemaphore != VK_NULL_HANDLE) {
			vkDestroySemaphore(mDevice, mSemaphore, nullptr);
		}
	}

}