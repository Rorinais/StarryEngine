#include "multiSample.hpp"
namespace StarryEngine {

    MultiSample& MultiSample::setSampleCount(VkSampleCountFlagBits count) {
        mConfig.rasterizationSamples = count;
        return *this;
    }

    MultiSample& MultiSample::enableSampleShading(VkBool32 enable) {
        mConfig.sampleShadingEnable = enable;
        return *this;
    }

    const VkPipelineMultisampleStateCreateInfo& MultiSample::getCreateInfo() const {
        static VkPipelineMultisampleStateCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        info.rasterizationSamples = mConfig.rasterizationSamples;
        info.sampleShadingEnable = mConfig.sampleShadingEnable;
        return info;
    }
}