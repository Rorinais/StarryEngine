#include"RHI_VK_RESOURCE.hpp"

namespace StarryEngine::RHI {
    void RHI_VK_PipelineLayout::destroy() {
        if (mDevice && mDevice->getLogicalDevice() != VK_NULL_HANDLE) {
            if (mPipelineLayout != VK_NULL_HANDLE) {
                mDevice->destroyPipelineLayout(mPipelineLayout);
                mPipelineLayout = VK_NULL_HANDLE;
            }

            // 销毁描述符集布局
            for (auto layout : mVkDescriptorSetLayouts) {
                vkDestroyDescriptorSetLayout(mDevice->getLogicalDevice(), layout, nullptr);
            }
            mVkDescriptorSetLayouts.clear();
        }
    }

    const std::vector<RHI::DescriptorSetLayoutBinding>& RHI_VK_PipelineLayout::getDescriptorSetLayout(uint32_t set) const  {
        if (set < mDesc.descriptorSets.size()) {
            return mDesc.descriptorSets[set];
        }
        static const std::vector<RHI::DescriptorSetLayoutBinding> empty;
        return empty;
    }

    const RHI::PushConstantRange& RHI_VK_PipelineLayout::getPushConstantRange(uint32_t index) const  {
        if (index < mDesc.pushConstants.size()) {
            return mDesc.pushConstants[index];
        }
        throw std::out_of_range("Push constant range index out of range");
    }

    uint32_t RHI_VK_PipelineLayout::getBindingPoint(uint32_t set, uint32_t binding) const  {
        // 简化实现：返回绑定索引本身
        // 实际实现中可能需要从Vulkan反射数据中获取
        return binding;
    }

    size_t RHI_VK_PipelineLayout::getMemoryUsage() const  {
        size_t size = sizeof(*this);
        // 计算描述符集布局的内存使用
        for (const auto& set : mDesc.descriptorSets) {
            size += set.size() * sizeof(RHI::DescriptorSetLayoutBinding);
        }
        size += mDesc.pushConstants.size() * sizeof(RHI::PushConstantRange);
        return size;
    }

    VkDescriptorSetLayout RHI_VK_PipelineLayout::getVkDescriptorSetLayout(uint32_t set) const {
        if (set < mVkDescriptorSetLayouts.size()) {
            return mVkDescriptorSetLayouts[set];
        }
        return VK_NULL_HANDLE;
    }

    void RHI_VK_PipelineLayout::createPipelineLayout() {
        // 创建Vulkan描述符集布局
        mVkDescriptorSetLayouts.reserve(mDesc.descriptorSets.size());

        for (const auto& descriptorSet : mDesc.descriptorSets) {
            std::vector<VkDescriptorSetLayoutBinding> vkBindings;
            vkBindings.reserve(descriptorSet.size());

            for (const auto& binding : descriptorSet) {
                VkDescriptorSetLayoutBinding vkBinding{};
                vkBinding.binding = binding.binding;
                vkBinding.descriptorType = static_cast<VkDescriptorType>(binding.type);
                vkBinding.descriptorCount = binding.count;
                vkBinding.stageFlags = static_cast<VkShaderStageFlags>(binding.stageFlags);
                vkBinding.pImmutableSamplers = nullptr;

                vkBindings.push_back(vkBinding);
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
            layoutInfo.pBindings = vkBindings.data();

            VkDescriptorSetLayout vkLayout = VK_NULL_HANDLE;
            VkResult result = vkCreateDescriptorSetLayout(
                mDevice->getLogicalDevice(),
                &layoutInfo,
                nullptr,
                &vkLayout
            );

            if (result != VK_SUCCESS) {
                // 清理已创建的所有描述符集布局
                for (auto layout : mVkDescriptorSetLayouts) {
                    vkDestroyDescriptorSetLayout(mDevice->getLogicalDevice(), layout, nullptr);
                }
                mVkDescriptorSetLayouts.clear();

                throw std::runtime_error("Failed to create descriptor set layout");
            }

            mVkDescriptorSetLayouts.push_back(vkLayout);
        }

        // 创建Vulkan推送常量范围
        std::vector<VkPushConstantRange> vkPushConstants;
        vkPushConstants.reserve(mDesc.pushConstants.size());

        for (const auto& range : mDesc.pushConstants) {
            VkPushConstantRange vkRange{};
            vkRange.stageFlags = static_cast<VkShaderStageFlags>(range.stage);
            vkRange.offset = range.offset;
            vkRange.size = range.size;
            vkPushConstants.push_back(vkRange);
        }

        // 创建Vulkan管线布局
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(mVkDescriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = mVkDescriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(vkPushConstants.size());
        pipelineLayoutInfo.pPushConstantRanges = vkPushConstants.data();

        VkResult result = vkCreatePipelineLayout(
            mDevice->getLogicalDevice(),
            &pipelineLayoutInfo,
            nullptr,
            &mPipelineLayout
        );

        if (result != VK_SUCCESS) {
            // 清理已创建的所有描述符集布局
            for (auto layout : mVkDescriptorSetLayouts) {
                vkDestroyDescriptorSetLayout(mDevice->getLogicalDevice(), layout, nullptr);
            }
            mVkDescriptorSetLayouts.clear();

            throw std::runtime_error("Failed to create pipeline layout");
        }
    }

}