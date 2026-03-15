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

    class VertexLayout {
    public:
        VertexLayout& addBinding(uint32_t binding, uint32_t stride,
            RHI::VertexInputRate inputRate = RHI::VertexInputRate::PerVertex);
        VertexLayout& addAttribute(uint32_t location, uint32_t binding,
            RHI::Format format, uint32_t offset);
        VertexLayout& addAttribute(uint32_t location, uint32_t binding, RHI::Format format);
        RHI::VertexInputState build() const;

        // 获取指定 binding 的 stride，用于创建顶点缓冲区
        uint32_t getBindingStride(uint32_t binding) const;

        // 获取所有已定义的 binding 索引（升序）
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