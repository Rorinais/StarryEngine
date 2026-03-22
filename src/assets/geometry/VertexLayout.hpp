#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../AssetType.hpp"

namespace StarryEngine::Assets {
    struct Uniforms {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    struct GlobalUniforms {
        glm::mat4 view;
        glm::mat4 proj;
    };

    struct MaterialUniforms {
        glm::vec4 baseColor = glm::vec4(1.0f);
        float metallic = 0.0f;
        float roughness = 0.5f;
        float emissiveIntensity = 0.0f;
        glm::vec3 emissiveColor = glm::vec3(0.0f);
    };

    class VertexLayout {
    public:
        VertexLayout& addBinding(uint32_t binding, uint32_t stride,
            RHI::VertexInputRate inputRate = RHI::VertexInputRate::PerVertex);
        VertexLayout& addAttribute(uint32_t location, uint32_t binding,
            RHI::Format format, uint32_t offset);
        VertexLayout& addAttribute(uint32_t location, uint32_t binding, RHI::Format format);
        RHI::VertexInputState build() const;

        uint32_t getBindingStride(uint32_t binding) const;

        std::vector<uint32_t> getBindings() const;

        void print() const;

    private:
        uint32_t getNextOffset(uint32_t binding) const;
        uint32_t getFormatSize(RHI::Format format) const;

        struct BindingInfo {
            uint32_t stride;
            RHI::VertexInputRate inputRate;
        };
        std::unordered_map<uint32_t, BindingInfo> mBindings;
        std::vector<RHI::VertexAttribute> mAttributes;
        mutable std::unordered_map<uint32_t, uint32_t> mBindingCurrentOffsets;
    };
}