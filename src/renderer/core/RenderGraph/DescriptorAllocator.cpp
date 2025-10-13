#include "DescriptorAllocator.hpp"
#include <stdexcept>
#include <iostream>

namespace StarryEngine {

    DescriptorAllocator::DescriptorAllocator(VkDevice device) : mDevice(device) {
    }

    DescriptorAllocator::~DescriptorAllocator() {
        cleanup();
    }

    bool DescriptorAllocator::initialize() {
        mCurrentPool = createPool();
        return mCurrentPool != VK_NULL_HANDLE;
    }

    void DescriptorAllocator::cleanup() {
        for (auto pool : mUsedPools) {
            vkDestroyDescriptorPool(mDevice, pool, nullptr);
        }
        for (auto pool : mFreePools) {
            vkDestroyDescriptorPool(mDevice, pool, nullptr);
        }
        mUsedPools.clear();
        mFreePools.clear();
        mCurrentPool = VK_NULL_HANDLE;
    }

    VkDescriptorSet DescriptorAllocator::allocateDescriptorSet(VkDescriptorSetLayout layout) {
        if (!mCurrentPool) {
            return VK_NULL_HANDLE;
        }

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mCurrentPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSet descriptorSet;
        VkResult result = vkAllocateDescriptorSets(mDevice, &allocInfo, &descriptorSet);

        if (result != VK_SUCCESS) {
            // 池可能已满，创建新池
            mUsedPools.push_back(mCurrentPool);
            mCurrentPool = createPool();
            if (!mCurrentPool) {
                return VK_NULL_HANDLE;
            }

            allocInfo.descriptorPool = mCurrentPool;
            result = vkAllocateDescriptorSets(mDevice, &allocInfo, &descriptorSet);
        }

        return (result == VK_SUCCESS) ? descriptorSet : VK_NULL_HANDLE;
    }

    void DescriptorAllocator::resetPools() {
        for (auto pool : mUsedPools) {
            vkResetDescriptorPool(mDevice, pool, 0);
            mFreePools.push_back(pool);
        }
        mUsedPools.clear();

        if (mCurrentPool) {
            vkResetDescriptorPool(mDevice, mCurrentPool, 0);
        }
    }

    VkDescriptorPool DescriptorAllocator::createPool() {
        // 简单的描述符池创建
        std::vector<VkDescriptorPoolSize> sizes = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 }
        };

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1000;
        poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
        poolInfo.pPoolSizes = sizes.data();

        VkDescriptorPool descriptorPool;
        if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }
        return descriptorPool;
    }

} // namespace StarryEngine