#include"DescriptorPool.hpp"
#include"../vulkanCore/VulkanCore.hpp"

namespace StarryEngine {
	DescriptorPool::DescriptorPool(const std::shared_ptr<LogicalDevice>& logicalDevice) :mLogicalDevice(logicalDevice) {}

	void DescriptorPool::initialize(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets, VkDescriptorPoolCreateFlags flags = 0) {
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = flags;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = maxSets;

		if (vkCreateDescriptorPool(mLogicalDevice->getHandle(), &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS)
			throw std::runtime_error("Failed to create descriptor pool");
	}

	DescriptorPool::~DescriptorPool() {
		if (mDescriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(mLogicalDevice->getHandle(), mDescriptorPool, nullptr);
			mDescriptorPool = VK_NULL_HANDLE;
		}
	}
}