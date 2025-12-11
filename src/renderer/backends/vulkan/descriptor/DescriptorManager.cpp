#include "DescriptorManager.hpp"
#include "../vulkanCore/VulkanCore.hpp"
#include <stdexcept>

namespace StarryEngine {

    DescriptorManager::DescriptorManager(const std::shared_ptr<LogicalDevice>& logicalDevice)
        : mLogicalDevice(logicalDevice) {
        mAllocator = std::make_shared<DescriptorAllocator>(logicalDevice);
        mWriter = std::make_shared<DescriptorWriter>(logicalDevice);
    }

    // === 布局定义方法 ===

    void DescriptorManager::beginSetLayout(uint32_t setIndex) {
        if (mIsBuildingLayout) {
            throw std::runtime_error("Already building a layout. Call endSetLayout() first.");
        }

        if (mLayouts.find(setIndex) != mLayouts.end()) {
            auto existingLayout = mLayouts[setIndex];
            if (existingLayout->isBuilt()) {
                throw std::runtime_error("Set layout " + std::to_string(setIndex) + " is already built and cannot be modified");
            }
        }

        mLayouts[setIndex] = std::make_shared<DescriptorSetLayout>(mLogicalDevice);
        mCurrentSetIndex = setIndex;
        mIsBuildingLayout = true;
    }

    void DescriptorManager::addUniformBuffer(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count) {
        if (!mIsBuildingLayout) {
            throw std::runtime_error("Not currently building a layout. Call beginSetLayout() first.");
        }

        auto layout = getCurrentLayout();
        layout->addBinding(binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stageFlags, count);
    }

    void DescriptorManager::addCombinedImageSampler(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count) {
        if (!mIsBuildingLayout) {
            throw std::runtime_error("Not currently building a layout. Call beginSetLayout() first.");
        }

        auto layout = getCurrentLayout();
        layout->addBinding(binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, stageFlags, count);
    }

    void DescriptorManager::addStorageBuffer(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count) {
        if (!mIsBuildingLayout) {
            throw std::runtime_error("Not currently building a layout. Call beginSetLayout() first.");
        }

        auto layout = getCurrentLayout();
        layout->addBinding(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stageFlags, count);
    }

    void DescriptorManager::addImage(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count) {
        if (!mIsBuildingLayout) {
            throw std::runtime_error("Not currently building a layout. Call beginSetLayout() first.");
        }

        auto layout = getCurrentLayout();
        layout->addBinding(binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, stageFlags, count);
    }

    void DescriptorManager::addSampler(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count) {
        if (!mIsBuildingLayout) {
            throw std::runtime_error("Not currently building a layout. Call beginSetLayout() first.");
        }

        auto layout = getCurrentLayout();
        layout->addBinding(binding, VK_DESCRIPTOR_TYPE_SAMPLER, stageFlags, count);
    }

    void DescriptorManager::endSetLayout() {
        if (!mIsBuildingLayout) {
            throw std::runtime_error("Not currently building a layout. Call beginSetLayout() first.");
        }

        auto layout = getCurrentLayout();
        if (!layout->isBuilt()) {
            layout->build();
        }

        mIsBuildingLayout = false;
    }

    // === 分配方法 ===

    void DescriptorManager::allocateSets(uint32_t setCount) {

        if (mIsBuildingLayout) {
            throw std::runtime_error("Cannot allocate sets while building a layout. Call endSetLayout() first.");
        }

        // 如果已经有分配的描述符集，先释放它们
        if (!mSets.empty()) {
            freeSets();
        }

        // 计算需求
        mRequirements.reset();
        for (const auto& [setIndex, layout] : mLayouts) {
            auto bindings = layout->getBindings();
            mRequirements.addLayout(bindings, setCount);
        }

        // 初始化分配器
        mAllocator->initialize(mRequirements);

        // 分配描述符集
        for (const auto& [setIndex, layout] : mLayouts) {
            auto descriptorSets = mAllocator->allocate(layout, setCount);
            mSets[setIndex] = SetInstance{ descriptorSets };
        }
    }

    void DescriptorManager::freeSets(bool isClearAllocator) {
        if (mSets.empty()) return;

        // 释放所有描述符集
        for (auto& [setIndex, setInstance] : mSets) {
            mAllocator->free(setInstance.descriptorSets);
        }
        mSets.clear();

        if (isClearAllocator) {
			mAllocator->reset();
        }
    }

    // === 更新方法 ===

    void DescriptorManager::updateUniformBuffer(uint32_t setIndex, uint32_t binding, uint32_t frameIndex,
        VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
        if (mIsBuildingLayout) {
            throw std::runtime_error("Cannot update sets while building a layout. Call endSetLayout() first.");
        }

        validateSetIndex(setIndex);
        validateAllocated();
        validateFrameIndex(frameIndex);

        auto set = getDescriptorSet(setIndex, frameIndex);
        mWriter->updateUniformBuffer(set, binding, buffer, offset, range);
    }

    void DescriptorManager::updateCombinedImageSampler(uint32_t setIndex, uint32_t binding, uint32_t frameIndex,
        VkImageView imageView, VkSampler sampler,
        VkImageLayout imageLayout) {
        if (mIsBuildingLayout) {
            throw std::runtime_error("Cannot update sets while building a layout. Call endSetLayout() first.");
        }

        validateSetIndex(setIndex);
        validateAllocated();
        validateFrameIndex(frameIndex);

        auto set = getDescriptorSet(setIndex, frameIndex);
        mWriter->updateCombinedImageSampler(set, binding, imageView, sampler, imageLayout);
    }

    void DescriptorManager::updateStorageBuffer(uint32_t setIndex, uint32_t binding, uint32_t frameIndex,
        VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
        if (mIsBuildingLayout) {
            throw std::runtime_error("Cannot update sets while building a layout. Call endSetLayout() first.");
        }

        validateSetIndex(setIndex);
        validateAllocated();
        validateFrameIndex(frameIndex);

        auto set = getDescriptorSet(setIndex, frameIndex);
        mWriter->updateStorageBuffer(set, binding, buffer, offset, range);
    }

    void DescriptorManager::updateImage(uint32_t setIndex, uint32_t binding, uint32_t frameIndex,
        VkImageView imageView, VkImageLayout imageLayout) {
        if (mIsBuildingLayout) {
            throw std::runtime_error("Cannot update sets while building a layout. Call endSetLayout() first.");
        }

        validateSetIndex(setIndex);
        validateAllocated();
        validateFrameIndex(frameIndex);

        auto set = getDescriptorSet(setIndex, frameIndex);
        mWriter->updateImage(set, binding, imageView, imageLayout);
    }

    void DescriptorManager::updateSampler(uint32_t setIndex, uint32_t binding, uint32_t frameIndex, VkSampler sampler) {
        if (mIsBuildingLayout) {
            throw std::runtime_error("Cannot update sets while building a layout. Call endSetLayout() first.");
        }

        validateSetIndex(setIndex);
        validateAllocated();
        validateFrameIndex(frameIndex);

        auto set = getDescriptorSet(setIndex, frameIndex);
        mWriter->updateSampler(set, binding, sampler);
    }

    void DescriptorManager::updateUniformBuffers(uint32_t setIndex, uint32_t frameIndex,
        const std::vector<uint32_t>& bindings,
        const std::vector<VkBuffer>& buffers,
        const std::vector<VkDeviceSize>& offsets,
        const std::vector<VkDeviceSize>& ranges) {
        if (mIsBuildingLayout) {
            throw std::runtime_error("Cannot update sets while building a layout. Call endSetLayout() first.");
        }

        validateSetIndex(setIndex);
        validateAllocated();
        validateFrameIndex(frameIndex);

        auto set = getDescriptorSet(setIndex, frameIndex);
        mWriter->updateUniformBuffers(set, bindings, buffers, offsets, ranges);
    }

    void DescriptorManager::updateCombinedImageSamplers(uint32_t setIndex, uint32_t frameIndex,
        const std::vector<uint32_t>& bindings,
        const std::vector<VkImageView>& imageViews,
        const std::vector<VkSampler>& samplers,
        const std::vector<VkImageLayout>& imageLayouts) {
        if (mIsBuildingLayout) {
            throw std::runtime_error("Cannot update sets while building a layout. Call endSetLayout() first.");
        }

        validateSetIndex(setIndex);
        validateAllocated();
        validateFrameIndex(frameIndex);

        auto set = getDescriptorSet(setIndex, frameIndex);
        mWriter->updateCombinedImageSamplers(set, bindings, imageViews, samplers, imageLayouts);
    }

    // === 批量更新所有帧 ===

    void DescriptorManager::updateUniformBufferForAllFrames(uint32_t setIndex, uint32_t binding,
        VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
        if (mIsBuildingLayout) {
            throw std::runtime_error("Cannot update sets while building a layout. Call endSetLayout() first.");
        }

        validateSetIndex(setIndex);
        validateAllocated();

        uint32_t frameCount = getCurrentInstanceCount();
        for (uint32_t frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
            auto set = getDescriptorSet(setIndex, frameIndex);
            mWriter->updateUniformBuffer(set, binding, buffer, offset, range);
        }
    }

    void DescriptorManager::updateCombinedImageSamplerForAllFrames(uint32_t setIndex, uint32_t binding,
        VkImageView imageView, VkSampler sampler,
        VkImageLayout imageLayout) {
        if (mIsBuildingLayout) {
            throw std::runtime_error("Cannot update sets while building a layout. Call endSetLayout() first.");
        }

        validateSetIndex(setIndex);
        validateAllocated();

        uint32_t frameCount = getCurrentInstanceCount();
        for (uint32_t frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
            auto set = getDescriptorSet(setIndex, frameIndex);
            mWriter->updateCombinedImageSampler(set, binding, imageView, sampler, imageLayout);
        }
    }
    // === 查询方法 ===

    VkDescriptorSet DescriptorManager::getDescriptorSet(uint32_t setIndex, uint32_t frameIndex) const {
        validateSetIndex(setIndex);
        auto it = mSets.find(setIndex);
        if (it == mSets.end() || frameIndex >= it->second.descriptorSets.size()) {
            throw std::runtime_error("Descriptor set not found for set index: " + std::to_string(setIndex) +
                ", frame index: " + std::to_string(frameIndex));
        }
        return it->second.descriptorSets[frameIndex];
    }

    uint32_t DescriptorManager::getCurrentInstanceCount() const {
        if (mSets.empty()) return 0;
        // 假设所有set的实例数量相同
        return static_cast<uint32_t>(mSets.begin()->second.descriptorSets.size());
    }

    VkDescriptorSetLayout DescriptorManager::getLayout(uint32_t setIndex) const {
        validateSetIndex(setIndex);
        return mLayouts.at(setIndex)->getHandle();
    }

    std::shared_ptr<DescriptorSetLayout> DescriptorManager::getLayoutObject(uint32_t setIndex) const {
        validateSetIndex(setIndex);
        return mLayouts.at(setIndex);
    }
    // === 获取布局的方法 ===

    std::vector<VkDescriptorSetLayout> DescriptorManager::getLayoutHandles() const {
        std::vector<VkDescriptorSetLayout> handles;

        // 我们需要按set索引顺序返回布局
        std::vector<uint32_t> setIndices;
        for (const auto& [setIndex, layout] : mLayouts) {
            setIndices.push_back(setIndex);
        }

        // 按set索引排序
        std::sort(setIndices.begin(), setIndices.end());

        // 按顺序添加布局句柄
        for (uint32_t setIndex : setIndices) {
            handles.push_back(mLayouts.at(setIndex)->getHandle());
        }

        return handles;
    }

    std::vector<VkDescriptorSetLayout> DescriptorManager::getLayoutHandles(uint32_t startSet, uint32_t count) const {
        std::vector<VkDescriptorSetLayout> handles;

        for (uint32_t i = 0; i < count; ++i) {
            uint32_t setIndex = startSet + i;
            if (mLayouts.find(setIndex) != mLayouts.end()) {
                handles.push_back(mLayouts.at(setIndex)->getHandle());
            }
            else {
                // 如果某个set索引不存在，可以添加空句柄或抛出异常
                handles.push_back(VK_NULL_HANDLE);
            }
        }

        return handles;
    }

    uint32_t DescriptorManager::getMaxSetIndex() const {
        if (mLayouts.empty()) return 0;

        uint32_t maxIndex = 0;
        for (const auto& [setIndex, layout] : mLayouts) {
            if (setIndex > maxIndex) {
                maxIndex = setIndex;
            }
        }
        return maxIndex;
    }

    bool DescriptorManager::hasContinuousSetIndices() const {
        if (mLayouts.empty()) return true;

        std::vector<uint32_t> indices;
        for (const auto& [setIndex, layout] : mLayouts) {
            indices.push_back(setIndex);
        }

        std::sort(indices.begin(), indices.end());

        for (size_t i = 1; i < indices.size(); ++i) {
            if (indices[i] != indices[i - 1] + 1) {
                return false;
            }
        }

        return true;
    }

    // === 管理方法 ===

    void DescriptorManager::reset() {
        if (mIsBuildingLayout) {
            throw std::runtime_error("Cannot reset while building a layout. Call endSetLayout() first.");
        }

        freeSets();
        mAllocator->reset();
    }

    void DescriptorManager::cleanup() {
        freeSets();
        mAllocator.reset();
        mWriter.reset();
        mLayouts.clear();
        mRequirements.reset();
        mIsBuildingLayout = false;
    }

    // === 私有方法 ===

    void DescriptorManager::validateSetIndex(uint32_t setIndex) const {
        if (mLayouts.find(setIndex) == mLayouts.end()) {
            throw std::runtime_error("Set layout not found: " + std::to_string(setIndex));
        }
    }

    void DescriptorManager::validateAllocated() const {
        if (mSets.empty()) {
            throw std::runtime_error("Descriptor sets not allocated");
        }
    }

    void DescriptorManager::validateFrameIndex(uint32_t frameIndex) const {
        if (mSets.empty()) return;

        uint32_t frameCount = getCurrentInstanceCount();
        if (frameIndex >= frameCount) {
            throw std::runtime_error("Frame index out of range: " + std::to_string(frameIndex) +
                ", max: " + std::to_string(frameCount - 1));
        }
    }

    std::shared_ptr<DescriptorSetLayout> DescriptorManager::getCurrentLayout() const {
        auto it = mLayouts.find(mCurrentSetIndex);
        if (it == mLayouts.end()) {
            throw std::runtime_error("No current set layout being built");
        }
        return it->second;
    }
}