#pragma once
#include<vulkan/vulkan.h>
#include<memory>
#include<stdexcept>

namespace StarryEngine {
	class Fence {
	public:
		using Ptr = std::shared_ptr<Fence>;
		static Ptr create(VkDevice device, bool signaled = true) {
			return std::make_shared<Fence>(device, signaled);
		}

		Fence(VkDevice device, bool signaled);
		~Fence();

		void resetFence();

		void block(uint64_t timeout = UINT64_MAX);

		VkFence getHandle() { return mFence; }

	private:
		VkDevice mDevice;
		VkFence mFence = VK_NULL_HANDLE;

	};
}