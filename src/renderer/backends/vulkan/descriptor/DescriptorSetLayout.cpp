#include"DescriptorSetLayout.hpp"
#include"../vulkanCore/VulkanCore.hpp"
#include <stdexcept>

namespace StarryEngine {
    DescriptorSetLayout::~DescriptorSetLayout() {
        if (mDescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(mLogicalDevice->getHandle(), mDescriptorSetLayout, nullptr);
            mDescriptorSetLayout = VK_NULL_HANDLE;
        }
    }

    void DescriptorSetLayout::addBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t descriptorCount, const VkSampler* sampler) {
        if (mIsBuilt) {
            throw std::runtime_error("Cannot add binding to a built DescriptorSetLayout");
        }

        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = binding;
        layoutBinding.descriptorType = descriptorType;
        layoutBinding.descriptorCount = descriptorCount;
        layoutBinding.stageFlags = stageFlags;
        layoutBinding.pImmutableSamplers = sampler;
        mBindings.push_back(layoutBinding);
    }

    void DescriptorSetLayout::build(VkDescriptorSetLayoutCreateFlags flags, const VkDescriptorSetLayoutBindingFlagsCreateInfo* pNext) {
        if (mIsBuilt) {
            throw std::runtime_error("DescriptorSetLayout already built");
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(mBindings.size());
        layoutInfo.pBindings = mBindings.data();
        layoutInfo.flags = flags;
        layoutInfo.pNext = pNext;

        if (vkCreateDescriptorSetLayout(mLogicalDevice->getHandle(), &layoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }

        mIsBuilt = true; 
    }
}