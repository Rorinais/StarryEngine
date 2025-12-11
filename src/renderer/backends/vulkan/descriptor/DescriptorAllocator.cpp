#include "DescriptorAllocator.hpp"
#include "../vulkanCore/VulkanCore.hpp"
#include <stdexcept>
#include <algorithm>

namespace StarryEngine {

    DescriptorAllocator::DescriptorAllocator(const std::shared_ptr<LogicalDevice>& logicalDevice)
        : mLogicalDevice(logicalDevice) {
    }

    DescriptorAllocator::~DescriptorAllocator() {
        // 清理所有分配的描述符集
        for (auto set : mAllocatedSets) {
            auto pool = findPoolForSet(set);
            if (pool != VK_NULL_HANDLE) {
                vkFreeDescriptorSets(mLogicalDevice->getHandle(), pool, 1, &set);
            }
        }
        mAllocatedSets.clear();
        mSetToPoolMap.clear();
        mPools.clear();
    }

    void DescriptorAllocator::initialize(const DescriptorTracker& requirements) {
        auto poolSizes = requirements.getPoolSizes();
        uint32_t maxSets = requirements.getTotalSetCount();

        if (!poolSizes.empty() && maxSets > 0) {
            auto pool = createPool(poolSizes, maxSets, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
            mPools.push_back(pool);  
        }
    }

    VkDescriptorSet DescriptorAllocator::allocate(VkDescriptorSetLayout layout) {
        return allocate(layout, 1)[0];
    }

    std::vector<VkDescriptorSet> DescriptorAllocator::allocate(VkDescriptorSetLayout layout, uint32_t count) {
        if (count == 0) return {};

        auto pool = findAvailablePool(layout, count);
        if (!pool) {
            throw std::runtime_error("No available pool for descriptor set allocation");
        }

        std::vector<VkDescriptorSetLayout> layouts(count, layout);
        std::vector<VkDescriptorSet> descriptorSets(count);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool->getHandle();
        allocInfo.descriptorSetCount = count;
        allocInfo.pSetLayouts = layouts.data();

        VkResult result = vkAllocateDescriptorSets(
            mLogicalDevice->getHandle(),
            &allocInfo,
            descriptorSets.data()
        );

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }

        // 记录分配
        for (auto set : descriptorSets) {
            mAllocatedSets.push_back(set);
            mSetToPoolMap[set] = pool->getHandle();
        }

        return descriptorSets;
    }

    VkDescriptorSet DescriptorAllocator::allocate(const std::shared_ptr<DescriptorSetLayout>& layout) {
        return allocate(layout->getHandle());
    }

    std::vector<VkDescriptorSet> DescriptorAllocator::allocate(
        const std::shared_ptr<DescriptorSetLayout>& layout,
        uint32_t count) {
        return allocate(layout->getHandle(), count);
    }

    void DescriptorAllocator::free(VkDescriptorSet descriptorSet) {
        auto pool = findPoolForSet(descriptorSet);
        if (pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(mLogicalDevice->getHandle(), pool, 1, &descriptorSet);

            // 从记录中移除
            mAllocatedSets.erase(
                std::remove(mAllocatedSets.begin(), mAllocatedSets.end(), descriptorSet),
                mAllocatedSets.end()
            );
            mSetToPoolMap.erase(descriptorSet);
        }
    }

    void DescriptorAllocator::free(const std::vector<VkDescriptorSet>& descriptorSets) {
        for (auto set : descriptorSets) {
            free(set);
        }
    }

    void DescriptorAllocator::reset() {
        for (auto& pool : mPools) {
            vkResetDescriptorPool(mLogicalDevice->getHandle(), pool->getHandle(), 0);
        }

        // 清空分配记录
        mAllocatedSets.clear();
        mSetToPoolMap.clear();
    }

    void DescriptorAllocator::addPool(const std::shared_ptr<DescriptorPool>& pool) {
        mPools.push_back(pool);
    }

    void DescriptorAllocator::addPool(const DescriptorTracker& requirements, VkDescriptorPoolCreateFlags flags) {
        auto poolSizes = requirements.getPoolSizes();
        uint32_t maxSets = requirements.getTotalSetCount();

        if (!poolSizes.empty() && maxSets > 0) {
            auto pool = createPool(poolSizes, maxSets, flags);
            mPools.push_back(pool); 
        }
    }

    // 私有方法实现
    std::shared_ptr<DescriptorPool> DescriptorAllocator::createPool(
        const std::vector<VkDescriptorPoolSize>& poolSizes,
        uint32_t maxSets,
        VkDescriptorPoolCreateFlags flags) {
        return DescriptorPool::create(mLogicalDevice, poolSizes, maxSets, flags);
    }

    std::shared_ptr<DescriptorPool> DescriptorAllocator::findAvailablePool(VkDescriptorSetLayout layout, uint32_t count) {
        // 简化实现：总是返回第一个池
        // 实际项目中可以实现更复杂的池选择逻辑
        if (mPools.empty()) {
            // 如果没有池，创建一个默认池
            DescriptorTracker defaultTracker;
            defaultTracker.addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10, 10);
            defaultTracker.addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10, 10);
            initialize(defaultTracker);
        }

        return mPools[0];
    }

    VkDescriptorPool DescriptorAllocator::findPoolForSet(VkDescriptorSet set) const {
        auto it = mSetToPoolMap.find(set);
        return (it != mSetToPoolMap.end()) ? it->second : VK_NULL_HANDLE;
    }
}