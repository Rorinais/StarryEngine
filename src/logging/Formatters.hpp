#pragma once
#include <glm/glm.hpp>
#include <fmt/fmt.h>
#include"../assets/AssetType.hpp"

template <>
struct fmt::formatter<glm::vec3> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    // 必须添加 const 限定符
    template <typename FormatContext>
    auto format(const glm::vec3& v, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "({:.2f}, {:.2f}, {:.2f})", v.x, v.y, v.z);
    }
};

// 为 glm::vec4 提供格式化器
template <>
struct fmt::formatter<glm::vec4> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const glm::vec4& v, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "({:.2f}, {:.2f}, {:.2f}, {:.2f})", v.x, v.y, v.z, v.w);
    }
};

// 为 glm::mat4 提供格式化器
template <>
struct fmt::formatter<glm::mat4> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    // 必须添加 const 限定符
    template <typename FormatContext>
    auto format(const glm::mat4& m, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "[{:.2f}, {:.2f}, {:.2f}, {:.2f}]",
            m[0][0], m[0][1], m[0][2], m[0][3]);
    }
};

// 为 CullMode 提供格式化器
template <>
struct fmt::formatter<StarryEngine::RHI::CullMode> : fmt::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(StarryEngine::RHI::CullMode mode, FormatContext& ctx) const {
        std::string_view name = "Unknown";
        switch (mode) {
        case StarryEngine::RHI::CullMode::None:  name = "None"; break;
        case StarryEngine::RHI::CullMode::Front: name = "Front"; break;
        case StarryEngine::RHI::CullMode::Back:  name = "Back"; break;
        }
        return fmt::formatter<std::string_view>::format(name, ctx);
    }
};

// 为 LogicOp 提供格式化器
template <>
struct fmt::formatter<StarryEngine::RHI::LogicOp> : fmt::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(StarryEngine::RHI::LogicOp op, FormatContext& ctx) const {
        std::string_view name = "Unknown";
        switch (op) {
        case StarryEngine::RHI::LogicOp::Copy:  name = "Copy"; break;
        case StarryEngine::RHI::LogicOp::Clear: name = "Clear"; break;
            // 添加其他枚举值
        }
        return fmt::formatter<std::string_view>::format(name, ctx);
    }
};

// 为 CompareOp 提供格式化器
template <>
struct fmt::formatter<StarryEngine::RHI::CompareOp> : fmt::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(StarryEngine::RHI::CompareOp op, FormatContext& ctx) const {
        std::string_view name = "Unknown";
        switch (op) {
        case StarryEngine::RHI::CompareOp::Never:          name = "Never"; break;
        case StarryEngine::RHI::CompareOp::Less:           name = "Less"; break;
        case StarryEngine::RHI::CompareOp::Equal:          name = "Equal"; break;
        case StarryEngine::RHI::CompareOp::LessOrEqual:    name = "LessOrEqual"; break;
        case StarryEngine::RHI::CompareOp::Greater:        name = "Greater"; break;
        case StarryEngine::RHI::CompareOp::NotEqual:       name = "NotEqual"; break;
        case StarryEngine::RHI::CompareOp::GreaterOrEqual: name = "GreaterOrEqual"; break;
        case StarryEngine::RHI::CompareOp::Always:         name = "Always"; break;
        }
        return fmt::formatter<std::string_view>::format(name, ctx);
    }
};


// 为 MaterialParams 特化 fmt::formatter
template <>
struct fmt::formatter<StarryEngine::Assets::MaterialParams> {
    // 解析格式字符串（此处忽略，直接使用默认）
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    // 格式化函数
    template <typename FormatContext>
    auto format(const StarryEngine::Assets::MaterialParams& p, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
            "MaterialParams {{\n"
            "  name: {}\n"
            "  index: {}\n"
            "  vertexShader: {}\n"
            "  fragmentShader: {}\n"
            "  baseColor: {}\n"
            "  emissiveColor: {} (intensity: {})\n"
            "  metallic: {:.2f}, roughness: {:.2f}, ao: {:.2f}\n"
            "  alphaThreshold: {:.2f}, alphaBlend: {}\n"
            "  textures:\n"
            "    albedo: {}\n"
            "    normal: {}\n"
            "    metallic: {}\n"
            "    roughness: {}\n"
            "    occlusion: {}\n"
            "    emissive: {}\n"
            "    opacity: {}\n"
            "  flags:\n"
            "    useNormalMap: {}, useEmissiveMap: {}, useAlphaTest: {}, doubleSided: {}\n"
            "}}",
            p.name, p.index,
            p.vertexShaderPath, p.fragmentShaderPath,
            p.baseColor,
            p.emissiveColor, p.emissiveIntensity,
            p.metallic, p.roughness, p.ambientOcclusion,
            p.alphaThreshold, p.alphaBlend,
            p.albedoTexture, p.normalTexture, p.metallicTexture,
            p.roughnessTexture, p.occlusionTexture, p.emissiveTexture, p.opacityTexture,
            p.useNormalMap, p.useEmissiveMap, p.useAlphaTest, p.doubleSided
        );
    }
};
