#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace StarryEngine {
    class LogicalDevice;

    class DescriptorWriter {
    public:
        using Ptr = std::shared_ptr<DescriptorWriter>;

        DescriptorWriter(const std::shared_ptr<LogicalDevice>& logicalDevice);

        // === 核心：单个 Binding 更新 ===

        // 更新单个 Uniform Buffer Binding
        void updateUniformBuffer(
            VkDescriptorSet set,
            uint32_t binding,
            VkBuffer buffer,
            VkDeviceSize offset = 0,
            VkDeviceSize range = VK_WHOLE_SIZE);

        // 更新单个 Combined Image Sampler Binding  
        void updateCombinedImageSampler(
            VkDescriptorSet set,
            uint32_t binding,
            VkImageView imageView,
            VkSampler sampler,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // 更新单个 Storage Buffer Binding
        void updateStorageBuffer(
            VkDescriptorSet set,
            uint32_t binding,
            VkBuffer buffer,
            VkDeviceSize offset = 0,
            VkDeviceSize range = VK_WHOLE_SIZE);

        // 更新单个 Image Binding
        void updateImage(
            VkDescriptorSet set,
            uint32_t binding,
            VkImageView imageView,
            VkImageLayout imageLayout);

        // 更新单个 Sampler Binding
        void updateSampler(
            VkDescriptorSet set,
            uint32_t binding,
            VkSampler sampler);

        // === 批量更新：同一个 Set 的多个 Binding ===

        // 批量更新 Uniform Buffers
        void updateUniformBuffers(
            VkDescriptorSet set,
            const std::vector<uint32_t>& bindings,
            const std::vector<VkBuffer>& buffers,
            const std::vector<VkDeviceSize>& offsets = {},
            const std::vector<VkDeviceSize>& ranges = {});

        // 批量更新 Image Samplers
        void updateCombinedImageSamplers(
            VkDescriptorSet set,
            const std::vector<uint32_t>& bindings,
            const std::vector<VkImageView>& imageViews,
            const std::vector<VkSampler>& samplers,
            const std::vector<VkImageLayout>& imageLayouts = {});

    private:
        std::shared_ptr<LogicalDevice> mLogicalDevice;

        // 内部通用更新方法
        void updateSingleBindingInternal(
            VkDescriptorSet set,
            uint32_t binding,
            VkDescriptorType type,
            const VkDescriptorBufferInfo* bufferInfo = nullptr,
            const VkDescriptorImageInfo* imageInfo = nullptr);
    };
}