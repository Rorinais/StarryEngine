#pragma once 
#include"Geometry.hpp"

namespace StarryEngine::Assets {
	
    class Shape {
    public:
        static std::shared_ptr<Assets::Geometry> createGridGeometry(std::shared_ptr<RHI::ResourceManager> resMgr) {
            struct GridVertex {
                glm::vec3 position;
                glm::vec3 color;
            };
            std::vector<GridVertex> vertices;
            std::vector<uint32_t> indices;

            const float size = 50.0f;
            const int divisions = 50;
            const float step = size / divisions;
            const float half = size * 0.5f;
            const glm::vec3 colorXAxis(1.0f, 0.0f, 0.0f);
            const glm::vec3 colorYAxis(0.0f, 1.0f, 0.0f);
            const glm::vec3 colorZAxis(0.0f, 0.0f, 1.0f);
            const glm::vec3 colorLine(0.4f, 0.4f, 0.4f);

            // X 方向线条
            for (int i = 0; i <= divisions; ++i) {
                float z = -half + i * step;
                bool isXAxis = (std::abs(z) < 0.001f);
                glm::vec3 col = isXAxis ? colorXAxis : colorLine;
                vertices.push_back({ {-half, 0.0f, z}, col });
                vertices.push_back({ { half, 0.0f, z}, col });
            }

            // Z 方向线条
            for (int i = 0; i <= divisions; ++i) {
                float x = -half + i * step;
                bool isZAxis = (std::abs(x) < 0.001f);
                glm::vec3 col = isZAxis ? colorZAxis : colorLine;
                vertices.push_back({ { x, 0.0f, -half}, col });
                vertices.push_back({ { x, 0.0f,  half}, col });
            }

            // Y 轴线
            vertices.push_back({ {0.0f, -half, 0.0f}, colorYAxis });
            vertices.push_back({ {0.0f,  half, 0.0f}, colorYAxis });

            for (uint32_t i = 0; i < vertices.size(); i += 2) {
                indices.push_back(i);
                indices.push_back(i + 1);
            }

            std::vector<float> vertexData;
            vertexData.reserve(vertices.size() * 6);
            for (const auto& v : vertices) {
                vertexData.push_back(v.position.x);
                vertexData.push_back(v.position.y);
                vertexData.push_back(v.position.z);
                vertexData.push_back(v.color.r);
                vertexData.push_back(v.color.g);
                vertexData.push_back(v.color.b);
            }

            auto Geometry = std::make_shared<Assets::Geometry>(resMgr);
            Geometry->setVertices(vertexData);
            Geometry->setIndices(indices);
            Geometry->setPrimitiveTopology(RHI::PrimitiveTopology::LineList);
            Assets::VertexLayout gridLayout;
            gridLayout.addAttribute(0, 0, RHI::Format::RGB32_Float, 0);                
            gridLayout.addAttribute(1, 0, RHI::Format::RGB32_Float, 3 * sizeof(float));
            gridLayout.addBinding(0, 6 * sizeof(float), RHI::VertexInputRate::PerVertex); 
            Geometry->setVertexLayout(gridLayout);

            Assets::Submesh submesh;
            submesh.indexOffset = 0;
            submesh.indexCount = static_cast<uint32_t>(indices.size());
            submesh.materialIndex = 0;
            Geometry->setSubmeshes({ submesh });

            if (!Geometry->uploadToGPU()) {
                LOG_ERROR("Failed to upload grid geometry to GPU");
            }
            return Geometry;
        }

        static std::shared_ptr<Assets::Geometry> createSphereGeometry(
            std::shared_ptr<RHI::ResourceManager> resMgr,
            float radius,
            int sectors,
            int stacks) {

            auto geometry = std::make_shared<Assets::Geometry>(resMgr);

            // 顶点数据：位置 (3 float) + 法线 (3 float) + UV (2 float) = 8 float 每个顶点
            std::vector<float> vertices;
            std::vector<uint32_t> indices;

            // 预计算角度步长
            float sectorStep = 2.0f * glm::pi<float>() / sectors;
            float stackStep = glm::pi<float>() / stacks;

            for (int i = 0; i <= stacks; ++i) {
                float phi = i * stackStep;                 // 极角 [0, π]
                float sinPhi = sin(phi);
                float cosPhi = cos(phi);

                for (int j = 0; j <= sectors; ++j) {
                    float theta = j * sectorStep;          // 方位角 [0, 2π)
                    float sinTheta = sin(theta);
                    float cosTheta = cos(theta);

                    // 位置
                    float x = radius * sinPhi * cosTheta;
                    float y = radius * cosPhi;
                    float z = radius * sinPhi * sinTheta;

                    // 法线：归一化的位置（球心在原点）
                    float nx = sinPhi * cosTheta;
                    float ny = cosPhi;
                    float nz = sinPhi * sinTheta;

                    // UV：u = theta / 2π, v = phi / π
                    float u = static_cast<float>(j) / sectors;
                    float v = static_cast<float>(i) / stacks;

                    vertices.push_back(x);
                    vertices.push_back(y);
                    vertices.push_back(z);
                    vertices.push_back(nx);
                    vertices.push_back(ny);
                    vertices.push_back(nz);
                    vertices.push_back(u);
                    vertices.push_back(v);
                }
            }

            // 生成索引（三角形条带）
            for (int i = 0; i < stacks; ++i) {
                int k1 = i * (sectors + 1);
                int k2 = k1 + sectors + 1;

                for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
                    // 两个三角形组成一个矩形
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);

                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }

            // 设置几何体数据
            geometry->setVertices(vertices);
            geometry->setIndices(indices);
            geometry->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);

            // 顶点布局：位置(location=0)、法线(location=1)、UV(location=2)
            Assets::VertexLayout layout;
            // binding 0：所有 per-vertex 数据
            layout.addBinding(0, 8 * sizeof(float), RHI::VertexInputRate::PerVertex);
            layout.addAttribute(0, 0, RHI::Format::RGB32_Float, 0);           // 位置，偏移 0
            layout.addAttribute(1, 0, RHI::Format::RGB32_Float, 3 * sizeof(float)); // 法线，偏移 12
            layout.addAttribute(2, 0, RHI::Format::RG32_Float, 6 * sizeof(float));   // UV，偏移 24
            geometry->setVertexLayout(layout);

            // 子网格
            geometry->setSubmeshes({ { 0, static_cast<uint32_t>(indices.size()), 0 } });
            geometry->uploadToGPU();

            return geometry;
        }

    };


}