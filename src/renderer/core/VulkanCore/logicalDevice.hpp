#pragma once
#include "physicalDevice.hpp"
namespace StarryEngine {
    class LogicalDevice;

    class LogicalDevice {
    public:
        struct Config {
            VkBool32 samplerAnisotropy = VK_FALSE;
            VkBool32 geometryShader = VK_FALSE;
            VkBool32 tessellationShader = VK_FALSE;
            VkBool32 fillModeNonSolid = VK_FALSE;
            VkBool32 wideLines = VK_FALSE;
        };

        struct QueueHandles {
            VkQueue graphicsQueue = VK_NULL_HANDLE;
            VkQueue presentQueue = VK_NULL_HANDLE;
        };

        using Ptr = std::shared_ptr<LogicalDevice>;

        static Ptr create(const PhysicalDevice::Ptr& physicalDevice, LogicalDevice::Config config) {
            return std::make_shared<LogicalDevice>(physicalDevice, config);
        }

        LogicalDevice(const PhysicalDevice::Ptr& physicalDevice, Config config);
        ~LogicalDevice();

        PhysicalDevice::Ptr getPhysicalDevice() const { return mPhysicalDevice; }
        VkDevice getHandle()const { return mLogicalDevice; }
        QueueHandles getQueueHandles()const { return mQueues; }

    private:
        Config mConfig;
        PhysicalDevice::Ptr mPhysicalDevice;
        VkDevice mLogicalDevice = VK_NULL_HANDLE;
        QueueHandles mQueues{};
    };
}
