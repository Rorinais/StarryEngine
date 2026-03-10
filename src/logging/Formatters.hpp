#pragma once
#include <glm/glm.hpp>
#include <fmt/fmt.h>

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