#pragma once
// ────────────────────────────────────────────────────────────────
// 模型描述：与 MaterialDesc 同一套"单参数创建"模式。
// 子网格名 → 材质 的映射也放在这里（替代 PbrScene 里的 kCharSpecs 并行数组）。
// ────────────────────────────────────────────────────────────────
#include <assets/material/MaterialDesc.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace StarryEngine::Assets {

class Geometry;
class Skeleton;
class AnimationClip;
class MaterialInstance;

/// 一条"子网格别名 → 材质"绑定
struct SubmeshBinding {
    std::vector<std::string> aliases;   ///< 子网格/材质名别名，如 {"顏","颜","脸","face"}
    MaterialDesc             material;
};

struct ModelDesc {
    std::string name = "Model";
    std::string file;                   ///< 模型文件路径
    std::string textureDir;             ///< 纹理目录（材质槽里的相对文件名按此解析）
    bool        skeleton = false;       ///< 是否加载骨骼与动画
    bool        outline = false;        ///< 是否生成描边材质
    float       outlineScale = 0.003f;
    glm::vec4   outlineColor{ 0.0f, 0.0f, 0.0f, 1.0f };
    std::string outlineTexture;         ///< 描边使用的噪声/透明度贴图（可空）
    std::vector<SubmeshBinding> bindings;
    glm::vec3   position{ 0.0f };
    glm::vec3   scale{ 1.0f };
};

/// 加载结果：几何/骨骼/动画 + 每个子网格对应的材质
struct ModelInstance {
    std::shared_ptr<Geometry>       geometry;
    std::shared_ptr<Skeleton>       skeleton;
    std::shared_ptr<AnimationClip>  clip;
    bool                            skinned = false;
    std::vector<std::pair<std::string, std::shared_ptr<MaterialInstance>>> parts;  ///< 子网格名 → 材质
    std::shared_ptr<MaterialInstance> outlineMaterial;                            ///< 可选
};

class ModelFactory {
public:
    /// 按 desc 加载模型并创建各子网格材质；失败返回 false。
    static bool load(const std::shared_ptr<RHI::ResourceManager>& resMgr,
                     RHI::DescriptorPoolHandle pool,
                     const ModelDesc& desc,
                     ModelInstance& out);
};

} // namespace StarryEngine::Assets
