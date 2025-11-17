#include "vertexInput.hpp"
namespace StarryEngine {
VertexInput& VertexInput::setBindings(const std::vector<VkVertexInputBindingDescription>& bindings) {
    mBindings = bindings;
    return *this;
}

VertexInput& VertexInput::setAttributes(const std::vector<VkVertexInputAttributeDescription>& attributes) {
    mAttributes = attributes;
    return *this;
}

VertexInput& VertexInput::addBinding(uint32_t binding, uint32_t stride) {
    VkVertexInputBindingDescription desc{};
    desc.binding = binding;
    desc.stride = stride;
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    mBindings.push_back(desc);
    return *this;
}

VertexInput& VertexInput::addAttribute(uint32_t binding, const VkVertexInputAttributeDescription& attr) {
    mAttributes.push_back(attr);
    return *this;
}

const VkPipelineVertexInputStateCreateInfo& VertexInput::getCreateInfo() {
    mCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    mCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(mBindings.size());
    mCreateInfo.pVertexBindingDescriptions = mBindings.data();
    mCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(mAttributes.size());
    mCreateInfo.pVertexAttributeDescriptions = mAttributes.data();
    return mCreateInfo;
}
}
