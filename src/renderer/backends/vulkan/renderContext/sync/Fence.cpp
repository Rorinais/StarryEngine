#include"Fence.hpp"
namespace StarryEngine {

    Fence::Fence(VkDevice device, bool signaled = true) :mDevice(device) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

        if (vkCreateFence(mDevice, &fenceInfo, nullptr, &mFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create fence!");
        }
    }

    Fence::~Fence() {
        if (mFence != VK_NULL_HANDLE) {
            vkDestroyFence(mDevice, mFence, nullptr);
            mFence = VK_NULL_HANDLE;
        }
    }

    void Fence::resetFence() {
        vkResetFences(mDevice, 1, &mFence);
    }

    void Fence::block(uint64_t timeout) {
        vkWaitForFences(mDevice, 1, &mFence, VK_TRUE, timeout);
    }

}