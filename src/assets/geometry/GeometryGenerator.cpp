#include "GeometryGenerator.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>

namespace StarryEngine::Assets {

    // ==================== Sphere ====================
    std::shared_ptr<Geometry> GeometryGenerator::createSphere(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        float radius,
        uint32_t sectorCount,
        uint32_t stackCount)
    {
        auto geometry = std::make_shared<Geometry>(resMgr);
        std::vector<float> vertices;
        std::vector<uint32_t> indices;

        float sectorStep = 2.0f * glm::pi<float>() / sectorCount;
        float stackStep = glm::pi<float>() / stackCount;

        for (uint32_t i = 0; i <= stackCount; ++i) {
            float phi = i * stackStep;          // 0 ~ PI
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);

            for (uint32_t j = 0; j <= sectorCount; ++j) {
                float theta = j * sectorStep;   // 0 ~ 2PI
                float sinTheta = sin(theta);
                float cosTheta = cos(theta);

                // 位置
                float x = radius * sinPhi * cosTheta;
                float y = radius * cosPhi;
                float z = radius * sinPhi * sinTheta;

                // 法线
                float nx = sinPhi * cosTheta;
                float ny = cosPhi;
                float nz = sinPhi * sinTheta;

                // 纹理坐标
                float u = (float)j / sectorCount;
                float v = (float)i / stackCount;

                // 切线
                float tx = -sinTheta;  
                float ty = 0.0f;
                float tz = cosTheta;
                glm::vec3 tangent = glm::normalize(glm::vec3(tx, ty, tz));

                // 副切
                float bx = cosPhi * cosTheta;
                float by = -sinPhi;
                float bz = cosPhi * sinTheta;
                glm::vec3 bitangent = glm::normalize(glm::vec3(bx, by, bz));

                glm::vec3 normal(nx, ny, nz);
                float handedness = (glm::dot(glm::cross(normal, tangent), bitangent) < 0.0f) ? -1.0f : 1.0f;

                // 存储数据：位置(3) + 法线(3) + UV(2) + 切线(4)
                vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
                vertices.push_back(nx); vertices.push_back(ny); vertices.push_back(nz);
                vertices.push_back(u); vertices.push_back(v);
                vertices.push_back(tangent.x); vertices.push_back(tangent.y); vertices.push_back(tangent.z);
                vertices.push_back(handedness);
            }
        }

        for (uint32_t i = 0; i < stackCount; ++i) {
            uint32_t k1 = i * (sectorCount + 1);
            uint32_t k2 = k1 + sectorCount + 1;
            for (uint32_t j = 0; j < sectorCount; ++j, ++k1, ++k2) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }

        geometry->setVertices(vertices);
        geometry->setIndices(indices);
        geometry->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);

        VertexLayout layout;
        layout.addBinding(0, 0, RHI::VertexInputRate::PerVertex); 
        layout.addAttribute(VertexSemantic::Position, 0, RHI::Format::RGB32_Float);  // offset 0
        layout.addAttribute(VertexSemantic::Normal, 0, RHI::Format::RGB32_Float);  // offset 12
        layout.addAttribute(VertexSemantic::TexCoord0, 0, RHI::Format::RG32_Float); // offset 24
        layout.addAttribute(VertexSemantic::Tangent, 0, RHI::Format::RGBA32_Float); // offset 32
        geometry->setVertexLayout(layout);
        geometry->setSubmeshes({ {0, (uint32_t)indices.size(), 0} });
        geometry->uploadToGPU();
        return geometry;
    }

    // ==================== Grid ====================
    std::shared_ptr<Geometry> GeometryGenerator::createGrid(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        float size,
        uint32_t divisions)
    {
        struct GridVertex {
            glm::vec3 position;
            glm::vec3 color;
        };
        std::vector<GridVertex> vertices;
        std::vector<uint32_t> indices;

        float half = size * 0.5f;
        float step = size / divisions;
        glm::vec3 colorXAxis(1.0f, 0.0f, 0.0f);
        glm::vec3 colorZAxis(0.0f, 0.0f, 1.0f);
        glm::vec3 colorLine(0.4f, 0.4f, 0.4f);

        // X 方向线条
        for (uint32_t i = 0; i <= divisions; ++i) {
            float z = -half + i * step;
            bool isXAxis = (std::abs(z) < 0.001f);
            glm::vec3 col = isXAxis ? colorXAxis : colorLine;
            vertices.push_back({ {-half, 0.0f, z}, col });
            vertices.push_back({ { half, 0.0f, z}, col });
        }

        // Z 方向线条
        for (uint32_t i = 0; i <= divisions; ++i) {
            float x = -half + i * step;
            bool isZAxis = (std::abs(x) < 0.001f);
            glm::vec3 col = isZAxis ? colorZAxis : colorLine;
            vertices.push_back({ { x, 0.0f, -half}, col });
            vertices.push_back({ { x, 0.0f,  half}, col });
        }

        // Y 轴线
        vertices.push_back({ {0.0f, -half, 0.0f}, glm::vec3(0.0f, 1.0f, 0.0f) });
        vertices.push_back({ {0.0f,  half, 0.0f}, glm::vec3(0.0f, 1.0f, 0.0f) });

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

        auto geometry = std::make_shared<Geometry>(resMgr);
        geometry->setVertices(vertexData);
        geometry->setIndices(indices);
        geometry->setPrimitiveTopology(RHI::PrimitiveTopology::LineList);
        VertexLayout layout;
        layout.addBinding(0, 6 * sizeof(float), RHI::VertexInputRate::PerVertex);
        layout.addAttribute(0, 0, RHI::Format::RGB32_Float, 0);
        layout.addAttribute(1, 0, RHI::Format::RGB32_Float, 3 * sizeof(float));
        geometry->setVertexLayout(layout);
        geometry->setSubmeshes({ {0, (uint32_t)indices.size(), 0} });
        geometry->uploadToGPU();
        return geometry;
    }

    // ==================== Cube ====================
    std::shared_ptr<Geometry> GeometryGenerator::createCube(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        float width, float height, float depth)
    {
        float halfL = width * 0.5f;
        float halfW = depth * 0.5f;   // Z 轴方向
        float halfH = height * 0.5f;  // Y 轴方向

        std::array<glm::vec3, 8> positions = {
            glm::vec3(-halfL, -halfH, -halfW), // 0  (X=宽, Y=高, Z=深)
            glm::vec3(halfL,  -halfH, -halfW), // 1
            glm::vec3(halfL,   halfH, -halfW), // 2
            glm::vec3(-halfL,  halfH, -halfW), // 3
            glm::vec3(-halfL, -halfH,  halfW), // 4
            glm::vec3(halfL,  -halfH,  halfW), // 5
            glm::vec3(halfL,   halfH,  halfW), // 6
            glm::vec3(-halfL,  halfH,  halfW)  // 7
        };

        // 面定义：{ 面索引, 顶点顺序（左下、右下、右上、左上），法线 }
        struct Face {
            std::array<uint32_t, 4> idx;
            glm::vec3 normal;
        };
        std::array<Face, 6> faces = { {
            { {0, 1, 2, 3}, glm::vec3(0,  0, -1) }, 
            { {4, 5, 6, 7}, glm::vec3(0,  0,  1) },
            { {0, 4, 7, 3}, glm::vec3(-1,  0,  0) },
            { {1, 5, 6, 2}, glm::vec3(1,  0,  0) },
            { {0, 1, 5, 4}, glm::vec3(0, -1,  0) },
            { {3, 2, 6, 7}, glm::vec3(0,  1,  0) }
        } };

        // 每个面的 UV 坐标
        std::array<glm::vec2, 4> uvs = {
            glm::vec2(0, 0), glm::vec2(1, 0),
            glm::vec2(1, 1), glm::vec2(0, 1)
        };

        std::vector<float> vertexData; 
        std::vector<uint32_t> indices;

        for (int f = 0; f < 6; ++f) {
            const auto& face = faces[f];
            glm::vec3 normal = face.normal;

            // 计算切线和副切线
            // 取顶点 0 到 1 的边作为切线方向
            glm::vec3 edge1 = positions[face.idx[1]] - positions[face.idx[0]];
            glm::vec3 edge2 = positions[face.idx[3]] - positions[face.idx[0]];
            glm::vec2 deltaUV1 = uvs[1] - uvs[0];
            glm::vec2 deltaUV2 = uvs[3] - uvs[0];

            float fInv = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
            glm::vec3 tangent, bitangent;
            tangent.x = fInv * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            tangent.y = fInv * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            tangent.z = fInv * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
            tangent = glm::normalize(tangent);

            bitangent.x = fInv * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
            bitangent.y = fInv * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
            bitangent.z = fInv * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
            bitangent = glm::normalize(bitangent);

            float handedness = (glm::dot(glm::cross(normal, tangent), bitangent) < 0.0f) ? -1.0f : 1.0f;

            for (int i = 0; i < 4; ++i) {
                const glm::vec3& pos = positions[face.idx[i]];
                const glm::vec2& uv = uvs[i];

                vertexData.push_back(pos.x);
                vertexData.push_back(pos.y);
                vertexData.push_back(pos.z);
                vertexData.push_back(normal.x);
                vertexData.push_back(normal.y);
                vertexData.push_back(normal.z);
                vertexData.push_back(uv.x);
                vertexData.push_back(uv.y);
                vertexData.push_back(tangent.x);
                vertexData.push_back(tangent.y);
                vertexData.push_back(tangent.z);
                vertexData.push_back(handedness);
            }

            uint32_t baseIndex = f * 4;
            indices.push_back(baseIndex + 0);
            indices.push_back(baseIndex + 1);
            indices.push_back(baseIndex + 2);
            indices.push_back(baseIndex + 0);
            indices.push_back(baseIndex + 2);
            indices.push_back(baseIndex + 3);
        }

        auto geometry = std::make_shared<Geometry>(resMgr);
        geometry->setVertices(vertexData);
        geometry->setIndices(indices);
        geometry->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);

        VertexLayout layout;
        layout.addBinding(0, 0, RHI::VertexInputRate::PerVertex);
        layout.addAttribute(VertexSemantic::Position, 0, RHI::Format::RGB32_Float);  // offset 0
        layout.addAttribute(VertexSemantic::Normal, 0, RHI::Format::RGB32_Float);  // offset 12
        layout.addAttribute(VertexSemantic::TexCoord0, 0, RHI::Format::RG32_Float);   // offset 24
        layout.addAttribute(VertexSemantic::Tangent, 0, RHI::Format::RGBA32_Float); // offset 32
        geometry->setVertexLayout(layout);
        geometry->setSubmeshes({ {0, (uint32_t)indices.size(), 0} });
        geometry->uploadToGPU();
        return geometry;
    }

    // ==================== Quad ====================
    std::shared_ptr<Geometry> GeometryGenerator::createQuad(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        float width,
        float height)
    {
        float hw = width * 0.5f;
        float hh = height * 0.5f;

        glm::vec3 normal = glm::vec3(0, 0, 1);
        glm::vec3 tangent = glm::vec3(1, 0, 0);
        glm::vec3 bitangent = glm::vec3(0, 1, 0);
        float handedness = 1.0f;

        std::vector<float> vertices = {
            // position       normal       uv     tangent + handedness
            -hw, -hh, 0.0f,  0,0,1,  0,0,  1,0,0, handedness,
             hw, -hh, 0.0f,  0,0,1,  1,0,  1,0,0, handedness,
             hw,  hh, 0.0f,  0,0,1,  1,1,  1,0,0, handedness,
            -hw,  hh, 0.0f,  0,0,1,  0,1,  1,0,0, handedness
        };
        std::vector<uint32_t> indices = { 0,1,2, 0,2,3 };

        auto geometry = std::make_shared<Geometry>(resMgr);
        geometry->setVertices(vertices);
        geometry->setIndices(indices);
        geometry->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);

        VertexLayout layout;
        layout.addBinding(0, 0, RHI::VertexInputRate::PerVertex);
        layout.addAttribute(VertexSemantic::Position, 0, RHI::Format::RGB32_Float);  // offset 0
        layout.addAttribute(VertexSemantic::Normal, 0, RHI::Format::RGB32_Float);  // offset 12
        layout.addAttribute(VertexSemantic::TexCoord0, 0, RHI::Format::RG32_Float);   // offset 24
        layout.addAttribute(VertexSemantic::Tangent, 0, RHI::Format::RGBA32_Float); // offset 32
        geometry->setVertexLayout(layout);
        geometry->setSubmeshes({ {0, (uint32_t)indices.size(), 0} });
        geometry->uploadToGPU();
        return geometry;
    }

    // ==================== Cylinder / Frustum ====================
    std::shared_ptr<Geometry> GeometryGenerator::createCylinder(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        float bottomRadius,
        float topRadius,
        float height,
        uint32_t radialSegments,
        uint32_t heightSegments,
        bool topCap,
        bool bottomCap)
    {
        auto geometry = std::make_shared<Geometry>(resMgr);
        std::vector<float> vertices;  // 位置(xyz) + 法线(xyz) + UV(uv)
        std::vector<uint32_t> indices;

        float halfH = height * 0.5f;
        float angleStep = 2.0f * glm::pi<float>() / radialSegments;
        float heightStep = height / heightSegments;

        // 添加顶点
        auto addVertex = [&](float x, float y, float z, float nx, float ny, float nz, float u, float v) {
            vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
            vertices.push_back(nx); vertices.push_back(ny); vertices.push_back(nz);
            vertices.push_back(u); vertices.push_back(v);
            return (vertices.size() / 8) - 1;
            };

        std::vector<std::vector<uint32_t>> ringIndices(heightSegments + 1);
        for (uint32_t i = 0; i <= heightSegments; ++i) {
            float t = (float)i / heightSegments;
            float y = -halfH + i * heightStep;
            float r = bottomRadius * (1.0f - t) + topRadius * t;
            ringIndices[i].resize(radialSegments);
            for (uint32_t j = 0; j < radialSegments; ++j) {
                float angle = j * angleStep;
                float x = r * cos(angle);
                float z = r * sin(angle);
                // 法线：径向方向
                float nx = cos(angle);
                float nz = sin(angle);
                float ny = 0.0f;
                float u = (float)j / radialSegments;
                float v = t;
                uint32_t idx = addVertex(x, y, z, nx, ny, nz, u, v);
                ringIndices[i][j] = idx;
            }
        }

        // 侧面三角形
        for (uint32_t i = 0; i < heightSegments; ++i) {
            for (uint32_t j = 0; j < radialSegments; ++j) {
                uint32_t nextJ = (j + 1) % radialSegments;
                uint32_t a = ringIndices[i][j];
                uint32_t b = ringIndices[i + 1][j];
                uint32_t c = ringIndices[i + 1][nextJ];
                uint32_t d = ringIndices[i][nextJ];
                indices.push_back(a); indices.push_back(b); indices.push_back(c);
                indices.push_back(a); indices.push_back(c); indices.push_back(d);
            }
        }

        // 底面
        if (bottomCap && bottomRadius > 0.0f) {
            uint32_t centerIdx = addVertex(0.0f, -halfH, 0.0f, 0.0f, -1.0f, 0.0f, 0.5f, 0.5f);
            std::vector<uint32_t> edgeIndices(radialSegments);
            for (uint32_t j = 0; j < radialSegments; ++j) {
                float angle = j * angleStep;
                float x = bottomRadius * cos(angle);
                float z = bottomRadius * sin(angle);
                float u = (cos(angle) + 1.0f) * 0.5f;
                float v = (sin(angle) + 1.0f) * 0.5f;
                edgeIndices[j] = addVertex(x, -halfH, z, 0.0f, -1.0f, 0.0f, u, v);
            }
            for (uint32_t j = 0; j < radialSegments; ++j) {
                uint32_t nextJ = (j + 1) % radialSegments;
                indices.push_back(centerIdx);
                indices.push_back(edgeIndices[nextJ]);
                indices.push_back(edgeIndices[j]);
            }
        }

        // 顶面
        if (topCap && topRadius > 0.0f) {
            uint32_t centerIdx = addVertex(0.0f, halfH, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f);
            std::vector<uint32_t> edgeIndices(radialSegments);
            for (uint32_t j = 0; j < radialSegments; ++j) {
                float angle = j * angleStep;
                float x = topRadius * cos(angle);
                float z = topRadius * sin(angle);
                float u = (cos(angle) + 1.0f) * 0.5f;
                float v = (sin(angle) + 1.0f) * 0.5f;
                edgeIndices[j] = addVertex(x, halfH, z, 0.0f, 1.0f, 0.0f, u, v);
            }
            for (uint32_t j = 0; j < radialSegments; ++j) {
                uint32_t nextJ = (j + 1) % radialSegments;
                indices.push_back(centerIdx);
                indices.push_back(edgeIndices[j]);
                indices.push_back(edgeIndices[nextJ]);
            }
        }

        geometry->setVertices(vertices);
        geometry->setIndices(indices);
        geometry->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
        VertexLayout layout;
        layout.addBinding(0, 8 * sizeof(float), RHI::VertexInputRate::PerVertex);
        layout.addAttribute(0, 0, RHI::Format::RGB32_Float, 0);
        layout.addAttribute(1, 0, RHI::Format::RGB32_Float, 3 * sizeof(float));
        layout.addAttribute(2, 0, RHI::Format::RG32_Float, 6 * sizeof(float));
        geometry->setVertexLayout(layout);
        geometry->setSubmeshes({ {0, (uint32_t)indices.size(), 0} });
        geometry->uploadToGPU();
        return geometry;
    }

    // ==================== Frustum (Wireframe) ====================
    std::shared_ptr<Geometry> GeometryGenerator::createFrustum(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        float nearPlane,
        float farPlane,
        float fovDegrees,
        float aspectRatio)
    {
        float fovRad = glm::radians(fovDegrees);
        float tanHalfFov = tanf(fovRad * 0.5f);
        float nearH = nearPlane * tanHalfFov;
        float nearW = nearH * aspectRatio;
        float farH = farPlane * tanHalfFov;
        float farW = farH * aspectRatio;

        // 8 个顶点 (近平面4个, 远平面4个)
        std::vector<glm::vec3> verticesPos = {
            {-nearW, -nearH, nearPlane}, { nearW, -nearH, nearPlane},
            { nearW,  nearH, nearPlane}, {-nearW,  nearH, nearPlane},
            {-farW,  -farH, farPlane},   { farW,  -farH, farPlane},
            { farW,   farH, farPlane},   {-farW,   farH, farPlane}
        };

        // 12 条线段
        std::vector<std::pair<int, int>> edges = {
            {0,1},{1,2},{2,3},{3,0}, // 近平面
            {4,5},{5,6},{6,7},{7,4}, // 远平面
            {0,4},{1,5},{2,6},{3,7}  // 连接线
        };

        std::vector<float> vertexData;
        for (const auto& p : verticesPos) {
            vertexData.push_back(p.x);
            vertexData.push_back(p.y);
            vertexData.push_back(p.z);
        }
        std::vector<uint32_t> indices;
        for (const auto& e : edges) {
            indices.push_back(e.first);
            indices.push_back(e.second);
        }
        auto geometry = std::make_shared<Geometry>(resMgr);
        geometry->setVertices(vertexData);
        geometry->setIndices(indices);
        geometry->setPrimitiveTopology(RHI::PrimitiveTopology::LineList);
        VertexLayout layout;
        layout.addBinding(0, 3 * sizeof(float), RHI::VertexInputRate::PerVertex);
        layout.addAttribute(0, 0, RHI::Format::RGB32_Float, 0);
        geometry->setVertexLayout(layout);
        geometry->setSubmeshes({ {0, (uint32_t)indices.size(), 0} });
        geometry->uploadToGPU();
        return geometry;
    }

} // namespace StarryEngine::Assets