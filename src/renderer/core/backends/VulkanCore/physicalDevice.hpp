#pragma once
#include"instance.hpp"
namespace StarryEngine {
    class PhysicalDevice {
    public:
        using Ptr = std::shared_ptr<PhysicalDevice>;
        static Ptr create(const Instance::Ptr& instance, VkSurfaceKHR surface) {
            return std::make_shared<PhysicalDevice>(instance, surface);
        }

        PhysicalDevice(const Instance::Ptr& instance, VkSurfaceKHR surface);
        ~PhysicalDevice();

        VkPhysicalDevice getHandle() const { return mPhysicalDevice; }
        VkPhysicalDeviceProperties getDeviceProperties() const { return mProperties; }
        VkSurfaceKHR getSurface() const { return mSurface; }
        Instance::Ptr getInstance() { return mInstance; }

        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
            VkImageTiling tiling,
            VkFormatFeatureFlags features);

        static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

    private:
        VkPhysicalDevice selectPhysicalDevice() const;
        bool isSuitable(VkPhysicalDevice physicalDevice) const;
        bool checkExtensions(VkPhysicalDevice physicalDevice) const;

        Instance::Ptr mInstance;
        VkPhysicalDeviceProperties mProperties{};
        VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
        VkSurfaceKHR mSurface = VK_NULL_HANDLE;
    };
}