#pragma once
#include "RHI_ENUMS.hpp"
#include <cstdint>
#include <array>
#include <glm/glm.hpp>

namespace StarryEngine::RHI {

    // ==================== 基础几何和数学结构体 ====================

    /**
     * @brief 版本号结构体
     * @details 用于表示API版本，支持打包为32位整数
     */
    struct Version {
        uint32_t major = 1;
        uint32_t minor = 0;
        uint32_t patch = 0;

        constexpr uint32_t packed() const {
            return (major << 22) | (minor << 12) | patch;
        }
    };

    /**
     * @brief 2D范围结构体
     * @details 用于表示宽度和高度
     */
    struct Extent2D {
        uint32_t width = 0;
        uint32_t height = 0;

        bool operator==(const Extent2D& other) const {
            return width == other.width && height == other.height;
        }

        bool operator!=(const Extent2D& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 3D范围结构体
     * @details 继承自Extent2D，增加深度维度
     */
    struct Extent3D : Extent2D {
        uint32_t depth = 1;

        bool operator==(const Extent3D& other) const {
            return width == other.width && height == other.height && depth == other.depth;
        }

        bool operator!=(const Extent3D& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 2D偏移结构体
     * @details 用于表示二维空间中的偏移量
     */
    struct Offset2D {
        int32_t x = 0;
        int32_t y = 0;

        bool operator==(const Offset2D& other) const {
            return x == other.x && y == other.y;
        }

        bool operator!=(const Offset2D& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 3D偏移结构体
     * @details 继承自Offset2D，增加Z轴偏移
     */
    struct Offset3D : Offset2D {
        int32_t z = 0;

        bool operator==(const Offset3D& other) const {
            return x == other.x && y == other.y && z == other.z;
        }

        bool operator!=(const Offset3D& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 2D矩形结构体
     * @details 由偏移和范围组成，用于表示屏幕空间区域
     */
    struct Rect2D {
        Offset2D offset;
        Extent2D extent;

        bool operator==(const Rect2D& other) const {
            return offset == other.offset && extent == other.extent;
        }

        bool operator!=(const Rect2D& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 视口结构体
     * @details 用于定义视口变换参数
     */
    struct Viewport {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float minDepth = 0.0f;
        float maxDepth = 1.0f;

        bool operator==(const Viewport& other) const {
            return x == other.x && y == other.y &&
                width == other.width && height == other.height &&
                minDepth == other.minDepth && maxDepth == other.maxDepth;
        }

        bool operator!=(const Viewport& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 颜色结构体
     * @details 使用RGBA浮点格式表示颜色，提供常用颜色常量
     */
    struct Color {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;

        Color() = default;
        Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

        static Color Black() { return Color(0.0f, 0.0f, 0.0f, 1.0f); }
        static Color White() { return Color(1.0f, 1.0f, 1.0f, 1.0f); }
        static Color Red() { return Color(1.0f, 0.0f, 0.0f, 1.0f); }
        static Color Green() { return Color(0.0f, 1.0f, 0.0f, 1.0f); }
        static Color Blue() { return Color(0.0f, 0.0f, 1.0f, 1.0f); }
        static Color Transparent() { return Color(0.0f, 0.0f, 0.0f, 0.0f); }

        bool operator==(const Color& other) const {
            return r == other.r && g == other.g && b == other.b && a == other.a;
        }

        bool operator!=(const Color& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 清除值结构体
     * @details 用于渲染目标或深度/模板缓冲区的清除值
     */
    struct ClearValue {
        Color color;
        float depth = 1.0f;
        uint32_t stencil = 0;

        ClearValue() = default;
        explicit ClearValue(const Color& color) : color(color) {}
        ClearValue(float depth, uint32_t stencil = 0) : depth(depth), stencil(stencil) {}

        bool operator==(const ClearValue& other) const {
            return color == other.color && depth == other.depth && stencil == other.stencil;
        }

        bool operator!=(const ClearValue& other) const {
            return !(*this == other);
        }
    };

} // namespace StarryEngine::RHI