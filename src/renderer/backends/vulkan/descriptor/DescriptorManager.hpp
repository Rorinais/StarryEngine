#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include "DescriptorAllocator.hpp"
#include "DescriptorWriter.hpp"
#include "DescriptorSetLayout.hpp"
#include "DescriptorTracker.hpp"

namespace StarryEngine {

    class DescriptorManager {
    public:
        using Ptr = std::shared_ptr<DescriptorManager>;

        DescriptorManager(const std::shared_ptr<LogicalDevice>& logicalDevice);
        ~DescriptorManager() = default;

        // === 布局定义阶段 ===
        void beginSetLayout(uint32_t setIndex);
        void addUniformBuffer(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count = 1);
        void addCombinedImageSampler(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count = 1);
        void addStorageBuffer(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count = 1);
        void addImage(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count = 1);
        void addSampler(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count = 1);
        void endSetLayout();

        // === 分配阶段 ===
        void allocateSets(uint32_t setCount = 2);
        void freeSets(bool isClearAllocator = false);

        // === 更新阶段 ===
        // 更新指定帧的描述符集
        void updateUniformBuffer(uint32_t setIndex, uint32_t binding, uint32_t frameIndex,
            VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);

        void updateCombinedImageSampler(uint32_t setIndex, uint32_t binding, uint32_t frameIndex,
            VkImageView imageView, VkSampler sampler,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        void updateStorageBuffer(uint32_t setIndex, uint32_t binding, uint32_t frameIndex,
            VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);

        void updateImage(uint32_t setIndex, uint32_t binding, uint32_t frameIndex,
            VkImageView imageView, VkImageLayout imageLayout);

        void updateSampler(uint32_t setIndex, uint32_t binding, uint32_t frameIndex, VkSampler sampler);

        // 批量更新多个 Binding（同一帧）
        void updateUniformBuffers(uint32_t setIndex, uint32_t frameIndex,
            const std::vector<uint32_t>& bindings,
            const std::vector<VkBuffer>& buffers,
            const std::vector<VkDeviceSize>& offsets = {},
            const std::vector<VkDeviceSize>& ranges = {});

        void updateCombinedImageSamplers(uint32_t setIndex, uint32_t frameIndex,
            const std::vector<uint32_t>& bindings,
            const std::vector<VkImageView>& imageViews,
            const std::vector<VkSampler>& samplers,
            const std::vector<VkImageLayout>& imageLayouts = {});

        // 批量更新所有帧的相同 Binding
        void updateUniformBufferForAllFrames(uint32_t setIndex, uint32_t binding,
            VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);

        void updateCombinedImageSamplerForAllFrames(uint32_t setIndex, uint32_t binding,
            VkImageView imageView, VkSampler sampler,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // === 查询接口 ===
        VkDescriptorSet getDescriptorSet(uint32_t setIndex, uint32_t frameIndex = 0) const;
        VkDescriptorSetLayout getLayout(uint32_t setIndex) const;
        std::shared_ptr<DescriptorSetLayout> getLayoutObject(uint32_t setIndex) const;
        std::vector<VkDescriptorSetLayout> getLayoutHandles() const;
        uint32_t getCurrentInstanceCount() const;

        // === 获取布局的方法 ===
        std::vector<VkDescriptorSetLayout> getLayoutHandles() const;
        std::vector<VkDescriptorSetLayout> getLayoutHandles(uint32_t startSet, uint32_t count) const;

        uint32_t getMaxSetIndex() const;
        bool hasContinuousSetIndices() const;

        // === 管理接口 ===
        void reset();
        void cleanup();

        // === 状态查询 ===
        bool isBuildingLayout() const { return mIsBuildingLayout; }
        bool hasAllocatedSets() const { return !mSets.empty(); }

    private:
        struct SetInstance {
            std::vector<VkDescriptorSet> descriptorSets;
        };

    private:
        std::shared_ptr<LogicalDevice> mLogicalDevice;
        std::shared_ptr<DescriptorAllocator> mAllocator;
        std::shared_ptr<DescriptorWriter> mWriter;

        std::unordered_map<uint32_t, std::shared_ptr<DescriptorSetLayout>> mLayouts;
        std::unordered_map<uint32_t, SetInstance> mSets;
        DescriptorTracker mRequirements;

        uint32_t mCurrentSetIndex = 0;
        bool mIsBuildingLayout = false;

    private:
        void validateSetIndex(uint32_t setIndex) const;
        void validateAllocated() const;
        void validateFrameIndex(uint32_t frameIndex) const;
        std::shared_ptr<DescriptorSetLayout> getCurrentLayout() const;
    };
}