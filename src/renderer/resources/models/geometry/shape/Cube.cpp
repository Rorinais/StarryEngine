#include "Cube.hpp"
namespace StarryEngine {
	Geometry::Ptr Cube::generateGeometry()const {
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		auto halfL = mLength * 0.5f;
		auto halfW = mWidth * 0.5f;
		auto halfH = mHeight * 0.5f;

		std::array<glm::vec3, 8> positions = {
			// 底部四个顶点 (z = -halfH)
			glm::vec3(-halfL, -halfW, -halfH), // 0 - 左后下
			glm::vec3(halfL, -halfW, -halfH), // 1 - 右后下
			glm::vec3(halfL,  halfW, -halfH), // 2 - 右前下
			glm::vec3(-halfL,  halfW, -halfH), // 3 - 左前下

			// 顶部四个顶点 (z = halfH)
			glm::vec3(-halfL, -halfW,  halfH), // 4 - 左后上
			glm::vec3(halfL, -halfW,  halfH), // 5 - 右后上
			glm::vec3(halfL,  halfW,  halfH), // 6 - 右前上
			glm::vec3(-halfL,  halfW,  halfH)  // 7 - 左前上
		};

		for (auto& pos : positions) {
			pos += mOrigin;
		}
		std::array<std::array<uint32_t, 4>, 6> faces = { {
			{0, 1, 2, 3}, // 底面 (-Z)
			{4, 5, 6, 7}, // 顶面 (+Z)
			{0, 4, 7, 3}, // 左面 (-X)
			{1, 5, 6, 2}, // 右面 (+X)
			{0, 1, 5, 4}, // 后面 (-Y)
			{3, 2, 6, 7}  // 前面 (+Y)
		} };

		std::array<glm::vec3, 6> normals = {
			glm::vec3(0.0f,  0.0f, -1.0f), // 底面
			glm::vec3(0.0f,  0.0f,  1.0f), // 顶面
			glm::vec3(-1.0f,  0.0f,  0.0f), // 左面
			glm::vec3(1.0f,  0.0f,  0.0f), // 右面
			glm::vec3(0.0f, -1.0f,  0.0f), // 后面
			glm::vec3(0.0f,  1.0f,  0.0f)  // 前面
		};

		std::array<glm::vec2, 4> texCoords = {
			glm::vec2(0.0f, 0.0f), // 左下
			glm::vec2(1.0f, 0.0f), // 右下
			glm::vec2(1.0f, 1.0f), // 右上
			glm::vec2(0.0f, 1.0f)  // 左上
		};

		for (int faceIdx = 0; faceIdx < 6; ++faceIdx) {
			const auto& face = faces[faceIdx];
			const glm::vec3& normal = normals[faceIdx];

			glm::vec3 tangent, bitangent;
			if (faceIdx == 0 || faceIdx == 1) { // 底面或顶面 (Z方向)
				tangent = glm::vec3(1.0f, 0.0f, 0.0f);
				bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
			}
			else if (faceIdx == 2 || faceIdx == 3) { // 左面或右面 (X方向)
				tangent = glm::vec3(0.0f, 0.0f, 1.0f);
				bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
			}
			else { // 后面或前面 (Y方向)
				tangent = glm::vec3(1.0f, 0.0f, 0.0f);
				bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
			}
			if (glm::dot(glm::cross(tangent, bitangent), normal) < 0.0f) tangent = -tangent;

			for (int i = 0; i < 4; ++i) {
				Vertex vertex;
				vertex.position = positions[face[i]];
				vertex.normal = normal;
				vertex.texCoord = texCoords[i];
				vertex.tangent = tangent;
				vertex.bitangent = bitangent;
				vertices.push_back(vertex);
			}

			uint32_t baseIdx = faceIdx * 4;
			indices.push_back(baseIdx + 0);
			indices.push_back(baseIdx + 1);
			indices.push_back(baseIdx + 2);
			indices.push_back(baseIdx + 0);
			indices.push_back(baseIdx + 2);
			indices.push_back(baseIdx + 3);
		}
		return Geometry::create(vertices, indices);
	}
	void Cube::getBoundingBox(glm::vec3& min, glm::vec3& max)const {
		min = mOrigin - glm::vec3(mLength * 0.5f, mWidth * 0.5f, mHeight * 0.5f);
		max = mOrigin + glm::vec3(mLength * 0.5f, mWidth * 0.5f, mHeight * 0.5f);
	}
}