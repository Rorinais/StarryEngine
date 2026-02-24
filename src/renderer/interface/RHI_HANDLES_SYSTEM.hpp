#pragma once
#include <cstdint>
#include <type_traits>
#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include "RHI_ENUMS.hpp"

namespace StarryEngine::RHI {
    // ==================== 基础句柄 ====================
    struct ResourceHandle {
        uint64_t id = 0;

        static constexpr uint8_t API_BITS = 4;      // 支持最多16个API
        static constexpr uint8_t CATEGORY_BITS = 6; // 支持最多64个资源类别
        static constexpr uint8_t INDEX_BITS = 22;   // 支持最多4百万个资源
        static constexpr uint8_t GENERATION_BITS = 32 - (API_BITS + CATEGORY_BITS + INDEX_BITS);

        static_assert(API_BITS + CATEGORY_BITS + INDEX_BITS + GENERATION_BITS == 32,
            "Handle bit layout incorrect");

        constexpr ResourceHandle() = default;
        explicit constexpr ResourceHandle(uint64_t i) : id(i) {}

        bool isValid() const noexcept { return id != 0; }
        bool operator==(ResourceHandle other) const noexcept { return id == other.id; }
        bool operator!=(ResourceHandle other) const noexcept { return id != other.id; }
        bool operator<(ResourceHandle other) const noexcept { return id < other.id; }

        explicit operator bool() const noexcept { return isValid(); }

        static constexpr ResourceHandle Null() noexcept { return ResourceHandle(0); }

        // 解码句柄
        uint8_t getAPI() const noexcept {
            return static_cast<uint8_t>((id >> 56) & 0x0F);  // 高4位为API标识
        }

        uint8_t getCategory() const noexcept {
            return static_cast<uint8_t>((id >> 50) & 0x3F);  // 接着6位为类别
        }

        uint32_t getIndex() const noexcept {
            return static_cast<uint32_t>((id >> 28) & 0x003FFFFF); // 接着22位为索引
        }

        uint32_t getGeneration() const noexcept {
            return static_cast<uint32_t>(id & 0x0FFFFFFF); // 低28位为世代
        }

        static ResourceHandle Create(uint8_t api, uint8_t category, uint32_t index, uint32_t generation) {
            return ResourceHandle(
                (static_cast<uint64_t>(api) << 56) |
                (static_cast<uint64_t>(category) << 50) |
                (static_cast<uint64_t>(index) << 28) |
                static_cast<uint64_t>(generation)
            );
        }
         
        // 哈希支持
        struct Hash {
            size_t operator()(ResourceHandle h) const noexcept {
                return std::hash<uint64_t>{}(h.id);
            }
        };
    };

    // ==================== 类型安全句柄 ====================
    template<ResourceCategory Category>
    struct TypedHandle {
        static constexpr ResourceCategory CATEGORY = Category;

        ResourceHandle handle{ 0 };

        constexpr TypedHandle() = default;
        explicit constexpr TypedHandle(ResourceHandle h) : handle(h) {}
        explicit constexpr TypedHandle(uint64_t id) : handle(id) {}

        bool isValid() const noexcept {
            return handle.isValid() &&
                handle.getCategory() == static_cast<uint8_t>(Category);
        }

        bool operator==(TypedHandle other) const noexcept { return handle == other.handle; }
        bool operator!=(TypedHandle other) const noexcept { return handle != other.handle; }
        bool operator<(TypedHandle other) const noexcept { return handle < other.handle; }

        explicit operator bool() const noexcept { return isValid(); }
        explicit operator ResourceHandle() const noexcept { return handle; }

        static TypedHandle Null() noexcept { return TypedHandle(ResourceHandle::Null()); }

        static TypedHandle Create(uint8_t api, uint32_t index, uint32_t generation) {
            return TypedHandle(ResourceHandle::Create(
                api,
                static_cast<uint8_t>(Category),
                index,
                generation
            ));
        }

        static TypedHandle Create(uint32_t index, uint32_t generation) {
            return Create(0, index, generation); 
        }

        // 添加获取句柄信息的方法
        uint8_t getAPI() const noexcept {
            return handle.getAPI();
        }

        uint32_t getIndex() const noexcept {
            return handle.getIndex();
        }

        uint32_t getGeneration() const noexcept {
            return handle.getGeneration();
        }

        uint8_t getCategoryRaw() const noexcept {
            return handle.getCategory();
        }

        // 调试信息
        std::string toString() const {
            if (!isValid()) return "Null";
            return "API:" + std::to_string(handle.getAPI()) +
                " Cat:" + std::to_string(static_cast<int>(Category)) +
                " Idx:" + std::to_string(handle.getIndex()) +
                " Gen:" + std::to_string(handle.getGeneration());
        }

        // 哈希支持
        struct Hash {
            size_t operator()(const TypedHandle& h) const noexcept {
                return ResourceHandle::Hash{}(h.handle);
            }
        };
    };

    // ==================== 具体句柄类型 ====================
    using BufferHandle = TypedHandle<ResourceCategory::Buffer>;
    using TextureHandle = TypedHandle<ResourceCategory::Texture>;
    using PipelineHandle = TypedHandle<ResourceCategory::Pipeline>;
    using PipelineLayoutHandle = TypedHandle<ResourceCategory::PipelineLayout>;
    using ShaderHandle = TypedHandle<ResourceCategory::Shader>;
    using RenderPassHandle = TypedHandle<ResourceCategory::RenderPass>;
    using FramebufferHandle = TypedHandle<ResourceCategory::Framebuffer>;
    using DescriptorSetHandle = TypedHandle<ResourceCategory::DescriptorSet>;
    using DescriptorSetLayoutHandle = TypedHandle<ResourceCategory::DescriptorSetLayout>;
    using DescriptorPoolHandle = TypedHandle<ResourceCategory::DescriptorPool>;
    using SamplerHandle = TypedHandle<ResourceCategory::Sampler>;
    using QueryPoolHandle = TypedHandle<ResourceCategory::QueryPool>;
    using CommandBufferHandle = TypedHandle<ResourceCategory::CommandBuffer>;
    using CommandPoolHandle = TypedHandle<ResourceCategory::CommandPool>;
    using FenceHandle = TypedHandle<ResourceCategory::Fence>;
    using SemaphoreHandle = TypedHandle<ResourceCategory::Semaphore>;
    using EventHandle = TypedHandle<ResourceCategory::Event>;
    using SwapChainHandle = TypedHandle<ResourceCategory::SwapChain>;
    using AccelerationStructureHandle = TypedHandle<ResourceCategory::AccelerationStructure>;
    using QueueHandle = TypedHandle<ResourceCategory::Queue>;

} // namespace StarryEngine::RHI