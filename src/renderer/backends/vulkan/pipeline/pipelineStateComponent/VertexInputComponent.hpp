#pragma once
#include "../interface/TypedPipelineComponent.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <stdexcept>
#include <set>
#include <unordered_map>

namespace StarryEngine {

    class VertexInputComponent : public
        TypedPipelineComponent<VertexInputComponent, PipelineComponentType::VERTEX_INPUT> {
    public:
        VertexInputComponent(const std::string& name);
        VertexInputComponent& reset();

        VertexInputComponent& addBinding(const VkVertexInputBindingDescription& bindingDesc);
        VertexInputComponent& addBinding(uint32_t binding, uint32_t stride,VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX);
        VertexInputComponent& addAttribute(const VkVertexInputAttributeDescription& attr);
        VertexInputComponent& addAttribute(uint32_t location, uint32_t binding,VkFormat format, uint32_t offset);
        VertexInputComponent& addBindings(const std::vector<VkVertexInputBindingDescription>& bindings);
        VertexInputComponent& addAttributes(const std::vector<VkVertexInputAttributeDescription>& attributes);
        VertexInputComponent& setBindings(const std::vector<VkVertexInputBindingDescription>& bindings);
        VertexInputComponent& setAttributes(const std::vector<VkVertexInputAttributeDescription>& attributes);

        template<typename VertexBufferType>
        VertexInputComponent& configureFromVertexBuffer(const VertexBufferType& vertexBuffer) {
            auto bindingDescriptions = vertexBuffer.getBindingDescriptions();
            auto attributeDescriptions = vertexBuffer.getAttributeDescriptions();

            return setBindings(bindingDescriptions)
                .setAttributes(attributeDescriptions);
        }

        void apply(VkGraphicsPipelineCreateInfo& pipelineInfo) override {
            updateCreateInfo(); 
            pipelineInfo.pVertexInputState = &mCreateInfo;
        }

        std::string getDescription() const override;
        const std::vector<VkVertexInputBindingDescription>& getBindings() const { return mBindings; }
        const std::vector<VkVertexInputAttributeDescription>& getAttributes() const { return mAttributes; }
        uint32_t getBindingCount() const { return static_cast<uint32_t>(mBindings.size()); }
        uint32_t getAttributeCount() const { return static_cast<uint32_t>(mAttributes.size()); }

        bool isValid() const override;
    private:
        std::vector<VkVertexInputBindingDescription> mBindings;
        std::vector<VkVertexInputAttributeDescription> mAttributes;
        std::set<uint32_t> mAttributeLocations;  

        VkPipelineVertexInputStateCreateInfo mCreateInfo{};

        void updateCreateInfo();
    };

} // namespace StarryEngine