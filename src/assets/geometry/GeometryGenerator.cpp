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

                // 法线（归一化位置）
                float nx = sinPhi * cosTheta;
                float ny = cosPhi;
                float nz = sinPhi * sinTheta;

                // 纹理坐标
                float u = (float)j / sectorCount;
                float v = (float)i / stackCount;

                // ---------- 计算切线 ----------
                // 切线：沿纬度方向 (theta 增大)
                float tx = -sinTheta;   // 注意 sinPhi 抵消了
                float ty = 0.0f;
                float tz = cosTheta;
                glm::vec3 tangent = glm::normalize(glm::vec3(tx, ty, tz));

                // 副切线：沿经度方向 (phi 增大)
                float bx = cosPhi * cosTheta;
                float by = -sinPhi;
                float bz = cosPhi * sinTheta;
                glm::vec3 bitangent = glm::normalize(glm::vec3(bx, by, bz));

                // 计算 handedness（通常为 1.0，取决于 UV 方向与叉积结果是否一致）
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

        // 索引生成保持不变
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

        // 更新顶点布局：每个顶点现在是 3(pos) + 3(norm) + 2(uv) + 4(tangent) = 12 floats
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
        float width,
        float height,
        float depth)
    {
        float halfL = width * 0.5f;
        float halfW = depth * 0.5f;   // Z 轴方向
        float halfH = height * 0.5f;  // Y 轴方向

        // 8 个顶点位置（局部坐标，原点为中心）
        std::array<glm::vec3, 8> positions = {
            glm::vec3(-halfL, -halfW, -halfH), // 0 - 左后下
            glm::vec3(halfL, -halfW, -halfH), // 1 - 右后下
            glm::vec3(halfL,  halfW, -halfH), // 2 - 右前下
            glm::vec3(-halfL,  halfW, -halfH), // 3 - 左前下
            glm::vec3(-halfL, -halfW,  halfH), // 4 - 左后上
            glm::vec3(halfL, -halfW,  halfH), // 5 - 右后上
            glm::vec3(halfL,  halfW,  halfH), // 6 - 右前上
            glm::vec3(-halfL,  halfW,  halfH)  // 7 - 左前上
        };

        // 6 个面，每个面由 4 个顶点索引构成（顺序：左下、右下、右上、左上）
        std::array<std::array<uint32_t, 4>, 6> faces = { {
            {0, 1, 2, 3}, // 底面 (-Z)
            {4, 5, 6, 7}, // 顶面 (+Z)
            {0, 4, 7, 3}, // 左面 (-X)
            {1, 5, 6, 2}, // 右面 (+X)
            {0, 1, 5, 4}, // 后面 (-Y)
            {3, 2, 6, 7}  // 前面 (+Y)
        } };

        // 每个面的法线
        std::array<glm::vec3, 6> normals = {
            glm::vec3(0.0f,  0.0f, -1.0f), // 底面
            glm::vec3(0.0f,  0.0f,  1.0f), // 顶面
            glm::vec3(-1.0f,  0.0f,  0.0f), // 左面
            glm::vec3(1.0f,  0.0f,  0.0f), // 右面
            glm::vec3(0.0f, -1.0f,  0.0f), // 后面
            glm::vec3(0.0f,  1.0f,  0.0f)  // 前面
        };

        // 每个面的 UV（所有面相同）
        std::array<glm::vec2, 4> texCoords = {
            glm::vec2(0.0f, 0.0f), // 左下
            glm::vec2(1.0f, 0.0f), // 右下
            glm::vec2(1.0f, 1.0f), // 右上
            glm::vec2(0.0f, 1.0f)  // 左上
        };

        std::vector<float> vertexData; // 每个顶点：pos(3) + normal(3) + uv(2) = 8 floats
        std::vector<uint32_t> indices;

        for (int faceIdx = 0; faceIdx < 6; ++faceIdx) {
            const auto& face = faces[faceIdx];
            const glm::vec3& normal = normals[faceIdx];

            // 为当前面的 4 个顶点生成数据
            for (int i = 0; i < 4; ++i) {
                const auto& pos = positions[face[i]];
                const auto& uv = texCoords[i];
                vertexData.push_back(pos.x);
                vertexData.push_back(pos.y);
                vertexData.push_back(pos.z);
                vertexData.push_back(normal.x);
                vertexData.push_back(normal.y);
                vertexData.push_back(normal.z);
                vertexData.push_back(uv.x);
                vertexData.push_back(uv.y);
            }

            // 生成索引（两个三角形）
            uint32_t baseIdx = faceIdx * 4;
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 1);
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 3);
        }

        auto geometry = std::make_shared<Geometry>(resMgr);
        geometry->setVertices(vertexData);
        geometry->setIndices(indices);
        geometry->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);

        VertexLayout layout;
        layout.addBinding(0, 8 * sizeof(float), RHI::VertexInputRate::PerVertex);
        layout.addAttribute(0, 0, RHI::Format::RGB32_Float, 0);                // 位置
        layout.addAttribute(1, 0, RHI::Format::RGB32_Float, 3 * sizeof(float)); // 法线
        layout.addAttribute(2, 0, RHI::Format::RG32_Float, 6 * sizeof(float));  // UV
        geometry->setVertexLayout(layout);

        geometry->setSubmeshes({ {0, static_cast<uint32_t>(indices.size()), 0} });
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
        // 位置、法线（朝上）、UV
        std::vector<float> vertices = {
            -hw, -hh, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f,
             hw, -hh, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,
             hw,  hh, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,
            -hw,  hh, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f
        };
        std::vector<uint32_t> indices = { 0,1,2, 0,2,3 };

        auto geometry = std::make_shared<Geometry>(resMgr);
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

        // 辅助：添加顶点
        auto addVertex = [&](float x, float y, float z, float nx, float ny, float nz, float u, float v) {
            vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
            vertices.push_back(nx); vertices.push_back(ny); vertices.push_back(nz);
            vertices.push_back(u); vertices.push_back(v);
            return (vertices.size() / 8) - 1;
            };

        // 存储每一圈的顶点索引
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
                // 法线：径向方向（近似，对于锥体需修正）
                float nx = cos(angle);
                float nz = sin(angle);
                float ny = 0.0f; // 简化，实际需考虑倾斜
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