#include "inputAssembly.hpp"
namespace StarryEngine {
    InputAssembly& InputAssembly::setTopology(VkPrimitiveTopology topology) {
        mConfig.topology = topology;
        updateCreateInfo();
        return *this;
    }

    InputAssembly& InputAssembly::enablePrimitiveRestart(VkBool32 enable) {
        mConfig.primitiveRestartEnable = enable;
        updateCreateInfo();
        return *this;
    }

    const VkPipelineInputAssemblyStateCreateInfo& InputAssembly::getCreateInfo() const {
        return mCreateInfo;
    }

    void InputAssembly::updateCreateInfo() {
        mCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = mConfig.topology,
            .primitiveRestartEnable = mConfig.primitiveRestartEnable
        };
    }
}
