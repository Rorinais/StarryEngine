#pragma once
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <renderer/interface/RHI_RESOURCE_MANAGER.hpp>

namespace StarryEngine::Assets {
    struct MaterialParams {
        std::string name = "Material";
        uint32_t index = 0;

        std::string vertexShaderPath;
        std::string fragmentShaderPath;

        glm::vec4 baseColor = glm::vec4(1.0f);
        glm::vec3 emissiveColor = glm::vec3(0.0f);
        float emissiveIntensity = 0.0f;

        float metallic = 0.0f;
        float roughness = 0.5f;
        float ambientOcclusion = 1.0f;

        float alphaThreshold = 0.0f;
        bool alphaBlend = false;

        std::string albedoTexture;
        std::string normalTexture;
        std::string metallicTexture;
        std::string roughnessTexture;
        std::string occlusionTexture;
        std::string emissiveTexture;
        std::string opacityTexture;

        RHI::TextureHandle albedoTextureHandle;
        RHI::TextureHandle normalTextureHandle;
        RHI::TextureHandle metallicTextureHandle;
        RHI::TextureHandle roughnessTextureHandle;
        RHI::TextureHandle occlusionTextureHandle;
        RHI::TextureHandle emissiveTextureHandle;
        RHI::TextureHandle opacityTextureHandle;

        bool useNormalMap = false;
        bool useEmissiveMap = false;
        bool useAlphaTest = false;
        bool doubleSided = false;
    };

    struct ShaderCreateInfo {
        RHI::ShaderHandle module;                   
        std::vector<uint32_t> spirv;               
        std::vector<RHI::VertexAttribute> vertexAttributes; 

        RHI::ShaderReflectionInfo reflection;

        std::unordered_map<uint32_t, RHI::DescriptorSetLayoutDesc> layoutDescs;
    };

    struct TextureLoadResult {
        RHI::TextureHandle texture;
        RHI::SamplerHandle sampler;
    };
}
