#pragma once 
#include <cstddef>
#include <vector>
#include <functional>
#include"../renderer/interface/RHI_ENUMS.hpp"
#include"../renderer/interface/RHI_HANDLES_SYSTEM.hpp"
namespace StarryEngine::Utils {

    // 基础 hash_combine（用于单个值）
    template <class T>
    inline void hash_combine(std::size_t& seed, const T& v) {
        seed ^= std::hash<T>{}(v)+0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    // 为 std::vector 特化的 hash_combine（递归组合每个元素）
    template <typename T>
    inline void hash_combine(std::size_t& seed, const std::vector<T>& vec) {
        for (const auto& elem : vec) {
            hash_combine(seed, elem);
        }
    }

}

namespace std {
    template<>
    struct hash<StarryEngine::RHI::TextureHandle> {
        size_t operator()(const StarryEngine::RHI::TextureHandle& handle) const noexcept {
            return typename StarryEngine::RHI::TextureHandle::Hash{}(handle);
        }
    };

    template<>
    struct hash<StarryEngine::RHI::PipelineLayoutHandle> {
        size_t operator()(const StarryEngine::RHI::PipelineLayoutHandle& handle) const noexcept {
            return typename StarryEngine::RHI::PipelineLayoutHandle::Hash{}(handle);
        }
    };

    template<>
    struct hash<StarryEngine::RHI::ShaderHandle> {
        size_t operator()(const StarryEngine::RHI::ShaderHandle& handle) const noexcept {
            return typename StarryEngine::RHI::ShaderHandle::Hash{}(handle);
        }
    };

    // RHI::Viewport
    template<> struct hash<StarryEngine::RHI::Viewport> {
        size_t operator()(const StarryEngine::RHI::Viewport& vp) const {
            size_t seed = 0;
            StarryEngine::Utils::hash_combine(seed, vp.x);
            StarryEngine::Utils::hash_combine(seed, vp.y);
            StarryEngine::Utils::hash_combine(seed, vp.width);
            StarryEngine::Utils::hash_combine(seed, vp.height);
            StarryEngine::Utils::hash_combine(seed, vp.minDepth);
            StarryEngine::Utils::hash_combine(seed, vp.maxDepth);
            return seed;
        }
    };

    // RHI::Rect2D
    template<> struct hash<StarryEngine::RHI::Rect2D> {
        size_t operator()(const StarryEngine::RHI::Rect2D& rect) const {
            size_t seed = 0;
            StarryEngine::Utils::hash_combine(seed, rect.offset.x);
            StarryEngine::Utils::hash_combine(seed, rect.offset.y);
            StarryEngine::Utils::hash_combine(seed, rect.extent.width);
            StarryEngine::Utils::hash_combine(seed, rect.extent.height);
            return seed;
        }
    };

    // RHI::BlendAttachmentState
    template<> struct hash<StarryEngine::RHI::BlendAttachmentState> {
        size_t operator()(const StarryEngine::RHI::BlendAttachmentState& bs) const {
            size_t seed = 0;
            StarryEngine::Utils::hash_combine(seed, bs.blendEnable);
            StarryEngine::Utils::hash_combine(seed, bs.srcColorBlendFactor);
            StarryEngine::Utils::hash_combine(seed, bs.dstColorBlendFactor);
            StarryEngine::Utils::hash_combine(seed, bs.colorBlendOp);
            StarryEngine::Utils::hash_combine(seed, bs.srcAlphaBlendFactor);
            StarryEngine::Utils::hash_combine(seed, bs.dstAlphaBlendFactor);
            StarryEngine::Utils::hash_combine(seed, bs.alphaBlendOp);
            StarryEngine::Utils::hash_combine(seed, bs.colorWriteMask);
            return seed;
        }
    };

    // RHI::VertexBinding
    template<> struct hash<StarryEngine::RHI::VertexBinding> {
        size_t operator()(const StarryEngine::RHI::VertexBinding& vb) const {
            size_t seed = 0;
            StarryEngine::Utils::hash_combine(seed, vb.binding);
            StarryEngine::Utils::hash_combine(seed, vb.stride);
            StarryEngine::Utils::hash_combine(seed, vb.inputRate);
            return seed;
        }
    };

    // RHI::VertexAttribute
    template<> struct hash<StarryEngine::RHI::VertexAttribute> {
        size_t operator()(const StarryEngine::RHI::VertexAttribute& attr) const {
            size_t seed = 0;
            StarryEngine::Utils::hash_combine(seed, attr.location);
            StarryEngine::Utils::hash_combine(seed, attr.binding);
            StarryEngine::Utils::hash_combine(seed, attr.format);
            StarryEngine::Utils::hash_combine(seed, attr.offset);
            return seed;
        }
    };

    // RHI::VertexInputState
    template<> struct hash<StarryEngine::RHI::VertexInputState> {
        size_t operator()(const StarryEngine::RHI::VertexInputState& vis) const {
            size_t seed = 0;
            StarryEngine::Utils::hash_combine(seed, vis.bindings);
            StarryEngine::Utils::hash_combine(seed, vis.attributes);
            return seed;
        }
    };
}



