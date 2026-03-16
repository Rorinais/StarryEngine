#include "../../logging/Logger.hpp"
#include "Geometry.hpp"
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

} // namespace StarryEngine::Assets