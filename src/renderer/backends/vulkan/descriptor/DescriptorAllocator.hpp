#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include "DescriptorPool.hpp"
#include "DescriptorSetLayout.hpp"
#include "DescriptorTracker.hpp"

namespace StarryEngine {
    class LogicalDevice;

    class DescriptorAllocator {
    public:
        using Ptr = std::shared_ptr<DescriptorAllocator>;

        DescriptorAllocator(const std::shared_ptr<LogicalDevice>& logicalDevice);
        ~DescriptorAllocator();

        // === 核心：分配管理 ===

        // 初始化分配器
        void initialize(const DescriptorTracker& requirements);

        // 分配单个描述符集
        VkDescriptorSet allocate(VkDescriptorSetLayout layout);

        // 批量分配描述符集
        std::vector<VkDescriptorSet> allocate(VkDescriptorSetLayout layout, uint32_t count);

        // 使用 DescriptorSetLayout 类分配
        VkDescriptorSet allocate(const std::shared_ptr<DescriptorSetLayout>& layout);
        std::vector<VkDescriptorSet> allocate(const std::shared_ptr<DescriptorSetLayout>& layout, uint32_t count);

        // 释放描述符集（返回到池中）
        void free(VkDescriptorSet descriptorSet);
        void free(const std::vector<VkDescriptorSet>& descriptorSets);

        // 重置所有池（释放所有描述符集）
        void reset();

        // === 池管理 ===

        // 添加现有的池
        void addPool(const std::shared_ptr<DescriptorPool>& pool);

        // 使用 DescriptorTracker 创建并添加新池
        void addPool(const DescriptorTracker& requirements, VkDescriptorPoolCreateFlags flags = 0);

        // 获取池信息
        std::shared_ptr<DescriptorPool> getDefaultPool() const {
            return mPools.empty() ? nullptr : mPools[0];
        }
        const std::vector<std::shared_ptr<DescriptorPool>>& getPools() const { return mPools; }

        // === 统计信息 ===
        size_t getAllocatedSetCount() const { return mAllocatedSets.size(); }
        size_t getPoolCount() const { return mPools.size(); }

    private:
        std::shared_ptr<DescriptorPool> createPool(
            const std::vector<VkDescriptorPoolSize>& poolSizes,
            uint32_t maxSets,
            VkDescriptorPoolCreateFlags flags = 0);

        std::shared_ptr<DescriptorPool> findAvailablePool(VkDescriptorSetLayout layout, uint32_t count = 1);
        VkDescriptorPool findPoolForSet(VkDescriptorSet set) const;

    private:
        std::shared_ptr<LogicalDevice> mLogicalDevice;
        std::vector<std::shared_ptr<DescriptorPool>> mPools;

        // 跟踪分配的描述符集和它们的池
        std::unordered_map<VkDescriptorSet, VkDescriptorPool> mSetToPoolMap;
        std::vector<VkDescriptorSet> mAllocatedSets;
    };
}