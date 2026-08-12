#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assets/AssetType.hpp>
#include <cstddef>

namespace StarryEngine::Assets {
#define MAX_LIGHTS 4

    struct Uniforms {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    struct GlobalUniforms {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 invView;
        glm::mat4 invProj;
        float time;
        float pad[3];          // std140：mat4 须 16 字节对齐 → time 后补 12 字节，否则 lightVP 错位到 260（glm::mat4 alignof=4）
        glm::mat4 lightVP;     // 光源的 view*proj（平行光正交投影）——阴影贴图渲染 + PBR 采样共用
    };
    static_assert(sizeof(GlobalUniforms) == 336, "GlobalUniforms 必须与 std140 布局一致（336 字节）");
    static_assert(offsetof(GlobalUniforms, lightVP) == 272, "lightVP 必须位于 std140 偏移 272");

    struct MaterialUniforms {
        glm::vec4 baseColor = glm::vec4(1.0f);
        float metallic = 0.0f;
        float roughness = 0.5f;
        float emissiveIntensity = 0.0f;
        glm::vec3 emissiveColor = glm::vec3(0.0f);
    };

    struct LightData {
        glm::vec4 position;      
        glm::vec4 color;        
    };

    struct LightingUniforms {
        LightData lights[MAX_LIGHTS];
        uint32_t lightCount;
        float ambientStrength;
        glm::vec2 padding;
    };

    enum  class VertexSemantic:uint8_t{
        Position,
        Normal,
        TexCoord0,
        Tangent,
        BoneIndices,     
        BoneWeights,     
        Color0,
        TexCoord1,
        InstanceMatrixRow0,
        InstanceMatrixRow1,
        InstanceMatrixRow2,
        InstanceMatrixRow3,
    };

    const std::unordered_map<VertexSemantic, uint32_t> DefaultSemanticLocation = {
        { VertexSemantic::Position,   0 },
        { VertexSemantic::Normal,     1 },
        { VertexSemantic::TexCoord0,  2 },
        { VertexSemantic::Tangent,    3 },
        { VertexSemantic::Color0,     4 },
        { VertexSemantic::TexCoord1,  5 },
        { VertexSemantic::InstanceMatrixRow0, 6 },
        { VertexSemantic::InstanceMatrixRow1, 7 },
        { VertexSemantic::InstanceMatrixRow2, 8 },
        { VertexSemantic::InstanceMatrixRow3, 9 },
    };

    class VertexLayout {
    public:
        VertexLayout& addBinding(uint32_t binding, uint32_t stride,
            RHI::VertexInputRate inputRate = RHI::VertexInputRate::PerVertex);

        VertexLayout& addAttribute(uint32_t location, uint32_t binding, RHI::Format format, uint32_t offset);
        VertexLayout& addAttribute(uint32_t location, uint32_t binding, RHI::Format format);

        VertexLayout& addAttribute(VertexSemantic semantic, uint32_t binding,RHI::Format format);

        void setSemanticMapping(const std::unordered_map<VertexSemantic, uint32_t>& mapping) {
            m_semanticMapping = mapping;
            m_hasCustomMapping = true;
        }
        uint32_t getLocationForSemantic(VertexSemantic sem) const;

        RHI::VertexInputState build() const;
        uint32_t getBindingStride(uint32_t binding) const;
        std::vector<uint32_t> getBindings() const;
        VertexLayout& merge(const VertexLayout& other);
        static VertexLayout makeInstancingLayout(uint32_t binding = 1);
        void print() const;
        static uint32_t getFormatSize(RHI::Format format);

        struct AppVertexAttribute {
            VertexSemantic semantic;
            uint32_t binding;
            RHI::Format format;
            uint32_t offset;
        };
        const std::vector<AppVertexAttribute>& getAppAttributes() const { return m_appAttributes; }

    private:
        uint32_t getNextOffset(uint32_t binding) const;
        bool m_hasCustomMapping = false;

        struct BindingInfo {
            uint32_t stride;
            RHI::VertexInputRate inputRate;
            bool autoStride = false;
        };
        std::unordered_map<uint32_t, BindingInfo> mBindings;
        std::vector<RHI::VertexAttribute> mAttributes;          
        std::vector<AppVertexAttribute> m_appAttributes;         

        mutable std::unordered_map<uint32_t, uint32_t> mBindingCurrentOffsets;
        std::unordered_map<VertexSemantic, uint32_t> m_semanticMapping;
    };

    struct InstancingAttribute {
        uint32_t location;      
        RHI::Format format;     
        uint32_t offset;      
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
            RHI::VertexBinding vb;
            vb.binding = binding;
            vb.stride = stride;
            vb.inputRate = RHI::VertexInputRate::PerInstance;
            state.bindings.push_back(vb);

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