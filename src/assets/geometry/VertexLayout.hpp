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
        glm::mat4 invView;
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

        VertexLayout& merge(const VertexLayout& other);

        static VertexLayout makeInstancingLayout(uint32_t binding = 1);

        void print() const;

        static uint32_t getFormatSize(RHI::Format format);

    private:
        uint32_t getNextOffset(uint32_t binding) const;

        struct BindingInfo {
            uint32_t stride;
            RHI::VertexInputRate inputRate;
        };
        std::unordered_map<uint32_t, BindingInfo> mBindings;
        std::vector<RHI::VertexAttribute> mAttributes;
        mutable std::unordered_map<uint32_t, uint32_t> mBindingCurrentOffsets;
    };

    struct InstancingAttribute {
        uint32_t location;      // 着色器中的 location
        RHI::Format format;     // 数据类型（如 RGBA32_Float）
        uint32_t offset;        // 在实例缓冲区中的偏移（字节）
    };

    struct InstancingLayout {
        uint32_t binding = 1;   
        uint32_t stride = 0;    
        std::vector<InstancingAttribute> attributes;

        void autoCalculateOffsets() {
            uint32_t currentOffset = 0;
            for (auto& attr : attributes) {
                attr.offset = currentOffset;
                currentOffset += VertexLayout::getFormatSize(attr.format);
            }
            stride = currentOffset;
        }

        RHI::VertexInputState toVertexInputState() const {
            RHI::VertexInputState state;

            // 显式构造 VertexBinding
            RHI::VertexBinding vb;
            vb.binding = binding;
            vb.stride = stride;
            vb.inputRate = RHI::VertexInputRate::PerInstance;
            state.bindings.push_back(vb);

            // 显式构造每个 VertexAttribute
            for (const auto& attr : attributes) {
                RHI::VertexAttribute va;
                va.location = attr.location;
                va.binding = binding;        
                va.offset = attr.offset;
                va.format = attr.format;
                va.debugName = "";         
                state.attributes.push_back(va);
            }

            return state;
        }
    };
}