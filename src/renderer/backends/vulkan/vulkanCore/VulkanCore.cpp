#include "VulkanCore.hpp"
#include "../WindowContext/WindowContext.hpp"

namespace StarryEngine {
    VulkanCore::VulkanCore() {}

    VulkanCore::~VulkanCore() {
        cleanup();
    }

    void VulkanCore::init(Window::Ptr window) {
        Instance::Config instanceConfig;
        instanceConfig.appName = "Starry Engine";
        instanceConfig.appVersion = VK_MAKE_VERSION(1, 0, 0);
        instanceConfig.apiVersion = VK_API_VERSION_1_3;
        instanceConfig.engineName = "Starry Engine";
        instanceConfig.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        instanceConfig.enableValidation = enableValidationLayers;

        instance = Instance::create(instanceConfig);
        vulkanDebug = VulkanDebug::create(instance);

        createSurface(window);

        physicalDevice = PhysicalDevice::create(instance, mSurface);

        LogicalDevice::Config deviceConfig{};
        deviceConfig.samplerAnisotropy = VK_TRUE;
        deviceConfig.fillModeNonSolid = VK_TRUE;
        deviceConfig.wideLines = VK_TRUE;
        logicalDevice = LogicalDevice::create(physicalDevice, deviceConfig);

        createAllocator();

        mInitialized = true;
    }

    bool VulkanCore::isInitialized() const {
        return mInitialized;
    }

    void VulkanCore::cleanup() {
        // 先销毁VMA分配器
        if (mAllocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(mAllocator);
            mAllocator = VK_NULL_HANDLE;
        }

        logicalDevice.reset();
        physicalDevice.reset();
        vulkanDebug.reset();

        if (mSurface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance->getHandle(), mSurface, nullptr);
            mSurface = VK_NULL_HANDLE;
        }

        instance.reset();
    }

    void VulkanCore::createSurface(Window::Ptr window) {
        if (glfwCreateWindowSurface(
            instance->getHandle(),
            window->getHandle(),
            nullptr,
            &mSurface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface!");
        }
    }

    void VulkanCore::createAllocator() {
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.vulkanApiVersion = instance->getInstanceConfig().apiVersion;
        allocatorInfo.physicalDevice = physicalDevice->getHandle();
        allocatorInfo.device = logicalDevice->getHandle();
        allocatorInfo.instance = instance->getHandle();

        allocatorInfo.flags = 0;

        VkResult result = vmaCreateAllocator(&allocatorInfo, &mAllocator);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create VMA allocator!");
        }
    }
}