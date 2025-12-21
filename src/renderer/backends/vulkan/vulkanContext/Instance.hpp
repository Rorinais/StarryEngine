#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace StarryEngine {

    class Instance {
    public:
        using DebugCallback = std::function<void(
            VkDebugUtilsMessageSeverityFlagBitsEXT,
            VkDebugUtilsMessageTypeFlagsEXT,
            const VkDebugUtilsMessengerCallbackDataEXT*)>;

        struct Config {
            std::string appName = "StarryEngine App";
            uint32_t appVersion = VK_MAKE_VERSION(1, 0, 0);
            std::string engineName = "StarryEngine";
            uint32_t engineVersion = VK_MAKE_VERSION(1, 0, 0);
            uint32_t apiVersion = VK_API_VERSION_1_2;

            bool enableValidation = true;
            std::vector<const char*> requiredExtensions = {};
            std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

            // 调试消息过滤
            VkDebugUtilsMessageSeverityFlagsEXT debugSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

            VkDebugUtilsMessageTypeFlagsEXT debugType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

            DebugCallback debugCallback = nullptr;

            Config() {
#ifdef NDEBUG
                enableValidation = false;
#endif
            }
        };

        using Ptr = std::shared_ptr<Instance>;

        static Ptr create(const Config& config = Config());

        ~Instance();

        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&& other) noexcept;
        Instance& operator=(Instance&& other) noexcept;

        VkInstance getHandle() const { return mInstance; }
        const Config& getConfig() const { return mConfig; }

        VkDebugUtilsMessengerEXT getDebugMessenger() const { return mDebugMessenger; }
        bool hasValidationEnabled() const { return mConfig.enableValidation; }

    private:
        Instance(const Config& config);

        void createInstance();
        void setupDebugMessenger();
        void destroyDebugMessenger();

        std::vector<const char*> getRequiredExtensions() const;
        bool checkValidationLayerSupport() const;

        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData);

    private:
        Config mConfig;
        VkInstance mInstance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;
    };

} // namespace StarryEngine