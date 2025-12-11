#pragma once
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>

namespace StarryEngine {

    class DescriptorTracker {
    public:
        void addBinding(VkDescriptorType type, uint32_t count, uint32_t setCount = 1);

        void addLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings, uint32_t setCount = 1);

        void merge(const DescriptorTracker& other);

        void reset();

        std::vector<VkDescriptorPoolSize> getPoolSizes() const;
        uint32_t getTotalSetCount() const { return mTotalSets; }
    private:
        std::unordered_map<VkDescriptorType, uint32_t> mTypeCounts;
        uint32_t mTotalSets = 0;
    };
} // namespace StarryEngine

