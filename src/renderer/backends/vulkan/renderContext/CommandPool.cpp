#include"CommandPool.hpp"
namespace StarryEngine {
	CommandPool::CommandPool(const LogicalDevice::Ptr& logicalDevice, VkCommandPoolCreateFlagBits flag)
		:mLogicalDevice(logicalDevice) {
		VkCommandPoolCreateInfo poolCreateInfo{};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolCreateInfo.flags = flag;

		auto physicalDevice = mLogicalDevice->getPhysicalDevice();
		auto queueFamilyIndex = physicalDevice->findQueueFamilies(physicalDevice->getHandle(), mLogicalDevice->getSurface());
		poolCreateInfo.queueFamilyIndex = queueFamilyIndex.graphicsFamily.value();

		if (vkCreateCommandPool(mLogicalDevice->getHandle(), &poolCreateInfo, nullptr, &mCommandPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");

		}
	}
	CommandPool::~CommandPool() {
		if (mCommandPool != VK_NULL_HANDLE) {
			vkDestroyCommandPool(mLogicalDevice->getHandle(), mCommandPool, nullptr);
		}
	}

}