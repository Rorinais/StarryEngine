#pragma once
#include"../geometry/Geometry.hpp"
#include "../animation/Skeleton.hpp"
#include "../animation/AnimationClip.hpp"
#include "TextureLoader.hpp"

namespace StarryEngine::Assets {

    class ModelLoader {
    public:
        static bool loadFromFile(
            std::shared_ptr<RHI::ResourceManager> resMgr,
            const std::string& path,Geometry& outGeometry,
            std::vector<MaterialParams>& outMaterials,
            Skeleton* outSkeleton = nullptr,
            AnimationClip* outClip = nullptr);

    private:
        static void processMesh(
            aiMesh* mesh,
            std::vector<float>& outVertices,
            std::vector<uint32_t>& outIndices,
            std::vector<Submesh>& outSubmeshes,
            const VertexLayout& layout,
            uint32_t stride, const glm::mat4& transform);

        static void extractMaterials(const aiScene* scene,
            std::vector<MaterialParams>& outMaterials, std::shared_ptr<RHI::ResourceManager> resMgr);

        static TextureLoadResult loadEmbeddedTexture(TextureLoader& loader, aiTexture* tex, const std::string& debugName);
    };

} // namespace StarryEngine::Assets
