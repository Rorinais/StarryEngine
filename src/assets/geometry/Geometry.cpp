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
        vbDesc.debugName = "GeometryVB";
        m_vertexBuffer = m_resMgr->createBuffer(vbDesc, "GeometryVB");
        if (!m_vertexBuffer.isValid()) {
            LOG_ERROR("Failed to create vertex buffer");
            return false;
        }

        auto* vb = m_resMgr->getBuffer(m_vertexBuffer);
        if (!vb->update(m_vertices.data(), vbDesc.size, 0)) {
            LOG_ERROR("Failed to upload vertex data");
            m_resMgr->destroy(m_vertexBuffer);
            return false;
        }

        RHI::BufferDesc ibDesc;
        ibDesc.size = m_indices.size() * sizeof(uint32_t);
        ibDesc.type = RHI::BufferType::Index;
        ibDesc.memoryType = RHI::MemoryType::GPU_Only;
        ibDesc.allowUpdate = false;
        ibDesc.debugName = "GeometryIB";
        m_indexBuffer = m_resMgr->createBuffer(ibDesc, "GeometryIB");
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

        LOG_INFO("Geometry uploaded to GPU");
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

    void Geometry::setVertexBuffer(uint32_t binding, const std::vector<float>& vertices,
        const Assets::VertexLayout& layout, const std::string& debugName) {
        mVertexLayout = layout;

        uint32_t stride = layout.getBindingStride(binding);
        if (stride == 0) {
            std::cerr << "[Geometry] Binding " << binding << " not found in vertex layout for "
                << debugName << std::endl;
            return;
        }

        // 计算顶点数
        uint32_t vertexCount = static_cast<uint32_t>(vertices.size() * sizeof(float) / stride);
        if (mVertexCount == 0) {
            mVertexCount = vertexCount;  // 第一个缓冲区，记录顶点数
        }
        else if (mVertexCount != vertexCount) {
            // 可选：如果后续缓冲区的顶点数不一致，可以抛出警告或错误
            std::cerr << "[Geometry] Warning: Vertex count mismatch for binding " << binding
                << " (" << vertexCount << " vs " << mVertexCount << ")" << std::endl;
            // 为了安全，可以选择继续使用原有 mVertexCount，或者更新为最小值
            // 这里我们选择不更新，但输出警告
        }

        RHI::BufferDesc bufferDesc;
        bufferDesc.size = vertices.size() * sizeof(float);
        bufferDesc.stride = stride;
        bufferDesc.type = RHI::BufferType::Vertex;
        bufferDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
        bufferDesc.allowUpdate = true;
        bufferDesc.debugName = debugName;

        RHI::BufferHandle handle = m_resMgr ->createBuffer(bufferDesc);
        if (!handle.isValid()) {
            std::cerr << "[Geometry] Failed to create vertex buffer for binding " << binding
                << ": " << debugName << std::endl;
            return;
        }

        auto* buffer = m_resMgr->getBuffer(handle);
        buffer->update(vertices.data(), vertices.size() * sizeof(float));

        mVertexBufferHandles[binding] = handle;
    }

    void Geometry::setVertexBuffer(const std::vector<float>& vertices,
        const Assets::VertexLayout& layout, const std::string& debugName) {
        setVertexBuffer(0, vertices, layout, debugName);  // 默认 binding 0
    }

    void Geometry::setIndexBuffer(const std::vector<uint32_t>& indices, const std::string& debugName) {
        RHI::BufferDesc bufferDesc;
        bufferDesc.size = indices.size() * sizeof(uint32_t);
        bufferDesc.stride = sizeof(uint32_t);
        bufferDesc.type = RHI::BufferType::Index;
        bufferDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
        bufferDesc.allowUpdate = true;
        bufferDesc.debugName = debugName;

        mIndexBufferHandle = m_resMgr->createBuffer(bufferDesc);
        if (!mIndexBufferHandle.isValid()) {
            std::cerr << "[Geometry] Failed to create index buffer: " << debugName << std::endl;
            return;
        }
        auto* buffer = m_resMgr->getBuffer(mIndexBufferHandle);
        buffer->update(indices.data(), indices.size() * sizeof(uint32_t));
        mIndexCount = static_cast<uint32_t>(indices.size());
    }

    RHI::BufferHandle Geometry::getVertexBufferHandle(uint32_t binding) const {
        auto it = mVertexBufferHandles.find(binding);
        if (it != mVertexBufferHandles.end()) {
            return it->second;
        }
        return RHI::BufferHandle::Null();
    }

} // namespace StarryEngine::Assets