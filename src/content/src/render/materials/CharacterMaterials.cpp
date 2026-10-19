#include <content/render/materials/CharacterMaterials.hpp>
#include <renderer/renderPaths/LayerRegistry.hpp>

namespace StarryEngine::Content {

StarryEngine::Assets::MaterialSetDesc makeCharacterMaterialSet(const SceneResources& res) {
    using StarryEngine::Assets::MaterialDesc;
    using StarryEngine::Assets::MaterialTextureSlot;
    using StarryEngine::RHI::Format;

    StarryEngine::Assets::MaterialSetDesc set;
    auto add = [&set, &res](const char* frag,
                   std::initializer_list<const char*> keywords,
                   std::initializer_list<MaterialTextureSlot> slots,
                   std::initializer_list<const char*> aliases) {
                    
        MaterialDesc d;
        d.fragmentShaderPath = frag;
        for (auto k : keywords) d.variants.push_back(k);
        d.textures.assign(slots.begin(), slots.end());
        d.renderLayer = RenderLayers::Opaque;
        d.renderQueue = 0;
        d.depthTest = true;
        d.depthWrite = true;

        std::vector<std::string> a;
        for (auto n : aliases) a.push_back(n);
        if (!a.empty()) d.name = a.front();

        StarryEngine::Assets::SubmeshBinding b; 
        b.aliases = std::move(a); 
        b.material = std::move(d);
        b.material.baseSetLayout = res.globalSetLayout;

        set.bindings.push_back(std::move(b));
    };

    add("assets/shaders/npr_toon/face.frag", { "USE_SDF_MAP", "USE_MASK_MAP" },
        {
            { .name = "texSampler", .path = "face_baseColor.png", .format = Format::RGBA8_UNorm },
            { .name = "uSdfMap", .requiredKeyword = "USE_SDF_MAP", .path = "face_sdf.tga", .format = Format::RGBA8_UNorm },
            { .name = "uMaskMap", .requiredKeyword = "USE_MASK_MAP", .path = "face_mask.tga", .format = Format::RGBA8_UNorm },
        },
        { "顏", "颜", "脸", "face" });

    add("assets/shaders/npr_toon/face.frag", { "USE_SDF_MAP", "USE_MASK_MAP" },
        {
            { .name = "texSampler", .path = "expression.png",     .format = Format::RGBA8_UNorm },
            { .name = "uSdfMap", .requiredKeyword = "USE_SDF_MAP", .path = "face_sdf.tga", .format = Format::RGBA8_UNorm },
            { .name = "uMaskMap", .requiredKeyword = "USE_MASK_MAP", .path = "face_mask.tga", .format = Format::RGBA8_UNorm },
        },
        { "表情" });

    add("assets/shaders/npr_toon/hair.frag", { "USE_ILM_MAP", "USE_RAMP_MAP" },
        {
            { .name = "texSampler", .path = "hair_baseColor.png", .format = Format::RGBA8_UNorm },
            { .name = "uIlmMap", .requiredKeyword = "USE_ILM_MAP", .path = "fair_ilm.tga", .format = Format::RGBA8_UNorm },
            { .name = "uRampMap", .requiredKeyword = "USE_RAMP_MAP", .path = "ramp.png", .format = Format::RGBA8_UNorm },
        },
        { "髪", "hair" });

    add("assets/shaders/npr_toon/shader.frag", { "USE_ILM_MAP", "USE_RAMP_MAP", "USE_MATCAP_MAP" },
        {
            { .name = "texSampler", .path = "clothes2_baseColor.png", .format = Format::RGBA8_UNorm },
            { .name = "uIlmMap", .requiredKeyword = "USE_ILM_MAP", .path = "clothes2_ilm.tga", .format = Format::RGBA8_UNorm },
            { .name = "uRampMap", .requiredKeyword = "USE_RAMP_MAP", .path = "ramp.png", .format = Format::RGBA8_UNorm },
            { .name = "uMatcapMap", .requiredKeyword = "USE_MATCAP_MAP", .path = "matcap_mat.png", .format = Format::RGBA8_UNorm },
        },
        { "上半身" });

    add("assets/shaders/npr_toon/shader.frag", { "USE_ILM_MAP", "USE_RAMP_MAP", "USE_MATCAP_MAP" },
        {
            { .name = "texSampler", .path = "clothes1_baseColor.png", .format = Format::RGBA8_UNorm },
            { .name = "uIlmMap", .requiredKeyword = "USE_ILM_MAP", .path = "clothes1_ilm.tga", .format = Format::RGBA8_UNorm },
            { .name = "uRampMap", .requiredKeyword = "USE_RAMP_MAP", .path = "ramp.png", .format = Format::RGBA8_UNorm },
            { .name = "uMatcapMap", .requiredKeyword = "USE_MATCAP_MAP", .path = "matcap_mat.png", .format = Format::RGBA8_UNorm },
        },
        { "下裙" });

    // 该模型为蒙皮模型：默认顶点着色器用蒙皮版（想换材质只改这里或 bindings 里的 MaterialDesc）
    set.fallbackVertexShader   = "assets/shaders/npr_toon/shader_skinned.vert";
    set.fallbackFragmentShader = "assets/shaders/npr_toon/shader.frag";
    set.fallbackLayer = 1;
    set.outline = true;
    set.outlineVertexShader   = "assets/shaders/npr_toon/shader_skinned.vert";
    set.outlineFragmentShader = "assets/shaders/npr_toon/outline.frag";
    set.outlineScale = 1.03f;
    return set;
}

} // namespace StarryEngine::Content
