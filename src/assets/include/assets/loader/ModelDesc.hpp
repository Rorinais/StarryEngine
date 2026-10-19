#pragma once
// ────────────────────────────────────────────────────────────────
// 模型装配：资产(模型) 与 材质(shader/贴图) 分离，两者可独立替换。
//
//   ModelDesc        只描述资产：文件 / 骨骼 / 位置缩放
//   ModelData        loadModel 的结果：几何 + 骨骼 + 动画 + 自带材质参数
//   MaterialSetDesc  只描述材质策略：别名→MaterialDesc(含 shader)、描边
//   ModelInstance    load() 组装结果：几何 + 每子网格材质 + 描边材质
// ────────────────────────────────────────────────────────────────
#include <assets/AssetType.hpp>
#include <assets/loader/MaterialDesc.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace StarryEngine::Assets {

class Geometry;
class Skeleton;
class AnimationClip;
class MaterialInstance;

/// ① 资产描述（不含任何 shader/材质信息）
struct ModelDesc {
    std::string name = "Model";
    std::string file;                   ///< 模型文件路径
    std::string textureDir;             ///< 贴图目录（材质槽相对文件名按此解析）
    bool        skeleton = false;       ///< 是否加载骨骼与动画
    glm::vec3   position{ 0.0f };
    glm::vec3   scale{ 1.0f };
};

/// 模型加载结果（仍不含材质策略）
struct ModelData {
    std::string                    textureDir; ///< 贴图目录（供材质槽解析相对路径）
    std::shared_ptr<Geometry>      geometry;   ///< 已 uploadToGPU
    std::shared_ptr<Skeleton>      skeleton;
    std::shared_ptr<AnimationClip> clip;
    bool                           skinned = false;
    std::vector<MaterialParams>    params;     ///< 模型自带的材质参数(名字等)
};

/// 一条"子网格别名 → 材质"绑定（材质里含 shader 路径，可替换）
struct SubmeshBinding {
    std::vector<std::string> aliases;
    MaterialDesc             material;
};

/// ② 材质策略描述（与模型无关，可整体替换）
struct MaterialSetDesc {
    std::vector<SubmeshBinding> bindings;      ///< 子网格 → 材质
    std::string fallbackVertexShader;          ///< 未命中时的默认 shader
    std::string fallbackFragmentShader;
    int         fallbackLayer = 1;             ///< RenderLayers::Opaque

    bool        outline = false;
    std::string outlineVertexShader;
    std::string outlineFragmentShader;
    float       outlineScale = 1.03f;
    std::string outlineTexture;                ///< 可空 = 内联白色
    std::string vertexShaderOverride;          ///< 非空则强制替换所有子网格顶点着色器
};

/// ③ 组装结果
struct ModelInstance {
    std::shared_ptr<Geometry>      geometry;
    std::shared_ptr<Skeleton>      skeleton;
    std::shared_ptr<AnimationClip> clip;
    bool                           skinned = false;
    std::vector<std::pair<std::string, std::shared_ptr<MaterialInstance>>> parts;
    std::shared_ptr<MaterialInstance> outlineMaterial;
};

class ModelFactory {
public:
    /// 只加载资产（几何/骨骼/动画），不创建任何材质
    static bool loadModel(const std::shared_ptr<RHI::ResourceManager>& resMgr,
                          const ModelDesc& desc, ModelData& out);

    /// 只创建材质（子网格名 → 材质 + 描边），输入是 loadModel 的结果 + 材质策略
    static bool loadMaterials(const std::shared_ptr<RHI::ResourceManager>& resMgr,
                              RHI::DescriptorPoolHandle pool,
                              const ModelData& data,
                              const MaterialSetDesc& set,
                              ModelInstance& out);

    /// 组装：资产 + 材质策略（常用入口）
    static bool load(const std::shared_ptr<RHI::ResourceManager>& resMgr,
                     RHI::DescriptorPoolHandle pool,
                     const ModelDesc& desc,
                     const MaterialSetDesc& set,
                     ModelInstance& out);
};

} // namespace StarryEngine::Assets
