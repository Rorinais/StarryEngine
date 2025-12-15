#pragma once
#include <memory>
#include <vector>
#include <glm/glm.hpp>
namespace StarryEngine {

    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;
        glm::vec3 tangent;
        glm::vec3 bitangent;
    };

    class Geometry {
    public:
        using Ptr = std::shared_ptr<Geometry>;
        static Ptr create() { return std::make_shared<Geometry>(); }

        static Ptr create(std::vector<Vertex> verts, std::vector<uint32_t> inds) {
            return std::make_shared<Geometry>(verts, inds);
        }

        Geometry() = default;
        Geometry(std::vector<Vertex> verts, std::vector<uint32_t> inds)
            : vertices(std::move(verts)), indices(std::move(inds)) {
        }

        void applyTransform(const glm::mat4& transform);
        void calculateNormals();
        void generateTangents();

        const std::vector<Vertex>& getVertices() const { return vertices; }
        const std::vector<uint32_t>& getIndices() const { return indices; }

        // 获取顶点数量
        uint32_t getVertexCount() const { return static_cast<uint32_t>(vertices.size()); }

        // 获取索引数量
        uint32_t getIndexCount() const { return static_cast<uint32_t>(indices.size()); }

    private:
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };
}
