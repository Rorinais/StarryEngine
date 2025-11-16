#pragma once
#include"../../../base.hpp"
namespace StarryEngine {
	class Instance {
	public:
		struct Config {
			const char* appName = "Vulkan App";
			uint32_t appVersion = VK_MAKE_VERSION(1, 0, 0);
			const char* engineName = "No Engine";
			uint32_t engineVersion = VK_MAKE_VERSION(1, 0, 0);
			uint32_t apiVersion = VK_API_VERSION_1_2;
			bool enableValidation = true;
		};

		using Ptr = std::shared_ptr<Instance>;
		static Ptr create(const Instance::Config& config) { return std::make_shared<Instance>(config); }

		Instance(const Instance::Config& config);
		~Instance();

		VkInstance getHandle()const { return mInstance; }

		Instance::Config& getInstanceConfig() { return mConfig; }

	private:
		bool checkValidationSupport() const;

		std::vector<const char*> getRequiredExtensions() const;

	private:
		Config mConfig{};

		VkInstance mInstance = VK_NULL_HANDLE;
	};
}
