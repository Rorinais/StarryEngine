#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/normal.hpp>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include"Geometry.hpp"

namespace StarryEngine {
    void Geometry::applyTransform(const glm::mat4& transform) {
        // 提取变换矩阵的左上角3x3部分用于法向量的变换
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(transform)));

        for (auto& vertex : vertices) {
            // 变换位置
            glm::vec4 pos = transform * glm::vec4(vertex.position, 1.0f);
            vertex.position = glm::vec3(pos);

            // 变换法线
            vertex.normal = normalMatrix * vertex.normal;

            // 变换切线和副切线
            vertex.tangent = normalMatrix * vertex.tangent;
            vertex.bitangent = normalMatrix * vertex.bitangent;
        }
    }

    void Geometry::calculateNormals() {
        // 重置所有法线为零
        for (auto& vertex : vertices) {
            vertex.normal = glm::vec3(0.0f);
        }

        // 遍历所有三角形面
        for (size_t i = 0; i < indices.size(); i += 3) {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];

            Vertex& v0 = vertices[i0];
            Vertex& v1 = vertices[i1];
            Vertex& v2 = vertices[i2];

            // 计算面法线
            glm::vec3 edge1 = v1.position - v0.position;
            glm::vec3 edge2 = v2.position - v0.position;
            glm::vec3 faceNormal = glm::cross(edge1, edge2);

            // 避免零向量
            if (glm::length(faceNormal) > 0.0001f) {
                faceNormal = glm::normalize(faceNormal);

                // 累加到顶点法线
                v0.normal += faceNormal;
                v1.normal += faceNormal;
                v2.normal += faceNormal;
            }
        }

        // 标准化所有法线
        for (auto& vertex : vertices) {
            if (glm::length(vertex.normal) > 0.0001f) {
                vertex.normal = glm::normalize(vertex.normal);
            }
            else {
                // 如果法线长度为零（孤立顶点），设置默认上向量
                vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }
    }

    void Geometry::generateTangents() {
        // 重置切空间向量
        for (auto& vertex : vertices) {
            vertex.tangent = glm::vec3(0.0f);
            vertex.bitangent = glm::vec3(0.0f);
        }

        // 遍历所有三角形面
        for (size_t i = 0; i < indices.size(); i += 3) {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];

            Vertex& v0 = vertices[i0];
            Vertex& v1 = vertices[i1];
            Vertex& v2 = vertices[i2];

            // 计算边向量
            glm::vec3 edge1 = v1.position - v0.position;
            glm::vec3 edge2 = v2.position - v0.position;

            // 计算纹理坐标差
            glm::vec2 deltaUV1 = v1.texCoord - v0.texCoord;
            glm::vec2 deltaUV2 = v2.texCoord - v0.texCoord;

            // 计算行列式
            float det = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;

            // 避免除以零
            if (fabs(det) < 0.0001f) {
                continue;
            }

            float invDet = 1.0f / det;

            // 计算切线和副切线
            glm::vec3 tangent = invDet * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
            glm::vec3 bitangent = invDet * (-deltaUV2.x * edge1 + deltaUV1.x * edge2);

            // 累加到顶点
            v0.tangent += tangent;
            v1.tangent += tangent;
            v2.tangent += tangent;

            v0.bitangent += bitangent;
            v1.bitangent += bitangent;
            v2.bitangent += bitangent;
        }

        // 正交化切空间向量并确保与法线垂直
        for (auto& vertex : vertices) {
            if (glm::length(vertex.tangent) < 0.0001f ||
                glm::length(vertex.bitangent) < 0.0001f) {
                // 如果切空间向量无效，使用默认值
                glm::vec3 normal = vertex.normal;
                if (fabs(normal.x) > fabs(normal.y)) {
                    vertex.tangent = glm::normalize(glm::vec3(normal.z, 0.0f, -normal.x));
                }
                else {
                    vertex.tangent = glm::normalize(glm::vec3(0.0f, -normal.z, normal.y));
                }
                vertex.bitangent = glm::cross(normal, vertex.tangent);
            }
            else {
                // 正交化切空间
                vertex.tangent = glm::normalize(vertex.tangent);
                vertex.bitangent = glm::normalize(vertex.bitangent);

                // 确保切空间与法线垂直
                vertex.tangent = glm::normalize(vertex.tangent - vertex.normal * glm::dot(vertex.normal, vertex.tangent));
                vertex.bitangent = glm::normalize(vertex.bitangent - vertex.normal * glm::dot(vertex.normal, vertex.bitangent));

                // 确保切空间正交
                float dotTangentBitangent = glm::dot(vertex.tangent, vertex.bitangent);
                if (fabs(dotTangentBitangent) > 0.01f) {
                    vertex.bitangent = glm::normalize(vertex.bitangent - vertex.tangent * dotTangentBitangent);
                }
            }
        }
    }
}