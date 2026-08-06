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

    template<>
    struct hash<StarryEngine::RHI::DescriptorSetLayoutHandle> {
        size_t operator()(const StarryEngine::RHI::DescriptorSetLayoutHandle& handle) const noexcept {
            return typename StarryEngine::RHI::DescriptorSetLayoutHandle::Hash{}(handle);
        }
    };

    template<>
    struct hash<StarryEngine::RHI::RenderPassHandle> {
        size_t operator()(const StarryEngine::RHI::RenderPassHandle& handle) const noexcept {
            return typename StarryEngine::RHI::RenderPassHandle::Hash{}(handle);
        }
    };

    // 枚举特化
    template<> struct hash<StarryEngine::RHI::ShaderStage> {
        size_t operator()(StarryEngine::RHI::ShaderStage val) const noexcept {
            return static_cast<size_t>(val);
        }
    };

    template<> struct hash<StarryEngine::RHI::ShaderStageFlags> {
        size_t operator()(const StarryEngine::RHI::ShaderStageFlags& flags) const noexcept {
            return static_cast<size_t>(static_cast<uint32_t>(flags));
        }
    };

    template<> struct hash<StarryEngine::RHI::DescriptorType> {
        size_t operator()(StarryEngine::RHI::DescriptorType val) const noexcept {
            return static_cast<size_t>(val);
        }
    };

    template<> struct hash<StarryEngine::RHI::CullMode> {
        size_t operator()(StarryEngine::RHI::CullMode val) const noexcept {
            return static_cast<size_t>(val);
        }
    };

    template<> struct hash<StarryEngine::RHI::FrontFace> {
        size_t operator()(StarryEngine::RHI::FrontFace val) const noexcept {
            return static_cast<size_t>(val);
        }
    };

    template<> struct hash<StarryEngine::RHI::CompareOp> {
        size_t operator()(StarryEngine::RHI::CompareOp val) const noexcept {
            return static_cast<size_t>(val);
        }
    };

    template<> struct hash<StarryEngine::RHI::PrimitiveTopology> {
        size_t operator()(StarryEngine::RHI::PrimitiveTopology val) const noexcept {
            return static_cast<size_t>(val);
        }
    };

    template<> struct hash<StarryEngine::RHI::DynamicState> {
        size_t operator()(StarryEngine::RHI::DynamicState val) const noexcept {
            return static_cast<size_t>(val);
        }
    };

    template<> struct hash<StarryEngine::RHI::VertexInputRate> {
        size_t operator()(StarryEngine::RHI::VertexInputRate val) const noexcept {
            return static_cast<size_t>(val);
        }
    };

    template<> struct hash<StarryEngine::RHI::Format> {
        size_t operator()(StarryEngine::RHI::Format val) const noexcept {
            return static_cast<size_t>(val);
        }
    };

    template<> struct hash<StarryEngine::RHI::SamplerFilter> {
        size_t operator()(StarryEngine::RHI::SamplerFilter val) const noexcept {
            return static_cast<size_t>(val);
        }
    };

    template<> struct hash<StarryEngine::RHI::SamplerAddressMode> {
        size_t operator()(StarryEngine::RHI::SamplerAddressMode val) const noexcept {
            return static_cast<size_t>(val);
        }
    };

    template<> struct hash<StarryEngine::RHI::SamplerBorderColor> {
        size_t operator()(StarryEngine::RHI::SamplerBorderColor val) const noexcept {
            return static_cast<size_t>(val);
        }
    };

    template<> struct hash<StarryEngine::RHI::BufferHandle> {
        size_t operator()(const StarryEngine::RHI::BufferHandle& handle) const noexcept {
            return typename StarryEngine::RHI::BufferHandle::Hash{}(handle);
        }
    };

    template<> struct hash<StarryEngine::RHI::DescriptorSetHandle> {
        size_t operator()(const StarryEngine::RHI::DescriptorSetHandle& handle) const noexcept {
            return typename StarryEngine::RHI::DescriptorSetHandle::Hash{}(handle);
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

    // RHI::StencilOpState
    template<> struct hash<StarryEngine::RHI::StencilOpState> {
        size_t operator()(const StarryEngine::RHI::StencilOpState& s) const {
            size_t seed = 0;
            StarryEngine::Utils::hash_combine(seed, static_cast<size_t>(s.failOp));
            StarryEngine::Utils::hash_combine(seed, static_cast<size_t>(s.passOp));
            StarryEngine::Utils::hash_combine(seed, static_cast<size_t>(s.depthFailOp));
            StarryEngine::Utils::hash_combine(seed, static_cast<size_t>(s.compareOp));
            StarryEngine::Utils::hash_combine(seed, s.compareMask);
            StarryEngine::Utils::hash_combine(seed, s.writeMask);
            StarryEngine::Utils::hash_combine(seed, s.reference);
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

    template<> struct hash<StarryEngine::RHI::PushConstantRange> {
        size_t operator()(const StarryEngine::RHI::PushConstantRange& pc) const {
            size_t seed = 0;
            StarryEngine::Utils::hash_combine(seed, pc.stageFlags);
            StarryEngine::Utils::hash_combine(seed, pc.offset);
            StarryEngine::Utils::hash_combine(seed, pc.size);
            return seed;
        }
    };

    template<> struct hash<StarryEngine::RHI::SamplerDesc> {
        size_t operator()(const StarryEngine::RHI::SamplerDesc& desc) const noexcept {
            size_t seed = 0;
            StarryEngine::Utils::hash_combine(seed, desc.minFilter);
            StarryEngine::Utils::hash_combine(seed, desc.magFilter);
            StarryEngine::Utils::hash_combine(seed, desc.mipFilter);
            StarryEngine::Utils::hash_combine(seed, desc.addressU);
            StarryEngine::Utils::hash_combine(seed, desc.addressV);
            StarryEngine::Utils::hash_combine(seed, desc.addressW);
            StarryEngine::Utils::hash_combine(seed, desc.mipLodBias);
            StarryEngine::Utils::hash_combine(seed, desc.maxAnisotropy);
            StarryEngine::Utils::hash_combine(seed, desc.minLod);
            StarryEngine::Utils::hash_combine(seed, desc.maxLod);
            StarryEngine::Utils::hash_combine(seed, desc.compareEnable);
            StarryEngine::Utils::hash_combine(seed, desc.compareOp);
            StarryEngine::Utils::hash_combine(seed, desc.borderColor);
            StarryEngine::Utils::hash_combine(seed, desc.unnormalizedCoordinates);
            return seed;
        }
    };

    template<> struct hash<StarryEngine::RHI::DescriptorSetLayoutBinding> {
        size_t operator()(const StarryEngine::RHI::DescriptorSetLayoutBinding& binding) const noexcept {
            size_t seed = 0;
            StarryEngine::Utils::hash_combine(seed, binding.binding);
            StarryEngine::Utils::hash_combine(seed, binding.type);
            StarryEngine::Utils::hash_combine(seed, binding.count);
            StarryEngine::Utils::hash_combine(seed, binding.stageFlags);
            StarryEngine::Utils::hash_combine(seed, binding.immutableSamplers);
            StarryEngine::Utils::hash_combine(seed, binding.samplerDescs); // 自动组合 vector<SamplerDesc>
            return seed;
        }
    };

    template<> struct hash<StarryEngine::RHI::DescriptorSetLayoutDesc> {
        size_t operator()(const StarryEngine::RHI::DescriptorSetLayoutDesc& desc) const noexcept {
            size_t seed = 0;
            StarryEngine::Utils::hash_combine(seed, desc.bindings); // vector<DescriptorSetLayoutBinding>
            StarryEngine::Utils::hash_combine(seed, desc.pushDescriptors);
            StarryEngine::Utils::hash_combine(seed, desc.updateAfterBind);
            StarryEngine::Utils::hash_combine(seed, desc.updateUnusedWhilePending);
            return seed;
        }
    };
}



