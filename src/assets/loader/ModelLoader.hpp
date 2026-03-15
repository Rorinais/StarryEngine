#pragma once
#include"../geometry/Geometry.hpp"

namespace StarryEngine::Assets {

    class ModelLoader {
    public:
        static bool loadFromFile(const std::string& path,Geometry& outGeometry,std::vector<MaterialParams>& outMaterials);

    private:
        static void processMesh(
            aiMesh* mesh,
            std::vector<float>& outVertices,
            std::vector<uint32_t>& outIndices,
            std::vector<Submesh>& outSubmeshes,
            const VertexLayout& layout,
            uint32_t stride, const glm::mat4& transform);

        static void extractMaterials(const aiScene* scene,
            std::vector<MaterialParams>& outMaterials);
    };

} // namespace StarryEngine::Assets
