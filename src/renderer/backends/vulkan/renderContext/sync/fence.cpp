#include"fence.hpp"
namespace StarryEngine {

    Fence::Fence(const LogicalDevice::Ptr& logicalDevice, bool signaled) :mLogicalDevice(logicalDevice) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

        if (vkCreateFence(mLogicalDevice->getHandle(), &fenceInfo, nullptr, &mFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create fence!");
        }
    }

    Fence::~Fence() {
        if (mFence != VK_NULL_HANDLE) {
            vkDestroyFence(mLogicalDevice->getHandle(), mFence, nullptr);
            mFence = VK_NULL_HANDLE;
        }
    }

    void Fence::resetFence() {
        vkResetFences(mLogicalDevice->getHandle(), 1, &mFence);
    }

    void Fence::block(uint64_t timeout) {
        vkWaitForFences(mLogicalDevice->getHandle(), 1, &mFence, VK_TRUE, timeout);
    }

}