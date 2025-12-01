#pragma once 
#include "../../buffers/IndexBuffer.hpp"
#include "../../buffers/VertexArrayBuffer.hpp"
#include "../boundingBox/BoundingBox.hpp"
#include "../geometry/Geometry.hpp"
//#include ".././../materials/DefaultMaterial.hpp"
#include "../../../../renderer/backends/vulkan/vulkanCore/VulkanCore.hpp"
#include "../../../../renderer/backends/vulkan/renderContext/CommandPool.hpp"

namespace StarryEngine {
    class Mesh {
    public:
        Mesh() = default;

        Mesh(Geometry::Ptr geo, const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool) : geometry(std::move(geo)) {
            vertexBuffer = VertexArrayBuffer::create(logicalDevice, commandPool);

            std::vector<glm::vec3> poss;
            for (auto& pos : geometry->getVertices()) {
                poss.push_back(pos.position);
            }

            vertexBuffer->beginBinding(0);
            vertexBuffer->addAttribute(0, VK_FORMAT_R32G32B32_SFLOAT, poss);
            vertexBuffer->finishBinding();

            indexBuffer = IndexBuffer::create(logicalDevice, commandPool);
            indexBuffer->loadData(geometry->getIndices());
        }

        //void setTransform(const glm::mat4& transform);
        //void setMaterial(const std::string& matID);
        //void uploadToGPU();
        //void bind() const;
        Geometry::Ptr getGeometry() const { return geometry; }
        std::string getMaterial() const { return materialID; }
        VertexArrayBuffer::Ptr getVertexBuffer() const { return vertexBuffer; }
        IndexBuffer::Ptr getIndexBuffer() const { return indexBuffer; }
    private:
        std::string name = "DefaultMesh";
        std::string materialID = "0";
        VertexArrayBuffer::Ptr vertexBuffer;
        IndexBuffer::Ptr indexBuffer;

        Geometry::Ptr geometry;
    };
}
