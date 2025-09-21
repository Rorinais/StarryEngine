#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
namespace StarryEngine {

    class Dynamic {
    public:
        void setEnableDynamic(bool enable = true) { mEnableDynamic = enable; }
        bool getEnableDynamic() { return mEnableDynamic; }

        const VkPipelineDynamicStateCreateInfo& getCreateInfo() {
            mDynamicStates.clear();

            if (mEnableDynamic) {
                mDynamicStates = {
                    VK_DYNAMIC_STATE_VIEWPORT,
                    VK_DYNAMIC_STATE_SCISSOR
                };
            }
            mDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            mDynamicStateCreateInfo.pNext = nullptr;
            mDynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(mDynamicStates.size());
            mDynamicStateCreateInfo.pDynamicStates = mDynamicStates.data();

            return mDynamicStateCreateInfo;
        }

        static void DynamicViewport(const VkExtent2D& swapChainExtent, const VkCommandBuffer& commandBuffers) {
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(swapChainExtent.width);
            viewport.height = static_cast<float>(swapChainExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffers, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = swapChainExtent;
            vkCmdSetScissor(commandBuffers, 0, 1, &scissor);
        }

    private:
        bool mEnableDynamic = false;
        std::vector<VkDynamicState> mDynamicStates;
        VkPipelineDynamicStateCreateInfo mDynamicStateCreateInfo{};
    };
}