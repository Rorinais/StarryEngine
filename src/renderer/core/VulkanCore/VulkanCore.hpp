#pragma once
#include "../../../core/platform/window.hpp"
#include "instance.hpp"
#include "logicalDevice.hpp"
#include "physicalDevice.hpp"
#include "vulkanDebug.hpp"

namespace StarryEngine {
    const int MAX_FRAMES_IN_FLIGHT = 2;

    class WindowContext; 

    class VulkanCore {
    public:
        using Ptr = std::shared_ptr<VulkanCore>;
        static Ptr create() { return std::make_shared<VulkanCore>(); }

        VulkanCore();
        ~VulkanCore();

        void init(Window::Ptr window);
        void cleanup();

        Instance::Ptr getInstance() const { return instance; }
        PhysicalDevice::Ptr getPhysicalDevice() const { return physicalDevice; }
        LogicalDevice::Ptr getLogicalDevice() const { return logicalDevice; }

        VkPhysicalDevice getPhysicalDeviceHandle() const { return physicalDevice->getHandle(); }
        VkInstance getInstanceHandle() const { return instance->getHandle(); }
        VkDevice getLogicalDeviceHandle() const { return logicalDevice->getHandle(); }
        VkQueue getGraphicsQueue() const { return logicalDevice->getQueueHandles().graphicsQueue; }
        VkQueue getPresentQueue() const { return logicalDevice->getQueueHandles().presentQueue; }

        VkSurfaceKHR getSurface() const { return mSurface; }

        bool isInitialized() const;

    private:
        void createSurface(Window::Ptr window);

        bool mInitialized = false;
        Instance::Ptr instance;
        PhysicalDevice::Ptr physicalDevice;
        LogicalDevice::Ptr logicalDevice;
        VulkanDebug::Ptr vulkanDebug;

        VkSurfaceKHR mSurface = VK_NULL_HANDLE;
    };
}