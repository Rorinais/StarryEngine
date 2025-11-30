#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace StarryEngine {
	class LogicalDevice;

	class DescriptorSetLayout {
	public:
		using Ptr = std::shared_ptr<DescriptorSetLayout>;
		
		Ptr create(const std::shared_ptr<LogicalDevice>& logicalDevice) {
			return std::make_shared<DescriptorSetLayout>(logicalDevice);
		}
		DescriptorSetLayout(const std::shared_ptr<LogicalDevice>& logicalDevice) :mLogicalDevice(logicalDevice) {}
		~DescriptorSetLayout();

		void addBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t descriptorCount = 1, const VkSampler* sampler = nullptr);

		void build(VkDescriptorSetLayoutCreateFlags flags = 0, const VkDescriptorSetLayoutBindingFlagsCreateInfo* pNext = nullptr);

		VkDescriptorSetLayout getHandle() { return mDescriptorSetLayout; }
		std::vector<VkDescriptorSetLayoutBinding> getBindings() { return mBindings; }

		bool isBuilt() const { return mIsBuilt; }

	private:
		std::shared_ptr<LogicalDevice> mLogicalDevice;
		std::vector<VkDescriptorSetLayoutBinding> mBindings;
		VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;

		bool mIsBuilt = false;
	};
}

