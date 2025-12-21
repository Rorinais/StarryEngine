#include "Instance.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <set>
#include <GLFW/glfw3.h>

namespace StarryEngine {

    namespace {
        // 调试工具函数
        VkResult createDebugUtilsMessengerEXT(
            VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
            const VkAllocationCallbacks* pAllocator,
            VkDebugUtilsMessengerEXT* pDebugMessenger) {

            auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                instance, "vkCreateDebugUtilsMessengerEXT");
            return func ? func(instance, pCreateInfo, pAllocator, pDebugMessenger)
                : VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        void destroyDebugUtilsMessengerEXT(
            VkInstance instance,
            VkDebugUtilsMessengerEXT debugMessenger,
            const VkAllocationCallbacks* pAllocator) {

            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                instance, "vkDestroyDebugUtilsMessengerEXT");
            if (func) {
                func(instance, debugMessenger, pAllocator);
            }
        }
    }

    Instance::Ptr Instance::create(const Config& config) {
        return std::make_shared<Instance>(config);
    }

    Instance::Instance(const Config& config) : mConfig(config) {
        // 检查验证层支持
        if (mConfig.enableValidation && !checkValidationLayerSupport()) {
            throw std::runtime_error("Validation layers requested but not available!");
        }

        // 创建实例
        createInstance();

        // 设置调试消息器
        if (mConfig.enableValidation) {
            setupDebugMessenger();
        }
    }

    Instance::~Instance() {
        destroyDebugMessenger();

        if (mInstance != VK_NULL_HANDLE) {
            vkDestroyInstance(mInstance, nullptr);
            mInstance = VK_NULL_HANDLE;
        }
    }

    Instance::Instance(Instance&& other) noexcept
        : mConfig(std::move(other.mConfig))
        , mInstance(other.mInstance)
        , mDebugMessenger(other.mDebugMessenger) {

        other.mInstance = VK_NULL_HANDLE;
        other.mDebugMessenger = VK_NULL_HANDLE;
    }

    Instance& Instance::operator=(Instance&& other) noexcept {
        if (this != &other) {
            destroyDebugMessenger();

            if (mInstance != VK_NULL_HANDLE) {
                vkDestroyInstance(mInstance, nullptr);
            }

            mConfig = std::move(other.mConfig);
            mInstance = other.mInstance;
            mDebugMessenger = other.mDebugMessenger;

            other.mInstance = VK_NULL_HANDLE;
            other.mDebugMessenger = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Instance::createInstance() {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = mConfig.appName.c_str();
        appInfo.applicationVersion = mConfig.appVersion;
        appInfo.pEngineName = mConfig.engineName.c_str();
        appInfo.engineVersion = mConfig.engineVersion;
        appInfo.apiVersion = mConfig.apiVersion;

        // 获取所需扩展
        auto extensions = getRequiredExtensions();

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // 调试消息器创建信息（链接到pNext）
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (mConfig.enableValidation) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(mConfig.validationLayers.size());
            createInfo.ppEnabledLayerNames = mConfig.validationLayers.data();

            // 填充调试信息
            debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugCreateInfo.messageSeverity = mConfig.debugSeverity;
            debugCreateInfo.messageType = mConfig.debugType;
            debugCreateInfo.pfnUserCallback = debugCallback;
            debugCreateInfo.pUserData = this;

            createInfo.pNext = &debugCreateInfo;
        }else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance!");
        }
    }

    void Instance::setupDebugMessenger() {
        if (!mConfig.enableValidation) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = mConfig.debugSeverity;
        createInfo.messageType = mConfig.debugType;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pUserData = this;

        if (createDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to set up debug messenger!");
        }
    }

    void Instance::destroyDebugMessenger() {
        if (mDebugMessenger != VK_NULL_HANDLE) {
            destroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
            mDebugMessenger = VK_NULL_HANDLE;
        }
    }

    std::vector<const char*> Instance::getRequiredExtensions() const {
        std::vector<const char*> extensions;

        // GLFW 所需扩展
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        extensions.insert(extensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);

        // 调试扩展
        if (mConfig.enableValidation) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        // 用户指定扩展
        extensions.insert(extensions.end(),
            mConfig.requiredExtensions.begin(),
            mConfig.requiredExtensions.end());

        return extensions;
    }

    bool Instance::checkValidationLayerSupport() const {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : mConfig.validationLayers) {
            bool found = false;
            for (const auto& layer : availableLayers) {
                if (strcmp(layerName, layer.layerName) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::cerr << "[ERROR] Validation layer not found: " << layerName << std::endl;
                return false;
            }
        }
        return true;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL Instance::debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

        auto instance = static_cast<Instance*>(pUserData);

        // 使用自定义回调（如果提供）
        if (instance->mConfig.debugCallback) {
            instance->mConfig.debugCallback(messageSeverity, messageType, pCallbackData);
            return VK_FALSE;
        }

        // 默认处理
        const char* prefix = "";
        switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            prefix = "[VERBOSE]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            prefix = "[INFO]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            prefix = "[WARNING]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            prefix = "[ERROR]";
            break;
        }

        std::cerr << prefix << " " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

} // namespace StarryEngine