#include <logging/Logger.hpp>
#include <assets/geometry/Geometry.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


namespace StarryEngine::Assets {

    Geometry::Geometry(std::shared_ptr<RHI::ResourceManager> resMgr)
        : m_resMgr(resMgr) {
    }

    Geometry::~Geometry() {
        releaseGPU();
    }

    bool Geometry::uploadToGPU() {
        if (m_vertices.empty() || m_indices.empty()) {
            LOG_ERROR("Cannot upload empty geometry to GPU");
            return false;
        }

        RHI::BufferDesc vbDesc;
        vbDesc.size = m_vertices.size() * sizeof(float);
        vbDesc.type = RHI::BufferType::Vertex;
        vbDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
        vbDesc.allowUpdate = false;
        vbDesc.debugName = "VBO";
        m_vertexBuffer = m_resMgr->createBuffer(vbDesc);
        if (!m_vertexBuffer.isValid()) {
            LOG_ERROR("Failed to create vertex buffer (size={})", vbDesc.size);
            return false;
        }

        auto* vb = m_resMgr->getBuffer(m_vertexBuffer);
        if (!vb->update(m_vertices.data(), vbDesc.size, 0)) {
            LOG_ERROR("Failed to upload vertex data (size={})", vbDesc.size);
            m_resMgr->destroy(m_vertexBuffer);
            m_vertexBuffer = RHI::BufferHandle::Null();
            return false;
        }

        RHI::BufferDesc ibDesc;
        ibDesc.size = m_indices.size() * sizeof(uint32_t);
        ibDesc.type = RHI::BufferType::Index;
        ibDesc.memoryType = RHI::MemoryType::GPU_Only;
        ibDesc.allowUpdate = false;
        ibDesc.debugName = "IBO";
        m_indexBuffer = m_resMgr->createBuffer(ibDesc);
        if (!m_indexBuffer.isValid()) {
            LOG_ERROR("Failed to create index buffer");
            m_resMgr->destroy(m_vertexBuffer);
            return false;
        }

        auto* ib = m_resMgr->getBuffer(m_indexBuffer);
        if (!ib->update(m_indices.data(), ibDesc.size, 0)) {
            LOG_ERROR("Failed to upload index data");
            m_resMgr->destroy(m_indexBuffer);
            m_resMgr->destroy(m_vertexBuffer);
            return false;
        }

        return true;
    }

    void Geometry::releaseGPU() {
        if (m_vertexBuffer.isValid()) {
            m_resMgr->destroy(m_vertexBuffer);
            m_vertexBuffer = RHI::BufferHandle::Null();
        }
        if (m_indexBuffer.isValid()) {
            m_resMgr->destroy(m_indexBuffer);
            m_indexBuffer = RHI::BufferHandle::Null();
        }
    }

    RHI::VertexInputState Geometry::getVertexInputStateWithInstancing(const InstancingLayout* layout) const {
        if (!layout || layout->attributes.empty()) {
            return m_vertexLayout.build();
        }
        VertexLayout merged = m_vertexLayout;
        // 将 InstancingLayout 转换为 VertexLayout 并合并
        VertexLayout instLayout;
        instLayout.addBinding(layout->binding, layout->stride, RHI::VertexInputRate::PerInstance);
        for (const auto& attr : layout->attributes) {
            instLayout.addAttribute(attr.location, layout->binding, attr.format, attr.offset);
        }
        merged.merge(instLayout);
        return merged.build();
    }

} // namespace StarryEngine::Assets