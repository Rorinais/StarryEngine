#pragma once
#include <glm/glm.hpp>
#include <fmt/fmt.h>
#include <fmt/ranges.h>
#include"../assets/AssetType.hpp"
#include "../renderer/graph/Types.hpp"

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


template <>
struct fmt::formatter<StarryEngine::RenderGraph::TextureId> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const StarryEngine::RenderGraph::TextureId& id, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "TextureId({})", id.id());
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

template <>
struct fmt::formatter<StarryEngine::RHI::Format> : fmt::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(StarryEngine::RHI::Format f, FormatContext& ctx) const {
        std::string_view name = "Undefined";
        switch (f) {
        case StarryEngine::RHI::Format::Undefined:          name = "Undefined"; break;
        case StarryEngine::RHI::Format::R8_UNorm:           name = "R8_UNorm"; break;
        case StarryEngine::RHI::Format::R8_SNorm:           name = "R8_SNorm"; break;
        case StarryEngine::RHI::Format::R8_UInt:            name = "R8_UInt"; break;
        case StarryEngine::RHI::Format::R8_SInt:            name = "R8_SInt"; break;
        case StarryEngine::RHI::Format::R8_sRGB:            name = "R8_sRGB"; break;
        case StarryEngine::RHI::Format::R16_UNorm:          name = "R16_UNorm"; break;
        case StarryEngine::RHI::Format::R16_SNorm:          name = "R16_SNorm"; break;
        case StarryEngine::RHI::Format::R16_UInt:           name = "R16_UInt"; break;
        case StarryEngine::RHI::Format::R16_SInt:           name = "R16_SInt"; break;
        case StarryEngine::RHI::Format::R16_Float:          name = "R16_Float"; break;
        case StarryEngine::RHI::Format::RG8_UNorm:          name = "RG8_UNorm"; break;
        case StarryEngine::RHI::Format::RG8_SNorm:          name = "RG8_SNorm"; break;
        case StarryEngine::RHI::Format::RG8_UInt:           name = "RG8_UInt"; break;
        case StarryEngine::RHI::Format::RG8_SInt:           name = "RG8_SInt"; break;
        case StarryEngine::RHI::Format::R32_UInt:           name = "R32_UInt"; break;
        case StarryEngine::RHI::Format::R32_SInt:           name = "R32_SInt"; break;
        case StarryEngine::RHI::Format::R32_Float:          name = "R32_Float"; break;
        case StarryEngine::RHI::Format::RG16_UNorm:         name = "RG16_UNorm"; break;
        case StarryEngine::RHI::Format::RG16_SNorm:         name = "RG16_SNorm"; break;
        case StarryEngine::RHI::Format::RG16_UInt:          name = "RG16_UInt"; break;
        case StarryEngine::RHI::Format::RG16_SInt:          name = "RG16_SInt"; break;
        case StarryEngine::RHI::Format::RG16_Float:         name = "RG16_Float"; break;
        case StarryEngine::RHI::Format::RGBA8_UNorm:        name = "RGBA8_UNorm"; break;
        case StarryEngine::RHI::Format::RGBA8_SNorm:        name = "RGBA8_SNorm"; break;
        case StarryEngine::RHI::Format::RGBA8_UInt:         name = "RGBA8_UInt"; break;
        case StarryEngine::RHI::Format::RGBA8_SInt:         name = "RGBA8_SInt"; break;
        case StarryEngine::RHI::Format::BGRA8_UNorm:        name = "BGRA8_UNorm"; break;
        case StarryEngine::RHI::Format::BGRA8_SNorm:        name = "BGRA8_SNorm"; break;
        case StarryEngine::RHI::Format::BGRA8_UInt:         name = "BGRA8_UInt"; break;
        case StarryEngine::RHI::Format::BGRA8_SInt:         name = "BGRA8_SInt"; break;
        case StarryEngine::RHI::Format::RGBA8_sRGB:         name = "RGBA8_sRGB"; break;
        case StarryEngine::RHI::Format::BGRA8_sRGB:         name = "BGRA8_sRGB"; break;
        case StarryEngine::RHI::Format::RG32_UInt:          name = "RG32_UInt"; break;
        case StarryEngine::RHI::Format::RG32_SInt:          name = "RG32_SInt"; break;
        case StarryEngine::RHI::Format::RG32_Float:         name = "RG32_Float"; break;
        case StarryEngine::RHI::Format::RGBA16_UNorm:       name = "RGBA16_UNorm"; break;
        case StarryEngine::RHI::Format::RGBA16_SNorm:       name = "RGBA16_SNorm"; break;
        case StarryEngine::RHI::Format::RGBA16_UInt:        name = "RGBA16_UInt"; break;
        case StarryEngine::RHI::Format::RGBA16_SInt:        name = "RGBA16_SInt"; break;
        case StarryEngine::RHI::Format::RGBA16_Float:       name = "RGBA16_Float"; break;
        case StarryEngine::RHI::Format::RGB32_UInt:         name = "RGB32_UInt"; break;
        case StarryEngine::RHI::Format::RGB32_SInt:         name = "RGB32_SInt"; break;
        case StarryEngine::RHI::Format::RGB32_Float:        name = "RGB32_Float"; break;
        case StarryEngine::RHI::Format::RGBA32_UInt:        name = "RGBA32_UInt"; break;
        case StarryEngine::RHI::Format::RGBA32_SInt:        name = "RGBA32_SInt"; break;
        case StarryEngine::RHI::Format::RGBA32_Float:       name = "RGBA32_Float"; break;
        case StarryEngine::RHI::Format::D16_UNorm:          name = "D16_UNorm"; break;
        case StarryEngine::RHI::Format::D32_Float:          name = "D32_Float"; break;
        case StarryEngine::RHI::Format::D24_UNorm_S8_UInt:  name = "D24_UNorm_S8_UInt"; break;
        case StarryEngine::RHI::Format::D32_Float_S8_UInt:  name = "D32_Float_S8_UInt"; break;
        case StarryEngine::RHI::Format::BC1_RGB_UNorm:      name = "BC1_RGB_UNorm"; break;
        case StarryEngine::RHI::Format::BC1_RGBA_UNorm:     name = "BC1_RGBA_UNorm"; break;
        case StarryEngine::RHI::Format::BC1_RGB_sRGB:       name = "BC1_RGB_sRGB"; break;
        case StarryEngine::RHI::Format::BC1_RGBA_sRGB:      name = "BC1_RGBA_sRGB"; break;
        case StarryEngine::RHI::Format::BC2_UNorm:          name = "BC2_UNorm"; break;
        case StarryEngine::RHI::Format::BC2_sRGB:           name = "BC2_sRGB"; break;
        case StarryEngine::RHI::Format::BC3_UNorm:          name = "BC3_UNorm"; break;
        case StarryEngine::RHI::Format::BC3_sRGB:           name = "BC3_sRGB"; break;
        case StarryEngine::RHI::Format::BC4_UNorm:          name = "BC4_UNorm"; break;
        case StarryEngine::RHI::Format::BC4_SNorm:          name = "BC4_SNorm"; break;
        case StarryEngine::RHI::Format::BC5_UNorm:          name = "BC5_UNorm"; break;
        case StarryEngine::RHI::Format::BC5_SNorm:          name = "BC5_SNorm"; break;
        case StarryEngine::RHI::Format::BC6H_UF16:          name = "BC6H_UF16"; break;
        case StarryEngine::RHI::Format::BC6H_SF16:          name = "BC6H_SF16"; break;
        case StarryEngine::RHI::Format::BC7_UNorm:          name = "BC7_UNorm"; break;
        case StarryEngine::RHI::Format::BC7_sRGB:           name = "BC7_sRGB"; break;
        case StarryEngine::RHI::Format::ASTC_4x4_UNorm:     name = "ASTC_4x4_UNorm"; break;
        case StarryEngine::RHI::Format::ASTC_4x4_sRGB:      name = "ASTC_4x4_sRGB"; break;
        case StarryEngine::RHI::Format::ASTC_8x8_UNorm:     name = "ASTC_8x8_UNorm"; break;
        case StarryEngine::RHI::Format::ASTC_8x8_sRGB:      name = "ASTC_8x8_sRGB"; break;
        case StarryEngine::RHI::Format::ETC2_RGB8_UNorm:    name = "ETC2_RGB8_UNorm"; break;
        case StarryEngine::RHI::Format::ETC2_RGB8_sRGB:     name = "ETC2_RGB8_sRGB"; break;
        case StarryEngine::RHI::Format::ETC2_RGBA8_UNorm:   name = "ETC2_RGBA8_UNorm"; break;
        case StarryEngine::RHI::Format::ETC2_RGBA8_sRGB:    name = "ETC2_RGBA8_sRGB"; break;
        case StarryEngine::RHI::Format::EAC_R11_UNorm:      name = "EAC_R11_UNorm"; break;
        case StarryEngine::RHI::Format::EAC_R11_SNorm:      name = "EAC_R11_SNorm"; break;
        case StarryEngine::RHI::Format::EAC_RG11_UNorm:     name = "EAC_RG11_UNorm"; break;
        case StarryEngine::RHI::Format::EAC_RG11_SNorm:     name = "EAC_RG11_SNorm"; break;
        }
        return fmt::formatter<std::string_view>::format(name, ctx);
    }
};

template <>
struct fmt::formatter<StarryEngine::RHI::DescriptorType> : fmt::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(StarryEngine::RHI::DescriptorType dt, FormatContext& ctx) const {
        std::string_view name = "Unknown";
        switch (dt) {
        case StarryEngine::RHI::DescriptorType::Sampler:                name = "Sampler"; break;
        case StarryEngine::RHI::DescriptorType::CombinedImageSampler:   name = "CombinedImageSampler"; break;
        case StarryEngine::RHI::DescriptorType::SampledImage:           name = "SampledImage"; break;
        case StarryEngine::RHI::DescriptorType::StorageImage:           name = "StorageImage"; break;
        case StarryEngine::RHI::DescriptorType::UniformTexelBuffer:     name = "UniformTexelBuffer"; break;
        case StarryEngine::RHI::DescriptorType::StorageTexelBuffer:     name = "StorageTexelBuffer"; break;
        case StarryEngine::RHI::DescriptorType::UniformBuffer:          name = "UniformBuffer"; break;
        case StarryEngine::RHI::DescriptorType::StorageBuffer:          name = "StorageBuffer"; break;
        case StarryEngine::RHI::DescriptorType::UniformBufferDynamic:   name = "UniformBufferDynamic"; break;
        case StarryEngine::RHI::DescriptorType::StorageBufferDynamic:   name = "StorageBufferDynamic"; break;
        case StarryEngine::RHI::DescriptorType::InputAttachment:        name = "InputAttachment"; break;
        case StarryEngine::RHI::DescriptorType::AccelerationStructure:  name = "AccelerationStructure"; break;
        case StarryEngine::RHI::DescriptorType::InlineUniformBlock:     name = "InlineUniformBlock"; break;
        }
        return fmt::formatter<std::string_view>::format(name, ctx);
    }
};

template <>
struct fmt::formatter<StarryEngine::RHI::ShaderStage> : fmt::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(StarryEngine::RHI::ShaderStage s, FormatContext& ctx) const {
        std::string_view name = "Unknown";
        switch (s) {
        case StarryEngine::RHI::ShaderStage::Vertex:                 name = "Vertex"; break;
        case StarryEngine::RHI::ShaderStage::TessellationControl:    name = "TessControl"; break;
        case StarryEngine::RHI::ShaderStage::TessellationEvaluation: name = "TessEval"; break;
        case StarryEngine::RHI::ShaderStage::Geometry:               name = "Geometry"; break;
        case StarryEngine::RHI::ShaderStage::Fragment:               name = "Fragment"; break;
        case StarryEngine::RHI::ShaderStage::Compute:                name = "Compute"; break;
        case StarryEngine::RHI::ShaderStage::RayGen:                 name = "RayGen"; break;
        case StarryEngine::RHI::ShaderStage::AnyHit:                 name = "AnyHit"; break;
        case StarryEngine::RHI::ShaderStage::ClosestHit:             name = "ClosestHit"; break;
        case StarryEngine::RHI::ShaderStage::Miss:                   name = "Miss"; break;
        case StarryEngine::RHI::ShaderStage::Intersection:           name = "Intersection"; break;
        case StarryEngine::RHI::ShaderStage::Callable:               name = "Callable"; break;
        case StarryEngine::RHI::ShaderStage::Amplification:          name = "Amplification"; break;
        case StarryEngine::RHI::ShaderStage::Mesh:                   name = "Mesh"; break;
        }
        return fmt::formatter<std::string_view>::format(name, ctx);
    }
};

template <>
struct fmt::formatter<StarryEngine::RHI::ShaderStageFlags> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const StarryEngine::RHI::ShaderStageFlags& flags, FormatContext& ctx) const {
        std::vector<std::string> stages;
        // 检查每个已知阶段
        auto tryAdd = [&](StarryEngine::RHI::ShaderStage s, const char* name) {
            if (flags.Has(s)) stages.push_back(name);
            };
        tryAdd(StarryEngine::RHI::ShaderStage::Vertex, "Vertex");
        tryAdd(StarryEngine::RHI::ShaderStage::TessellationControl, "TessControl");
        tryAdd(StarryEngine::RHI::ShaderStage::TessellationEvaluation, "TessEval");
        tryAdd(StarryEngine::RHI::ShaderStage::Geometry, "Geometry");
        tryAdd(StarryEngine::RHI::ShaderStage::Fragment, "Fragment");
        tryAdd(StarryEngine::RHI::ShaderStage::Compute, "Compute");
        tryAdd(StarryEngine::RHI::ShaderStage::Amplification, "Amplification");
        tryAdd(StarryEngine::RHI::ShaderStage::Mesh, "Mesh");
        tryAdd(StarryEngine::RHI::ShaderStage::RayGen, "RayGen");
        tryAdd(StarryEngine::RHI::ShaderStage::AnyHit, "AnyHit");
        tryAdd(StarryEngine::RHI::ShaderStage::ClosestHit, "ClosestHit");
        tryAdd(StarryEngine::RHI::ShaderStage::Miss, "Miss");
        tryAdd(StarryEngine::RHI::ShaderStage::Intersection, "Intersection");
        tryAdd(StarryEngine::RHI::ShaderStage::Callable, "Callable");

        std::string out;
        for (size_t i = 0; i < stages.size(); ++i) {
            if (i > 0) out += " | ";
            out += stages[i];
        }
        if (out.empty()) out = "None";
        return fmt::format_to(ctx.out(), "{}", out);
    }
};

template <>
struct fmt::formatter<StarryEngine::RHI::TextureDimension> : fmt::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(StarryEngine::RHI::TextureDimension dim, FormatContext& ctx) const {
        std::string_view name = "Unknown";
        switch (dim) {
        case StarryEngine::RHI::TextureDimension::Unknown: name = "Unknown"; break;
        case StarryEngine::RHI::TextureDimension::Tex1D: name = "1D"; break;
        case StarryEngine::RHI::TextureDimension::Tex2D: name = "2D"; break;
        case StarryEngine::RHI::TextureDimension::Tex3D: name = "3D"; break;
        case StarryEngine::RHI::TextureDimension::Cube:  name = "Cube"; break;
        case StarryEngine::RHI::TextureDimension::Tex2DArray:  name = "Tex2DArray"; break;
        case StarryEngine::RHI::TextureDimension::CubeArray:  name = "CubeArray"; break;
        }
        return fmt::formatter<std::string_view>::format(name, ctx);
    }
};

// ──── BufferMember ────
template <>
struct fmt::formatter<StarryEngine::RHI::BufferMember> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const StarryEngine::RHI::BufferMember& m, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
            "(name: {}, offset: {}, size: {}, format: {})",
            m.name, m.offset, m.size, m.format);
    }
};

// ──── ResourceBinding::TextureInfo ────
template <>
struct fmt::formatter<StarryEngine::RHI::ResourceBinding::TextureInfo> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const StarryEngine::RHI::ResourceBinding::TextureInfo& info, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
            "(dim: {}, array: {}, ms: {}, fmt: {})",
            info.dimension, info.isArray, info.isMultisample, info.imageFormat);
    }
};

// ──── ResourceBinding ────
template <>
struct fmt::formatter<StarryEngine::RHI::ResourceBinding> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const StarryEngine::RHI::ResourceBinding& rb, FormatContext& ctx) const {
        auto out = fmt::format_to(ctx.out(),
            "    ResourceBinding\n"
            "      name: {}\n"
            "      set: {}, binding: {}\n"
            "      type: {}\n"
            "      count: {}, stageFlags: {}\n",
            rb.name, rb.set, rb.binding, rb.type, rb.count, rb.stageFlags);
        if (!rb.members.empty()) {
            out = fmt::format_to(out, "      members:\n");
            for (const auto& m : rb.members)
                out = fmt::format_to(out, "        {}\n", m);
        }
        if (rb.texture.has_value()) {
            out = fmt::format_to(out, "      texture: {}\n", *rb.texture);
        }
        return out;
    }
};

// ──── PushConstant ────
template <>
struct fmt::formatter<StarryEngine::RHI::PushConstant> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const StarryEngine::RHI::PushConstant& pc, FormatContext& ctx) const {
        auto out = fmt::format_to(ctx.out(),
            "    PushConstant\n"
            "      name: {}\n"
            "      offset: {}, size: {}\n"
            "      stageFlags: {}\n",
            pc.name, pc.offset, pc.size, pc.stageFlags);
        if (!pc.members.empty()) {
            out = fmt::format_to(out, "      members:\n");
            for (const auto& m : pc.members)
                out = fmt::format_to(out, "        {}\n", m);
        }
        return out;
    }
};

// ──── SpecConstant ────
template <>
struct fmt::formatter<StarryEngine::RHI::SpecConstant> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const StarryEngine::RHI::SpecConstant& sc, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
            "SpecConstant(name: {}, id: {}, size: {})",
            sc.name, sc.constantId, sc.size);
    }
};

// ──── InputAttribute / OutputAttribute (如果希望单独打印) ────
template <>
struct fmt::formatter<StarryEngine::RHI::InputAttribute> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const StarryEngine::RHI::InputAttribute& a, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "(name: {}, loc: {}, fmt: {})", a.name, a.location, a.format);
    }
};

template <>
struct fmt::formatter<StarryEngine::RHI::OutputAttribute> {
    // 与 InputAttribute 几乎一样，也可复用
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const StarryEngine::RHI::OutputAttribute& a, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "(name: {}, loc: {}, fmt: {})", a.name, a.location, a.format);
    }
};

// ──── ShaderReflectionInfo ────
template <>
struct fmt::formatter<StarryEngine::RHI::ShaderReflectionInfo> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const StarryEngine::RHI::ShaderReflectionInfo& info, FormatContext& ctx) const {
        auto out = fmt::format_to(ctx.out(),"\nShaderReflectionInfo (stage: {})\n", info.shaderStage);

        constexpr uint32_t kWorkgroupStagesMask =
            static_cast<uint32_t>(StarryEngine::RHI::ShaderStage::Compute) |
            static_cast<uint32_t>(StarryEngine::RHI::ShaderStage::Amplification) |
            static_cast<uint32_t>(StarryEngine::RHI::ShaderStage::Mesh);

        if (static_cast<uint32_t>(info.shaderStage) & kWorkgroupStagesMask) {
            out = fmt::format_to(out, "  workgroup size: ({}, {}, {})\n",
                info.workGroupSizeX, info.workGroupSizeY, info.workGroupSizeZ);
        }

        if (!info.inputAttributes.empty()) {
            out = fmt::format_to(out, "  input attributes:\n");
            for (const auto& attr : info.inputAttributes)
                out = fmt::format_to(out, "    {}\n", attr);
        }
        if (!info.outputAttributes.empty()) {
            out = fmt::format_to(out, "  output attributes:\n");
            for (const auto& attr : info.outputAttributes)
                out = fmt::format_to(out, "    {}\n", attr);
        }
        if (!info.resourceBindings.empty()) {
            out = fmt::format_to(out, "  resource bindings:\n");
            for (const auto& rb : info.resourceBindings)
                out = fmt::format_to(out, "{}", rb); 
        }
        if (!info.pushConstants.empty()) {
            out = fmt::format_to(out, "  push constants:\n");
            for (const auto& pc : info.pushConstants)
                out = fmt::format_to(out, "{}", pc);
        }
        if (!info.specConstants.empty()) {
            out = fmt::format_to(out, "  spec constants:\n");
            for (const auto& sc : info.specConstants)
                out = fmt::format_to(out, "    {}\n", sc);
        }
        return out;
    }
};