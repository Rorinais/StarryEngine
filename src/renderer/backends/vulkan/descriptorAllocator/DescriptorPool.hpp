#pragma once
#include<vulkan/vulkan.h>
#include<memory>
#include<vector>

namespace StarryEngine {
	class LogicalDevice;

	class DescriptorPool {
	public:
		using Ptr = std::shared_ptr<DescriptorPool>;
		static Ptr create(const std::shared_ptr<LogicalDevice>& logicalDevice) {
			return std::make_shared<DescriptorPool>(logicalDevice);
		}

		DescriptorPool(const std::shared_ptr <LogicalDevice>& logicalDevice);
		~DescriptorPool();

		void initialize(const std::vector<VkDescriptorPoolSize>& poolSizes,uint32_t maxSets,VkDescriptorPoolCreateFlags flags = 0);

		VkDescriptorPool getHandle() { return mDescriptorPool; }
		std::shared_ptr<LogicalDevice> getLogicalDevice() { return mLogicalDevice; }

	private:
		std::shared_ptr<LogicalDevice> mLogicalDevice;
		VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
	};
}
