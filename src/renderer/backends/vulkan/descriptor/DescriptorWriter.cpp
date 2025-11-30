#include "DescriptorWriter.hpp"
#include "../vulkanCore/VulkanCore.hpp"
#include <stdexcept>

namespace StarryEngine {

    DescriptorWriter::DescriptorWriter(const std::shared_ptr<LogicalDevice>& logicalDevice)
        : mLogicalDevice(logicalDevice) {
    }

    // 单个 Binding 更新实现
    void DescriptorWriter::updateUniformBuffer(
        VkDescriptorSet set,
        uint32_t binding,
        VkBuffer buffer,
        VkDeviceSize offset,
        VkDeviceSize range) {

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer;
        bufferInfo.offset = offset;
        bufferInfo.range = range;

        updateSingleBindingInternal(set, binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufferInfo, nullptr);
    }

    void DescriptorWriter::updateCombinedImageSampler(
        VkDescriptorSet set,
        uint32_t binding,
        VkImageView imageView,
        VkSampler sampler,
        VkImageLayout imageLayout) {

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = imageView;
        imageInfo.sampler = sampler;
        imageInfo.imageLayout = imageLayout;

        updateSingleBindingInternal(set, binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &imageInfo);
    }

    void DescriptorWriter::updateStorageBuffer(
        VkDescriptorSet set,
        uint32_t binding,
        VkBuffer buffer,
        VkDeviceSize offset,
        VkDeviceSize range) {

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer;
        bufferInfo.offset = offset;
        bufferInfo.range = range;

        updateSingleBindingInternal(set, binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo, nullptr);
    }

    void DescriptorWriter::updateImage(
        VkDescriptorSet set,
        uint32_t binding,
        VkImageView imageView,
        VkImageLayout imageLayout) {

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = imageView;
        imageInfo.sampler = VK_NULL_HANDLE;
        imageInfo.imageLayout = imageLayout;

        updateSingleBindingInternal(set, binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, nullptr, &imageInfo);
    }

    void DescriptorWriter::updateSampler(
        VkDescriptorSet set,
        uint32_t binding,
        VkSampler sampler) {

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = sampler;
        imageInfo.imageView = VK_NULL_HANDLE;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        updateSingleBindingInternal(set, binding, VK_DESCRIPTOR_TYPE_SAMPLER, nullptr, &imageInfo);
    }

    // 批量更新实现
    void DescriptorWriter::updateUniformBuffers(
        VkDescriptorSet set,
        const std::vector<uint32_t>& bindings,
        const std::vector<VkBuffer>& buffers,
        const std::vector<VkDeviceSize>& offsets,
        const std::vector<VkDeviceSize>& ranges) {

        if (bindings.size() != buffers.size()) {
            throw std::runtime_error("Bindings and buffers count mismatch");
        }

        std::vector<VkWriteDescriptorSet> writes;
        std::vector<VkDescriptorBufferInfo> bufferInfos;

        for (size_t i = 0; i < bindings.size(); ++i) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = buffers[i];
            bufferInfo.offset = (i < offsets.size()) ? offsets[i] : 0;
            bufferInfo.range = (i < ranges.size()) ? ranges[i] : VK_WHOLE_SIZE;

            bufferInfos.push_back(bufferInfo);

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = set;
            write.dstBinding = bindings[i];
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo = &bufferInfos.back();

            writes.push_back(write);
        }

        if (!writes.empty()) {
            vkUpdateDescriptorSets(mLogicalDevice->getHandle(),
                static_cast<uint32_t>(writes.size()),
                writes.data(), 0, nullptr);
        }
    }

    void DescriptorWriter::updateCombinedImageSamplers(
        VkDescriptorSet set,
        const std::vector<uint32_t>& bindings,
        const std::vector<VkImageView>& imageViews,
        const std::vector<VkSampler>& samplers,
        const std::vector<VkImageLayout>& imageLayouts) {

        if (bindings.size() != imageViews.size() || bindings.size() != samplers.size()) {
            throw std::runtime_error("Bindings, imageViews and samplers count mismatch");
        }

        std::vector<VkWriteDescriptorSet> writes;
        std::vector<VkDescriptorImageInfo> imageInfos;

        for (size_t i = 0; i < bindings.size(); ++i) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageView = imageViews[i];
            imageInfo.sampler = samplers[i];
            imageInfo.imageLayout = (i < imageLayouts.size()) ? imageLayouts[i] : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            imageInfos.push_back(imageInfo);

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = set;
            write.dstBinding = bindings[i];
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo = &imageInfos.back();

            writes.push_back(write);
        }

        if (!writes.empty()) {
            vkUpdateDescriptorSets(mLogicalDevice->getHandle(),
                static_cast<uint32_t>(writes.size()),
                writes.data(), 0, nullptr);
        }
    }

    // 内部通用方法
    void DescriptorWriter::updateSingleBindingInternal(
        VkDescriptorSet set,
        uint32_t binding,
        VkDescriptorType type,
        const VkDescriptorBufferInfo* bufferInfo,
        const VkDescriptorImageInfo* imageInfo) {

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = type;

        if (bufferInfo) {
            write.pBufferInfo = bufferInfo;
        }
        else if (imageInfo) {
            write.pImageInfo = imageInfo;
        }
        else {
            throw std::runtime_error("No resource info provided for descriptor update");
        }

        vkUpdateDescriptorSets(mLogicalDevice->getHandle(), 1, &write, 0, nullptr);
    }
}