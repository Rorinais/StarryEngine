#pragma once

#include <cstdint>
#include <functional>

namespace StarryEngine::RenderGraph {

    class ResourceId {
    public:
        ResourceId() : m_id(0), m_generation(0) {}
        bool isValid() const { return m_id != 0; }
        uint32_t id() const { return m_id; }
        uint32_t generation() const { return m_generation; }
        bool operator==(const ResourceId& other) const {
            return m_id == other.m_id && m_generation == other.m_generation;
        }
        bool operator!=(const ResourceId& other) const { return !(*this == other); }

        bool operator<(const ResourceId& other) const {
            // 按 id 比较即可，generation 作为次要比较（可选）
            if (m_id != other.m_id) return m_id < other.m_id;
            return m_generation < other.m_generation;
        }

    protected:
        ResourceId(uint32_t id, uint32_t gen) : m_id(id), m_generation(gen) {}

    private:
        uint32_t m_id;
        uint32_t m_generation;
    };

    class TextureId : public ResourceId {
    public:
        TextureId() = default;
        static TextureId Null() { return TextureId(); }
        static TextureId Create(uint32_t id, uint32_t gen) { return TextureId(id, gen); }

    private:
        TextureId(uint32_t id, uint32_t gen) : ResourceId(id, gen) {}
    };

    class BufferId : public ResourceId {
    public:
        BufferId() = default;
        static BufferId Null() { return BufferId(); }
        static BufferId Create(uint32_t id, uint32_t gen) { return BufferId(id, gen); }

    private:
        BufferId(uint32_t id, uint32_t gen) : ResourceId(id, gen) {}
    };

} // namespace StarryEngine::RenderGraph

namespace std {
    template<> struct hash<StarryEngine::RenderGraph::TextureId> {
        size_t operator()(const StarryEngine::RenderGraph::TextureId& id) const noexcept {
            return hash<uint64_t>()((uint64_t(id.id()) << 32) | id.generation());
        }
    };
    template<> struct hash<StarryEngine::RenderGraph::BufferId> {
        size_t operator()(const StarryEngine::RenderGraph::BufferId& id) const noexcept {
            return hash<uint64_t>()((uint64_t(id.id()) << 32) | id.generation());
        }
    };
}