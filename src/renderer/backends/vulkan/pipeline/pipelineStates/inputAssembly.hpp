#pragma once
#include <vulkan/vulkan.h>
namespace StarryEngine {
    class InputAssembly {
    public:
        struct Config {
            VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkBool32 primitiveRestartEnable = VK_FALSE;
        };

        InputAssembly() {
            updateCreateInfo();
        }

        InputAssembly& setTopology(VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        InputAssembly& enablePrimitiveRestart(VkBool32 enable = VK_FALSE);

        const VkPipelineInputAssemblyStateCreateInfo& getCreateInfo() const;

    private:
        void updateCreateInfo();

        Config mConfig;
        VkPipelineInputAssemblyStateCreateInfo mCreateInfo{};
    };
}
