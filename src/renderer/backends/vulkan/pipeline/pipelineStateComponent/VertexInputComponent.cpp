#include"VertexInputComponent.hpp"


namespace StarryEngine {
    VertexInputComponent::VertexInputComponent(const std::string& name) {
        setName(name);
        reset();
    }

    VertexInputComponent& VertexInputComponent::reset() {
        mCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        mCreateInfo.pNext = nullptr;
        mCreateInfo.flags = 0;
        mCreateInfo.vertexBindingDescriptionCount = 0;
        mCreateInfo.pVertexBindingDescriptions = nullptr;
        mCreateInfo.vertexAttributeDescriptionCount = 0;
        mCreateInfo.pVertexAttributeDescriptions = nullptr;
        return *this;
    }

    VertexInputComponent& VertexInputComponent::addBinding(const VkVertexInputBindingDescription& bindingDesc) {
        for (auto& existing : mBindings) {
            if (existing.binding == bindingDesc.binding) {
                existing = bindingDesc;
                updateCreateInfo();
                return *this;
            }
        }

        mBindings.push_back(bindingDesc);
        updateCreateInfo();
        return *this;
    }

    VertexInputComponent& VertexInputComponent::addBinding(uint32_t binding, uint32_t stride,
        VkVertexInputRate inputRate) {
        VkVertexInputBindingDescription desc{
            .binding = binding,
            .stride = stride,
            .inputRate = inputRate
        };
        return addBinding(desc);
    }

    VertexInputComponent& VertexInputComponent::addAttribute(const VkVertexInputAttributeDescription& attr) {
        if (mAttributeLocations.find(attr.location) != mAttributeLocations.end()) {
            throw std::runtime_error("Attribute location " + std::to_string(attr.location) + " is already used");
        }

        mAttributes.push_back(attr);
        mAttributeLocations.insert(attr.location);
        updateCreateInfo();
        return *this;
    }

    VertexInputComponent& VertexInputComponent::addAttribute(uint32_t location, uint32_t binding,
        VkFormat format, uint32_t offset) {
        VkVertexInputAttributeDescription attr{
            .location = location,
            .binding = binding,
            .format = format,
            .offset = offset
        };
        return addAttribute(attr);
    }

    VertexInputComponent& VertexInputComponent::addBindings(const std::vector<VkVertexInputBindingDescription>& bindings) {
        for (const auto& binding : bindings) {
            addBinding(binding);
        }
        return *this;
    }

    VertexInputComponent& VertexInputComponent::addAttributes(const std::vector<VkVertexInputAttributeDescription>& attributes) {
        for (const auto& attr : attributes) {
            addAttribute(attr);
        }
        return *this;
    }

    VertexInputComponent& VertexInputComponent::setBindings(const std::vector<VkVertexInputBindingDescription>& bindings) {
        mBindings = bindings;
        updateCreateInfo();
        return *this;
    }

    VertexInputComponent& VertexInputComponent::setAttributes(const std::vector<VkVertexInputAttributeDescription>& attributes) {
        mAttributes.clear();
        mAttributeLocations.clear();
        for (const auto& attr : attributes) {
            mAttributes.push_back(attr);
            mAttributeLocations.insert(attr.location);
        }
        updateCreateInfo();
        return *this;
    }

    template<typename VertexBufferType>
    VertexInputComponent& VertexInputComponent::configureFromVertexBuffer(const VertexBufferType& vertexBuffer) {
        auto bindingDescriptions = vertexBuffer.getBindingDescriptions();
        auto attributeDescriptions = vertexBuffer.getAttributeDescriptions();

        return setBindings(bindingDescriptions)
            .setAttributes(attributeDescriptions);
    }

    std::string VertexInputComponent::getDescription() const {
        return "Bindings:" + std::to_string(mBindings.size()) +
            ", Attributes:" + std::to_string(mAttributes.size());
    }

    bool VertexInputComponent::isValid() const {
        std::set<uint32_t> bindingIds;
        for (const auto& binding : mBindings) {
            if (!bindingIds.insert(binding.binding).second) {
                return false;
            }
        }

        for (const auto& attr : mAttributes) {
            bool bindingFound = false;
            for (const auto& binding : mBindings) {
                if (binding.binding == attr.binding) {
                    bindingFound = true;
                    if (binding.stride > 0 && attr.offset >= binding.stride) {
                        return false;
                    }
                    break;
                }
            }
            if (!bindingFound) {
                return false;
            }
        }

        std::set<uint32_t> locations;
        for (const auto& attr : mAttributes) {
            if (!locations.insert(attr.location).second) {
                return false;
            }
        }

        return mCreateInfo.sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    }

    void VertexInputComponent::updateCreateInfo() {
        mCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(mBindings.size());
        mCreateInfo.pVertexBindingDescriptions = mBindings.empty() ? nullptr : mBindings.data();
        mCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(mAttributes.size());
        mCreateInfo.pVertexAttributeDescriptions = mAttributes.empty() ? nullptr : mAttributes.data();
    }
}