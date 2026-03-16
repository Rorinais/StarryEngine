#pragma once
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <assimp/scene.h> 
#include "VertexLayout.hpp"
#include "../AssetType.hpp"

//声明：追求低耦合，应该与rhi的枚举和结构体分开，应用层应该重新设计，但是句柄还是可以使用
//场景数据设计
//先思考，有哪些数据需要设计
//scene下有可渲染物体和不可渲染物体，可渲染物体有灯光和几何物体，天空盒，栅格
//不可渲染物体有空物体，摄像机
//那就可以这样考虑，场景结构体就包含各一个可渲染物体和不可渲染物体的数组，还有一个当前的摄像机，天空盒单独分开，因为是绑定在摄像机下，
// 更新的时候将当前摄像机的矩阵设置给MVP矩阵，实现摄像机的切换，update就是实时上传命令缓冲区
//更细致的考虑：
//可渲染物体的结构体应该有几何体和材质数组，变换矩阵，因为一个可渲染物体对应一个几何体，但是一个几何体可能有多个子网格，而每个子网格会对应一个材质
//那么一个材质可能对应两个或者以上的shader，有可能有多张贴图，也有可能有多个uniformBuffer，

namespace StarryEngine::Assets {
    struct Submesh {
        uint32_t indexOffset;      // 在全局索引缓冲区中的起始索引（以索引数计）
        uint32_t indexCount;       // 该子网格的索引数
        uint32_t materialIndex;    // 指向材质列表的索引（或句柄）
        // 可选：AABB 包围盒等
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
        void setVertexLayout(const VertexLayout& layout) { mVertexLayout = layout; }

        std::vector<uint32_t> getBindings() const { return mVertexLayout.getBindings(); }
        RHI::VertexInputState getVertexInputState() const { return mVertexLayout.build(); }
        RHI::BufferHandle getVertexBuffer() const { return m_vertexBuffer; }
        RHI::BufferHandle getIndexBuffer() const { return m_indexBuffer; }
        const std::vector<Submesh>& getSubmeshes() const { return m_submeshes; }
        const std::vector<float>& getVertices() const { return m_vertices; }
        const std::vector<uint32_t>& getIndices() const { return m_indices; }
        const VertexLayout& getVertexLayout() const { return mVertexLayout; }

    private:
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::vector<float> m_vertices;
        std::vector<uint32_t> m_indices;
        std::vector<Submesh> m_submeshes;
        RHI::BufferHandle m_vertexBuffer;
        RHI::BufferHandle m_indexBuffer;
        VertexLayout mVertexLayout;
    };

} // namespace StarryEngine::Assets