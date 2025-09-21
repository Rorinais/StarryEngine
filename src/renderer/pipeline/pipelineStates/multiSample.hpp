#pragma once
#include <vulkan/vulkan.h>
namespace StarryEngine {
    class MultiSample {
    public:
        struct Config {
            VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkBool32 sampleShadingEnable = VK_FALSE;
        };

        MultiSample& setSampleCount(VkSampleCountFlagBits count = VK_SAMPLE_COUNT_1_BIT);
        MultiSample& enableSampleShading(VkBool32 enable = VK_FALSE);

        const VkPipelineMultisampleStateCreateInfo& getCreateInfo() const;

    private:
        Config mConfig;
        VkPipelineMultisampleStateCreateInfo mCreateInfo{};
    };
}