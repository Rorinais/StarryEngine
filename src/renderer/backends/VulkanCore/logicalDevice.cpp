#include"logicalDevice.hpp"
namespace StarryEngine {
	LogicalDevice::LogicalDevice(const PhysicalDevice::Ptr& physicalDevice, LogicalDevice::Config config) :
		mPhysicalDevice(physicalDevice), mConfig(config) {
		auto queueIndices = PhysicalDevice::findQueueFamilies(mPhysicalDevice->getHandle(), mPhysicalDevice->getSurface());

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = {};
		std::set<uint32_t> uniqueQueueFamilies = { queueIndices.graphicsFamily.value(), queueIndices.presentFamily.value() };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = mConfig.samplerAnisotropy;
		deviceFeatures.geometryShader = mConfig.geometryShader;
		deviceFeatures.tessellationShader = mConfig.tessellationShader;
		deviceFeatures.fillModeNonSolid = mConfig.fillModeNonSolid;
		deviceFeatures.wideLines = mConfig.wideLines;

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();

		createInfo.pEnabledFeatures = &deviceFeatures;

		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();

		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		vkCreateDevice(physicalDevice->getHandle(), &createInfo, nullptr, &mLogicalDevice);

		vkGetDeviceQueue(mLogicalDevice, queueIndices.graphicsFamily.value(), 0, &mQueues.graphicsQueue);
		vkGetDeviceQueue(mLogicalDevice, queueIndices.presentFamily.value(), 0, &mQueues.presentQueue);
	}

	LogicalDevice::~LogicalDevice() {
		if (mLogicalDevice != VK_NULL_HANDLE) {
			vkDestroyDevice(mLogicalDevice, nullptr);
		}
	}
}
