#include"instance.hpp"
#include"vulkanDebug.hpp"
namespace StarryEngine {

	Instance::Instance(const Instance::Config& config) :mConfig(config) {
		if (mConfig.enableValidation && !checkValidationSupport()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.applicationVersion = mConfig.appVersion;
		appInfo.pApplicationName = mConfig.appName;
		appInfo.pEngineName = mConfig.engineName;
		appInfo.engineVersion = mConfig.engineVersion;
		appInfo.apiVersion = mConfig.apiVersion;

		auto extensions = getRequiredExtensions();

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (mConfig.enableValidation) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
			VulkanDebug::populateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = &debugCreateInfo;
		}
		else {
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}
		if (vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}
	}
	Instance::~Instance() {
		if (mInstance != VK_NULL_HANDLE) {
			vkDestroyInstance(mInstance, nullptr);
			mInstance = VK_NULL_HANDLE;
		}
	}

	bool Instance::checkValidationSupport() const {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		bool allLayersFound = true;
		for (const char* layerName : validationLayers) {
			bool found = false;
			for (const auto& layer : availableLayers) {
				if (strcmp(layerName, layer.layerName) == 0) {
					found = true;
					break;
				}
			}
			if (!found) {
				std::cerr << "[ERROR] Layer not found: " << layerName << std::endl;
				allLayersFound = false;
			}
		}
		return allLayersFound;
	}

	std::vector<const char*> Instance::getRequiredExtensions() const {
		std::vector<const char*> extensions;

		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		extensions.insert(extensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);

		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		return extensions;
	}

}