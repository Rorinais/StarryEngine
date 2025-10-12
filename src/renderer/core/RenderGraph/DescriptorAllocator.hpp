#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>

class DescriptorAllocator {
public:
    DescriptorAllocator(VkDevice device);
    ~DescriptorAllocator();

    bool initialize();
    void cleanup();

    VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout layout);
    void resetPools();

private:
    VkDevice mDevice;
    VkDescriptorPool mCurrentPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorPool> mUsedPools;
    std::vector<VkDescriptorPool> mFreePools;

    VkDescriptorPool createPool();
};