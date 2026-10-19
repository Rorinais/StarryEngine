#pragma once
// ────────────────────────────────────────────────────────────────
// 材质描述：把所有"可控制属性"合并成一个参数。
// 目标：外部只需构造一个 MaterialDesc，调用 MaterialFactory::create() 即可得到
// 完整可用的材质，不再分散地调用十几个 setter。
// ────────────────────────────────────────────────────────────────
#include <renderer/interface/RHIStructs.hpp>
#include <renderer/interface/RHIEnums.hpp>
#include <glm/glm.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace StarryEngine::RHI { class ResourceManager; }

namespace StarryEngine::Assets {

class MaterialTemplate;
class MaterialInstance;

namespace RHI = ::StarryEngine::RHI;

/// 纹理槽：按名字绑定（名字来自 shader 反射），或按 (set,binding) 绑定。
struct MaterialTextureSlot {
    std::string           name;          ///< 反射里的采样器名（如 "uBaseColor"）
    std::string           requiredKeyword; ///< 非空时：只有该关键字开启才绑定（对应 CharMatSpec 的 keyword 列）
    std::string           path;          ///< 若给路径则自动加载（与 texture 二选一）
    RHI::TextureHandle    texture = RHI::TextureHandle::Null();
    RHI::SamplerHandle    sampler = RHI::SamplerHandle::Null();
    RHI::Format           format = RHI::Format::RGBA8_sRGB;   ///< 路径加载时使用
    /// 无文件时用内存数据（如 1x1 白色）：非空则优先于 path 走 loadTextureFromMemory
    std::vector<uint8_t>  inlineData;
    uint32_t              inlineWidth = 1;
    uint32_t              inlineHeight = 1;
    // 按 (set,binding) 直接绑定（name 为空时使用）
    uint32_t              set = 0xFFFFFFFFu;
    uint32_t              binding = 0xFFFFFFFFu;
    RHI::ImageLayout      layout = RHI::ImageLayout::ShaderReadOnly;
    bool                  asInputAttachment = false;
};

/// UBO / SSBO 数据块
struct MaterialBufferSlot {
    std::string  name;                   ///< 反射里的块名（如 "LightingUBO"）
    uint32_t     set = 0xFFFFFFFFu;
    uint32_t     binding = 0xFFFFFFFFu;
    const void*  data = nullptr;
    size_t       size = 0;
    uint32_t     slot = 0xFFFFFFFFu;     ///< 每帧槽位，默认自动
    bool         isStorage = false;
};

struct MaterialDesc {
    std::string name = "Material";
    std::string textureDir;                  ///< 纹理目录：槽里的 path 可以是相对文件名

    // ── 着色器 ──
    std::string              vertexShaderPath;
    std::string              fragmentShaderPath;
    std::vector<std::string> variants;      ///< 着色器关键字（如 USE_ILM_MAP）

    // ── 绑定 ──
    std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> layouts;   ///< 模板布局（多集）
    RHI::DescriptorSetLayoutHandle baseSetLayout = RHI::DescriptorSetLayoutHandle::Null(); ///< 单一基础布局（layouts 为空时使用）
    RHI::DescriptorSetHandle  globalSet = RHI::DescriptorSetHandle::Null();
    std::vector<MaterialTextureSlot> textures;
    std::vector<MaterialBufferSlot>  buffers;

    // ── 便捷 PBR 参数（写进同名 UBO 字段；没有对应块时忽略）──
    glm::vec4 baseColor{ 1.0f };
    glm::vec3 emissiveColor{ 0.0f };
    float     emissiveIntensity = 0.0f;
    float     metallic = 0.0f;
    float     roughness = 0.5f;
    float     ambientOcclusion = 1.0f;
    float     alphaThreshold = 0.0f;
    bool      alphaBlend = false;

    // ── 渲染状态（原来散落的十几个 setter）──
    RHI::CullMode                cullMode = RHI::CullMode::None;   ///< 默认双面(与 MaterialInstance 一致)
    bool                         depthTest = true;
    bool                         depthWrite = true;
    RHI::CompareOp               depthCompareOp = RHI::CompareOp::Less;
    bool                         stencilTest = false;
    RHI::StencilOpState          stencilFront;
    RHI::StencilOpState          stencilBack;
    std::vector<RHI::BlendAttachmentState> attachments;
    bool                         deferred = false;
    int                          renderLayer = -1;   ///< RenderLayers 索引（-1=未指定）
    int                          renderQueue = 0;    ///< 同层绘制顺序
    uint32_t                     currentSlot = 0xFFFFFFFFu;
};

class MaterialFactory {
public:
    static std::shared_ptr<MaterialInstance> create(const std::shared_ptr<RHI::ResourceManager>& resMgr,
                                                    RHI::DescriptorPoolHandle pool,
                                                    const MaterialDesc& desc);
};

} // namespace StarryEngine::Assets
