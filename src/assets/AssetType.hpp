#pragma once
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../renderer/interface/RHI_RESOURCE_MANAGER.hpp"

namespace StarryEngine::Assets {
	//struct
    struct MaterialParams {
        // 基础标识
        std::string name = "Material";
        uint32_t index = 0;

        //--- shader ---
        std::string vertexShaderPath;
        std::string fragmentShaderPath;

        // --- 颜色参数 ---
        glm::vec4 baseColor = glm::vec4(1.0f);
        glm::vec3 emissiveColor = glm::vec3(0.0f);
        float emissiveIntensity = 0.0f;

        // --- PBR 参数 ---
        float metallic = 0.0f;
        float roughness = 0.5f;
        float ambientOcclusion = 1.0f;

        // --- 透明度 ---
        float alphaThreshold = 0.0f;
        bool alphaBlend = false;

        // --- 贴图资源句柄 ---
        std::string albedoTexture;
        std::string normalTexture;
        std::string metallicTexture;
        std::string roughnessTexture;
        std::string occlusionTexture;
        std::string emissiveTexture;
        std::string opacityTexture;

        // --- 着色器变体标志 ---
        bool useNormalMap = false;
        bool useEmissiveMap = false;
        bool useAlphaTest = false;
        bool doubleSided = false;

        // --- 渲染状态覆盖 ---
        RHI::CullMode cullMode = RHI::CullMode::Back;
        RHI::LogicOp blendMode = RHI::LogicOp::Copy;
        RHI::CompareOp depthCompare = RHI::CompareOp::Less;
        bool depthWrite = true;
    };

    struct ShaderCreateInfo {
        RHI::ShaderHandle module;                     // 已创建的模块句柄
        std::vector<uint32_t> spirv;                        // 编译后的 SPIR-V（可选保留）
        std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> setLayouts; // 每个 set 的布局句柄
        std::vector<RHI::VertexAttribute> vertexAttributes; // 顶点输入描述（用于管线创建）
    };

    struct TextureLoadResult {
        RHI::TextureHandle texture;
        RHI::SamplerHandle sampler;
    };
	//enum
}
