#pragma once
#include <vulkan/vulkan.h>
#include <vector>
namespace StarryEngine {
    class VertexInput {
    public:
        VertexInput() {
            mCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            mCreateInfo.pNext = nullptr;
            mCreateInfo.flags = 0;
            mCreateInfo.vertexBindingDescriptionCount = 0;
            mCreateInfo.pVertexBindingDescriptions = nullptr;
            mCreateInfo.vertexAttributeDescriptionCount = 0;
            mCreateInfo.pVertexAttributeDescriptions = nullptr;
        }

        VertexInput& setBindings(const std::vector<VkVertexInputBindingDescription>& bindings);

        VertexInput& setAttributes(const std::vector<VkVertexInputAttributeDescription>& attributes);

        VertexInput& addBinding(uint32_t binding, uint32_t stride);

        VertexInput& addAttribute(uint32_t binding, const VkVertexInputAttributeDescription& attr);

        const VkPipelineVertexInputStateCreateInfo& getCreateInfo();

    private:
        std::vector<VkVertexInputBindingDescription> mBindings;
        std::vector<VkVertexInputAttributeDescription> mAttributes;
        VkPipelineVertexInputStateCreateInfo mCreateInfo{};
    };
}
