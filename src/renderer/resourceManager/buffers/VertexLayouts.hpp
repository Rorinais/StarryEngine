#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace StarryEngine {

    struct VertexAttribute {
        uint32_t location;
        VkFormat format;
        size_t offset;
        const char* name;

        VertexAttribute(uint32_t loc, VkFormat fmt, size_t off, const char* n = nullptr)
            : location(loc), format(fmt), offset(off), name(n) {
        }
    };

    struct VertexLayout {
        uint32_t binding;
        uint32_t stride;
        std::vector<VertexAttribute> attributes;

        VertexLayout& addAttribute(uint32_t location, VkFormat format,
            size_t offset, const char* name = nullptr) {
            attributes.emplace_back(location, format, offset, name);
            return *this;
        }
    };

    enum class BufferMode {
        INTERLEAVED,
        SEPARATED
    };

    struct VertexPos {
        glm::vec3 position;
    };

    struct VertexPosColor {
        glm::vec3 position;
        glm::vec3 color;
    };

    struct VertexPosTex {
        glm::vec3 position;
        glm::vec2 texCoord;
    };

    struct VertexPosNormalTex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;
    };
}