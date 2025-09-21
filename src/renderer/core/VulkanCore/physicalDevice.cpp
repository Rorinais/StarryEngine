#include"physicalDevice.hpp"
#include"../WindowContext/swapchain.hpp"
namespace StarryEngine {
    PhysicalDevice::PhysicalDevice(const Instance::Ptr& instance, VkSurfaceKHR surface)
        : mInstance(instance), mSurface(surface) {
        mPhysicalDevice = selectPhysicalDevice();
        vkGetPhysicalDeviceProperties(mPhysicalDevice, &mProperties);
    }

    PhysicalDevice::~PhysicalDevice() {}

    VkPhysicalDevice PhysicalDevice::selectPhysicalDevice() const {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(mInstance->getHandle(), &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(mInstance->getHandle(), &deviceCount, physicalDevices.data());

        for (const auto& physicalDevice : physicalDevices) {
            if (isSuitable(physicalDevice)) {
                return physicalDevice;
            }
        }
        throw std::runtime_error("Failed to find a suitable GPU!");
    }


    bool PhysicalDevice::isSuitable(VkPhysicalDevice physicalDevice) const {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice, mSurface);
        if (!indices.isComplete()) {
            std::cout << "  Missing required queue families\n";
            return false;
        }
        bool extensionsSupported = checkExtensions(physicalDevice);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = SwapChain::querySwapChainSupport(physicalDevice, mSurface);
            swapChainAdequate = !swapChainSupport.formats.empty() &&
                !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;

    }

    bool PhysicalDevice::checkExtensions(VkPhysicalDevice physicalDevice) const {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    VkFormat PhysicalDevice::findSupportedFormat(const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR &&
                (props.linearTilingFeatures & features) == features) {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("Failed to find supported format!");
    }

    QueueFamilyIndices PhysicalDevice::findQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilies.size(); i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.graphicsFamily == indices.presentFamily) {
                break;
            }
        }

        return indices;
    }
}