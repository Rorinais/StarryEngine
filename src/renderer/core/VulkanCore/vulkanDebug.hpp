#pragma once
#include"instance.hpp"

namespace StarryEngine {
    class VulkanDebug {
    public:

        using Ptr = std::shared_ptr<VulkanDebug>;

        static Ptr create(Instance::Ptr instance) { return std::make_shared<VulkanDebug>(instance); }

        VulkanDebug(Instance::Ptr instance);
        ~VulkanDebug();

        static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    private:
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData);

    private:
        Instance::Ptr mInstance;
        VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;
    };
}
