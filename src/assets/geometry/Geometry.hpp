#pragma once
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <assimp/scene.h> 
#include "VertexLayout.hpp"
#include "../AssetType.hpp"

namespace StarryEngine::Assets {
    struct Submesh {
        uint32_t indexOffset;      // 在全局索引缓冲区中的起始索引
        uint32_t indexCount;       // 该子网格的索引数
        uint32_t materialIndex;    // 指向材质列表的索引
    };

    class Geometry {
    public:
        Geometry(std::shared_ptr<RHI::ResourceManager> resMgr);
        ~Geometry();

        bool uploadToGPU();
        void releaseGPU();

        void setVertices(const std::vector<float>& vertices) { m_vertices = vertices; }
        void setIndices(const std::vector<uint32_t>& indices) { m_indices = indices; }
        void setSubmeshes(const std::vector<Submesh>& submeshes) { m_submeshes = submeshes; }
        void setVertexLayout(const VertexLayout& layout) { m_vertexLayout = layout; }
        void setPrimitiveTopology(const RHI::PrimitiveTopology& topology) { m_topology = topology; }
        void setLineWidth(float lineWidth) { lineWidth = m_lineWidth; }

        RHI::VertexInputState getVertexInputStateWithInstancing(const InstancingLayout* layout) const;

        std::vector<uint32_t> getBindings() const { return m_vertexLayout.getBindings(); }
        RHI::VertexInputState getVertexInputState() const { return m_vertexLayout.build(); }
        RHI::BufferHandle getVertexBuffer() const { return m_vertexBuffer; }
        RHI::BufferHandle getIndexBuffer() const { return m_indexBuffer; }
        RHI::PrimitiveTopology getPrimitiveTopology() const { return m_topology; }
        const std::vector<Submesh>& getSubmeshes() const { return m_submeshes; }
        const std::vector<float>& getVertices() const { return m_vertices; }
        const std::vector<uint32_t>& getIndices() const { return m_indices; }
        const VertexLayout& getVertexLayout() const { return m_vertexLayout; }
        float getLineWidth() { return m_lineWidth; }

    private:
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::vector<float> m_vertices;
        std::vector<uint32_t> m_indices;
        std::vector<Submesh> m_submeshes;
        RHI::BufferHandle m_vertexBuffer;
        RHI::BufferHandle m_indexBuffer;
        VertexLayout m_vertexLayout;

        RHI::PrimitiveTopology m_topology = RHI::PrimitiveTopology::TriangleList;
        float m_lineWidth = 1.0f;
    };

} // namespace StarryEngine::Assets