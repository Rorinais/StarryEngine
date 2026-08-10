#pragma once
#include <memory>
#include <optional>
#include <string>
#include <assets/geometry/Geometry.hpp>
#include <assets/animation/Skeleton.hpp>
#include <assets/animation/AnimationClip.hpp>
#include <assets/loader/TextureLoader.hpp>

namespace Assimp { class Importer; }   // 前向声明，避免头文件引入 assimp

namespace StarryEngine::Assets {
    class ModelLoader {
    public:
        struct Config {
            bool calcTangents = true;   // 计算切线
            Config();
        };

        explicit ModelLoader(std::shared_ptr<RHI::ResourceManager> resMgr, Config config = Config());
        ~ModelLoader();

        // 打开模型文件（解析一次，缓存 scene 与单位缩放）。可反复调用以加载其他文件。
        bool open(const std::string& path);
        void close();
        bool isOpen() const { return m_scene != nullptr; }
        const std::string& currentPath() const { return m_path; }

        // 各取所需（职责单一，各返回自己的结果）
        std::optional<Skeleton> loadSkeleton();          // 骨骼层级（内部缓存，供 geometry/clip 复用）
        std::optional<Geometry> loadGeometry();          // 网格（复用已构建的 skeleton）
        std::optional<AnimationClip> loadClip();         // 动画（复用已构建的 skeleton）
        std::vector<MaterialParams> loadMaterials();     // 材质

        void clearCache();

    private:
        void ensureSkeleton();
        void buildSkeleton(Skeleton& out);
        void processMesh(aiMesh* mesh,
            std::vector<float>& outVertices,
            std::vector<uint32_t>& outIndices,
            std::vector<Submesh>& outSubmeshes,
            const VertexLayout& layout,
            uint32_t stride, const glm::mat4& transform);
        void extractMaterials(const aiScene* scene,
            std::vector<MaterialParams>& outMaterials);
        TextureLoadResult loadEmbeddedTexture(TextureLoader& loader, aiTexture* tex, const std::string& debugName);

        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        Config m_config;
        std::unique_ptr<Assimp::Importer> m_importer;   // 持有 aiScene 生命周期
        const aiScene* m_scene = nullptr;
        std::string m_path;
        float m_unitScale = 1.0f;                        // 单位缩放（cm→m），open 时计算
        std::optional<Skeleton> m_skeleton;              // 缓存，供 geometry/clip 复用
    };

} // namespace StarryEngine::Assets
