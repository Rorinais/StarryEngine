#include"DescriptorTracker.hpp"

namespace StarryEngine {

    // 添加单个绑定的需求
    void DescriptorTracker::addBinding(VkDescriptorType type, uint32_t count, uint32_t setCount) {
        mTypeCounts[type] += count * setCount;
        mTotalSets += setCount;
    }

    // 添加整个布局的需求
    void DescriptorTracker::addLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings, uint32_t setCount) {
        for (const auto& binding : bindings) {
            mTypeCounts[binding.descriptorType] += binding.descriptorCount * setCount;
        }
        mTotalSets += setCount;
    }

    // 获取池大小配置
    std::vector<VkDescriptorPoolSize> DescriptorTracker::getPoolSizes() const {
        std::vector<VkDescriptorPoolSize> poolSizes;
        for (const auto& [type, count] : mTypeCounts) {
            if (count > 0) {
                poolSizes.push_back({ type, count });
            }
        }
        return poolSizes;
    }

    // 重置统计
    void DescriptorTracker::reset() {
        mTypeCounts.clear();
        mTotalSets = 0;
    }

    // 合并另一个统计器
    void DescriptorTracker::merge(const DescriptorTracker& other) {
        for (const auto& [type, count] : other.mTypeCounts) {
            mTypeCounts[type] += count;
        }
        mTotalSets += other.mTotalSets;
    }

} // namespace StarryEngine