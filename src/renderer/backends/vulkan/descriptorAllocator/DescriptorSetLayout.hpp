#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <stdexcept>

//binding && decinfo

namespace StarryEngine {

	void test() {
		VkDescriptorSetLayoutBinding layoutBinding1{};
		layoutBinding1.binding = 0;
		layoutBinding1.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBinding1.descriptorCount = 1;
		layoutBinding1.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layoutBinding1.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutBinding layoutBinding2{};
		layoutBinding2.binding = 1;
		layoutBinding2.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBinding2.descriptorCount = 1;
		layoutBinding2.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layoutBinding2.pImmutableSamplers = nullptr;

		std::vector<VkDescriptorSetLayoutBinding> desclayout;

		desclayout.push_back(layoutBinding1);
		desclayout.push_back(layoutBinding2);
		

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = desclayout.size();
		layoutInfo.pBindings = desclayout.data();
		layoutInfo.flags = 0;
		layoutInfo.pNext = nullptr;

		VkDescriptorSetLayout Layout;
		
		if (vkCreateDescriptorSetLayout(Device,&layoutInfo,nullptr,&Layout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor set layout!");
		}


	}



	class LogicalDevice;

	class DescriptorSetLayout {
	public:


	private:
		std::shared_ptr<LogicalDevice> mLogicalDevice;
		std::vector<VkDescriptorSetLayoutBinding> mBindings;
		VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;
	};
}

