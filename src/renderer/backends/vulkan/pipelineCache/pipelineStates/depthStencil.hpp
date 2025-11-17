#pragma once
#include <vulkan/vulkan.h>
namespace StarryEngine {
    class DepthStencil {
    public:
        struct StencilConfig {
            VkStencilOp failOp = VK_STENCIL_OP_KEEP;
            VkStencilOp passOp = VK_STENCIL_OP_KEEP;
            VkStencilOp depthFailOp = VK_STENCIL_OP_KEEP;
            VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
            uint32_t compareMask = 0xFF;
            uint32_t writeMask = 0xFF;
            uint32_t reference = 0;
        };

        DepthStencil& enableDepthTest(VkBool32 enable = VK_TRUE);
        DepthStencil& enableDepthWrite(VkBool32 enable = VK_TRUE);
        DepthStencil& setDepthCompareOp(VkCompareOp compareOp);
        DepthStencil& enableStencilTest(VkBool32 enable = VK_TRUE);
        DepthStencil& setStencilFront(const StencilConfig& config);
        DepthStencil& setStencilBack(const StencilConfig& config);
        DepthStencil& enableDepthBoundsTest(VkBool32 enable = VK_FALSE);
        DepthStencil& setDepthBounds(float min, float max);

        const VkPipelineDepthStencilStateCreateInfo& getCreateInfo() const;

    private:
        VkPipelineDepthStencilStateCreateInfo mCreateInfo{};
        StencilConfig mFrontStencil{};
        StencilConfig mBackStencil{};
        float mMinDepthBounds = 0.0f;
        float mMaxDepthBounds = 1.0f;
    };
}
