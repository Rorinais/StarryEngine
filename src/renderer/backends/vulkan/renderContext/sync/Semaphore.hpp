#pragma once
#include<vulkan/vulkan.h>
#include<memory>
#include<stdexcept>

namespace StarryEngine {

	class Semaphore {
	public:
		using Ptr = std::shared_ptr<Semaphore>;
		static Ptr create(VkDevice device) {
			return std::make_shared<Semaphore>(device);
		}

		Semaphore(VkDevice device);
		~Semaphore();

		VkSemaphore getHandle() { return mSemaphore; }

	private:
		VkDevice mDevice;
		VkSemaphore mSemaphore = VK_NULL_HANDLE;

	};

}