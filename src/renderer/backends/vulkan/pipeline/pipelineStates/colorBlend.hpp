#pragma once
#include <vulkan/vulkan.h>
#include <vector>
namespace StarryEngine {

    class ColorBlend {
    public:
        struct Config {
            VkBool32 blendEnable = VK_FALSE;
            VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            VkBlendOp colorBlendOp = VK_BLEND_OP_ADD;
            VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD;
            VkColorComponentFlags colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT;
        };

        ColorBlend& addAttachment(const Config& config);

        const VkPipelineColorBlendStateCreateInfo& getCreateInfo() const;

    private:
        std::vector<VkPipelineColorBlendAttachmentState> mAttachments;
        VkPipelineColorBlendStateCreateInfo mCreateInfo{};
    };
}