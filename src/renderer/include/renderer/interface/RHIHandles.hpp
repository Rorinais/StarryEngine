#pragma once
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <functional>
#include <string>
#include <memory>
#include <atomic>

#include <interface/RHIEnums.hpp>

namespace StarryEngine::RHI {
    struct ResourceHandle {
        uint64_t id = 0;

        static constexpr uint8_t API_BITS = 4;     
        static constexpr uint8_t CATEGORY_BITS = 6; 
        static constexpr uint8_t INDEX_BITS = 22;   
        static constexpr uint8_t GENERATION_BITS = 32 - (API_BITS + CATEGORY_BITS + INDEX_BITS);

        static_assert(API_BITS + CATEGORY_BITS + INDEX_BITS + GENERATION_BITS == 32,"Handle bit layout incorrect");

        constexpr ResourceHandle() = default;
        explicit constexpr ResourceHandle(uint64_t i) : id(i) {}

        auto isValid() const noexcept { return id != 0; }
        auto operator==(ResourceHandle other) const noexcept { return id == other.id; }
        auto operator!=(ResourceHandle other) const noexcept { return id != other.id; }
        auto operator<(ResourceHandle other) const noexcept { return id < other.id; }

        explicit operator bool() const noexcept { return isValid(); }

        static constexpr ResourceHandle Null() noexcept { return ResourceHandle(0); }

        auto getAPI() const noexcept {
            return static_cast<uint8_t>((id >> 56) & 0x0F);  // 高4位为API标识
        }

        auto getCategory() const noexcept {
            return static_cast<uint8_t>((id >> 50) & 0x3F);  // 接着6位为类别
        }

        auto getIndex() const noexcept {
            return static_cast<uint32_t>((id >> 28) & 0x003FFFFF); // 接着22位为索引
        }

        auto getGeneration() const noexcept {
            return static_cast<uint32_t>(id & 0x0FFFFFFF); // 低28位为世代
        }

        static auto Create(uint8_t api, uint8_t category, uint32_t index, uint32_t generation) {
            return ResourceHandle(
                (static_cast<uint64_t>(api) << 56) |
                (static_cast<uint64_t>(category) << 50) |
                (static_cast<uint64_t>(index) << 28) |
                static_cast<uint64_t>(generation)
            );
        }
         
        struct Hash {
            auto operator()(ResourceHandle h) const noexcept {
                return std::hash<uint64_t>{}(h.id);
            }
        };
    };

    template<ResourceCategory Category>
    struct TypedHandle {
        static constexpr ResourceCategory CATEGORY = Category;

        ResourceHandle handle{ 0 };

        constexpr TypedHandle() = default;
        explicit constexpr TypedHandle(ResourceHandle h) : handle(h) {}
        explicit constexpr TypedHandle(uint64_t id) : handle(id) {}

        auto isValid() const noexcept {
            return handle.isValid() &&
                handle.getCategory() == static_cast<uint8_t>(Category);
        }

        auto operator==(TypedHandle other) const noexcept { return handle == other.handle; }
        auto operator!=(TypedHandle other) const noexcept { return handle != other.handle; }
        auto operator<(TypedHandle other) const noexcept { return handle < other.handle; }

        explicit operator bool() const noexcept { return isValid(); }
        explicit operator ResourceHandle() const noexcept { return handle; }

        static auto Null() noexcept { return TypedHandle(ResourceHandle::Null()); }

        void reset() { handle = ResourceHandle::Null(); }

        static auto Create(uint8_t api, uint32_t index, uint32_t generation) {
            return TypedHandle(ResourceHandle::Create(
                api,
                static_cast<uint8_t>(Category),
                index,
                generation
            ));
        }

        static auto Create(uint32_t index, uint32_t generation) {
            return Create(0, index, generation); 
        }

        auto getAPI() const noexcept {
            return handle.getAPI();
        }

        auto getIndex() const noexcept {
            return handle.getIndex();
        }

        auto getGeneration() const noexcept {
            return handle.getGeneration();
        }

        auto getCategoryRaw() const noexcept {
            return handle.getCategory();
        }

        auto toString() ->std::string const {
            if (!isValid()) return "Null";
            return "API:" + std::to_string(handle.getAPI()) +
                " Cat:" + std::to_string(static_cast<int>(Category)) +
                " Idx:" + std::to_string(handle.getIndex()) +
                " Gen:" + std::to_string(handle.getGeneration());
        }

        struct Hash {
            auto operator()(const TypedHandle& h) const noexcept {
                return ResourceHandle::Hash{}(h.handle);
            }
        };
    };

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

} // namespace StarryEngine::RHI