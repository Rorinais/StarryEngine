#include <assets/loader/MaterialDesc.hpp>
#include <content/scenes/pbr/PbrScene.hpp>
#include <renderer/renderPaths/LayerRegistry.hpp>
#include <content/renderpath/RenderPathRegistry.hpp>
#include <assets/loader/MaterialDesc.hpp>
#include <assets/loader/TextureLoader.hpp>

    void PBRDemo::createRenderer() {
        m_scene = std::make_shared<Scene::Scene>();

        m_renderer = std::make_shared<RasterRenderer>(m_rhi, m_descriptorPool, m_scene);
        m_renderer->createGlobalSetLayout();
        m_renderer->createGlobalUniformBuffer();
        m_renderer->initDefaultMaterials();

        m_descriptorSetLayout = m_renderer->getGlobalSetLayout();
        m_descriptorSet = m_renderer->getGlobalDescriptorSet(0);

        StarryEngine::Content::StandardPipeline::declare(m_width, m_height);   // 标准管线：pass + 附件
        auto renderPath = std::make_shared<DeferredRenderPath>(m_rhi, m_width, m_height);
        renderPath->setScene(m_scene.get());
        m_renderPath = renderPath;
        m_renderer->setRenderPath(m_renderPath);

        {
            glm::vec3 lightDir = glm::normalize(glm::vec3(0.5f, 1.0f, 0.8f));  
            glm::vec3 sceneCenter(0.0f, 1.0f, 0.0f);

            float halfExtent = 12.0f;
            float dist = 12.0f;

            glm::vec3 eye = sceneCenter + lightDir * dist;
            glm::mat4 view = glm::lookAt(eye, sceneCenter, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 proj = glm::orthoRH_ZO(-halfExtent, halfExtent, -halfExtent, halfExtent, 0.1f, 26.0f);
            proj[1][1] *= -1.0f;  
            m_renderer->setLightViewProj(proj * view);
        }

        if (StarryEngine::App::runtimeSettings().frameDump) {
            const uint32_t n = StarryEngine::App::runtimeSettings().frameCount;
            const uint32_t fi = (StarryEngine::App::runtimeSettings().frameIndex < 0)
                ? UINT32_MAX : static_cast<uint32_t>(StarryEngine::App::runtimeSettings().frameIndex);
            m_frameExit = StarryEngine::App::runtimeSettings().frameExit;
            m_frameExitTarget = (fi != UINT32_MAX) ? 1u : (n > 0 ? n : UINT32_MAX);
            m_frameCapture = std::make_shared<FrameCapture>(m_rhi);
            if (!m_frameCapture->initialize({ StarryEngine::App::runtimeSettings().frameDumpDir.c_str(), n, fi })) {
                m_frameCapture.reset();
            }
        }
    }

    PBRDemo::PBRDemo(std::shared_ptr<RHI::IRHI> rhi, RHI::DescriptorPoolHandle descriptorPool, uint32_t width, uint32_t height):
    m_rhi(rhi),m_descriptorPool(descriptorPool),m_width(width),m_height(height){
        m_iblBuilder = std::make_shared<Assets::IBLBuilder>(rhi->getResourceManager(), rhi);
        createRenderer();
        createScene();
    }

bool PBRDemo::loadModelGeometry(Assets::Geometry& outGeometry,std::vector<Assets::MaterialParams>& outParams,Assets::Skeleton* outSkeleton) {
    Assets::ModelLoader loader(m_rhi->getResourceManager());
    if (!loader.open("assets/models/fuxuan_Animation2.fbx")) {
        LOG_ERROR("Failed to load model");
        return false;
    }

    auto skeleton = loader.loadSkeleton();
    auto geometry = loader.loadGeometry();
    auto clip     = loader.loadClip();
    auto materials = loader.loadMaterials();
    if (!skeleton || !geometry || !clip) {
        LOG_ERROR("Failed to extract model data");
        return false;
    }

    *outSkeleton = std::move(*skeleton);
    m_modelClip = std::move(*clip);
    m_modelClip.looping = true;

    outParams = std::move(materials);
    outGeometry = std::move(*geometry);

    outGeometry.uploadToGPU();
    return true;
}

namespace {
// 角色材质表：别名 → 一份完整的 MaterialDesc（替代旧的 CharMatSpec 并行数组）。
// 以后加角色/材质只需在此表增行；创建走统一的 MaterialFactory::create()。
std::vector<std::pair<std::vector<std::string>, StarryEngine::Assets::MaterialDesc>>
makeCharMaterialTable() {
    using StarryEngine::Assets::MaterialDesc;
    using StarryEngine::Assets::MaterialTextureSlot;
    using StarryEngine::RHI::Format;

    std::vector<std::pair<std::vector<std::string>, MaterialDesc>> table;
    auto add = [&](const char* frag,
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
        table.emplace_back(std::move(a), std::move(d));
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

    return table;
}
} // namespace

std::shared_ptr<Assets::MaterialInstance> PBRDemo::makeModelMaterial(const Assets::MaterialParams& param, bool skinned) {
    const char* vsPath = skinned ? "assets/shaders/npr_toon/shader_skinned.vert"
                                 : "assets/shaders/npr_toon/shader.vert";

    static const auto kCharMats = makeCharMaterialTable();

    // 按材质名(含别名)查表
    const Assets::MaterialDesc* matched = nullptr;
    for (const auto& [aliases, desc] : kCharMats) {
        for (const auto& alias : aliases) {
            if (param.name == alias) { matched = &desc; break; }
        }
        if (matched) break;
    }

    Assets::TextureLoader loader(m_rhi->getResourceManager());

    // 未命中：默认 frag + 白色贴图（保持原行为）
    if (!matched) {
        Assets::MaterialDesc fallback;
        fallback.name = param.name;
        fallback.vertexShaderPath = vsPath;
        fallback.fragmentShaderPath = "assets/shaders/npr_toon/shader.frag";
        fallback.renderLayer = RenderLayers::Opaque;
        fallback.renderQueue = 0;
        fallback.baseSetLayout = m_descriptorSetLayout;
        auto instance = Assets::MaterialFactory::create(m_rhi->getResourceManager(), m_descriptorPool, fallback);
        if (!instance) return nullptr;
        static const uint8_t kWhite[4] = { 255, 255, 255, 255 };
        auto white = loader.loadTextureFromMemory(kWhite, 1, 1, RHI::Format::RGBA8_UNorm, "White");
        if (white.texture.isValid()) instance->setTexture("texSampler", white.texture, white.sampler);
        return instance;
    }

    // 命中：复制 desc → 补上本次模型相关的字段（顶点着色器、纹理目录），交给统一工厂
    Assets::MaterialDesc d = *matched;
    d.name = param.name;
    d.vertexShaderPath = vsPath;
    d.textureDir = "assets/models/textures/" + m_modelTextureDir;
    d.baseSetLayout = m_descriptorSetLayout;

    auto instance = Assets::MaterialFactory::create(m_rhi->getResourceManager(), m_descriptorPool, d);
    if (!instance) {
        LOG_ERROR("Failed to create material for: {}", param.name);
        return nullptr;
    }

    // 逐材质灯光参数（保持原行为）
    if (auto* lightBlock = instance->getBlock("LightingUBO")) {
        lightBlock->setVec4("lights.position", glm::vec4(-0.5f, -1.0f, -0.8f, 0.0f));
        lightBlock->setVec4("lights.color", glm::vec4(3.0f, 2.7f, 2.3f, 1.0f));
        lightBlock->setFloat("lightCount", 1.0f);
        lightBlock->setFloat("ambientStrength", 0.15f);
    }
    instance->applyAllDirtyBlocks();
    return instance;
}

std::shared_ptr<Assets::MaterialInstance> PBRDemo::makeOutlineMaterial() {
    auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(
        m_rhi->getResourceManager(), m_descriptorSetLayout);
    if (!tmpl->loadShaders("assets/shaders/npr_toon/shader_skinned.vert", "assets/shaders/npr_toon/outline.frag")) {
        LOG_ERROR("Failed to load outline shaders");
        return nullptr;
    }
    auto mat = std::make_shared<Assets::MaterialInstance>(
        tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);

    Assets::TextureLoader loader(m_rhi->getResourceManager());
    static const uint8_t kWhite[4] = { 255, 255, 255, 255 };
    auto white = loader.loadTextureFromMemory(kWhite, 1, 1, RHI::Format::RGBA8_UNorm, "White");
    if (white.texture.isValid())
        mat->setTexture("texSampler", white.texture, white.sampler);

    mat->setRenderLayer(RenderLayers::Outline);
    mat->setRenderQueue(1);
    mat->setDepthTest(true);
    mat->setDepthWrite(false);

    RHI::StencilOpState outline;
    outline.failOp = RHI::StencilOp::Keep;
    outline.passOp = RHI::StencilOp::Keep;       
    outline.depthFailOp = RHI::StencilOp::Keep;
    outline.compareOp = RHI::CompareOp::NotEqual;  
    outline.compareMask = 0xFF;
    outline.writeMask = 0x00;                     
    outline.reference = 1;
    mat->setStencilTest(true);
    mat->setStencilOps(outline, outline);
    return mat;
}

void PBRDemo::addGriseoModel() {
    auto geometry = std::make_shared<Assets::Geometry>(m_rhi->getResourceManager());
    std::vector<Assets::MaterialParams> params;
    if (!loadModelGeometry(*geometry, params, &m_modelSkeleton)) return;

    bool skinned = m_modelClip.isSkeletal() && !m_modelSkeleton.bones.empty();

    std::vector<std::shared_ptr<Assets::MaterialInstance>> materials;
    for (auto& param : params) {
        auto inst = makeModelMaterial(param, skinned);
        if (inst) materials.push_back(inst);
    }

    auto obj = std::make_shared<Scene::RenderObject>();
    obj->geometry = geometry;
    obj->materials = materials;
    obj->transform = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f));
    m_scene->addObject(obj);

    if (const std::string& ec = StarryEngine::App::runtimeSettings().extraChars; !ec.empty()) {
        uint32_t n = std::strtoul(ec.c_str(), nullptr, 10);
        for (uint32_t i = 0; i < n; ++i) {
            auto clone = std::make_shared<Scene::RenderObject>();
            clone->geometry = geometry;
            clone->materials = materials;
            clone->transform = glm::translate(glm::mat4(1.0f),
                glm::vec3(2.0f + static_cast<float>(i + 1) * 2.2f, 0.0f, 0.0f));
            m_scene->addObject(clone);
        }
        LOG_INFO("[demo] 额外角色副本 x{}（draw call 基准）", n);
    }

    m_modelGeometry = geometry;
    m_modelMaterialCount = materials.size();

    m_skeletalReady = skinned;
    if (m_skeletalReady) {
        m_skinnedMaterials = materials;

        AsyncBoneProducer::Config cfg;
        const bool poolActive = (m_renderer && m_renderer->getJobSystem() != nullptr);
        cfg.enableThread = !poolActive && StarryEngine::App::runtimeSettings().asyncBones;
        m_boneProducer = std::make_unique<AsyncBoneProducer>(m_modelSkeleton, m_modelClip, cfg);
        LOG_INFO("[async] 骨骼动画: {}",poolActive ? "池（帧内并行 job）": (cfg.enableThread ? "ON（独立线程）" : "OFF（同步内联）"));

        if (m_renderer && m_renderer->isFrameInFlight()) {
            auto* js = m_renderer->getJobSystem();
            m_renderer->setFrameDataBoneProvider([this, js](uint32_t dataSlot) {
                float d = m_lastDelta;  
                js->submit([this, d, dataSlot]() {
                    const auto& matrices = m_boneProducer->step(d);  
                    if (matrices.empty()) return;
                    size_t bytes = matrices.size() * sizeof(glm::mat4);
                    for (auto& mat : m_skinnedMaterials) if (mat) mat->setStorageBuffer(1, 2, matrices.data(), bytes, dataSlot);
                    if (m_outlineMaterial) m_outlineMaterial->setStorageBuffer(1, 2, matrices.data(), bytes, dataSlot);
                });
            });
        }
    }
}

void PBRDemo::onUpdate(float deltaTime) {
    if (!m_boneProducer || m_skinnedMaterials.empty()) return;

    m_lastDelta = deltaTime;
    if (m_renderer && m_renderer->isFrameInFlight()) return;

    uint32_t targetSlot = m_rhi->getCurrentFrameIndex();
    if (targetSlot >= RHI::kMaxFramesInFlight) targetSlot = 0;

    auto upload = [this, targetSlot](const std::vector<glm::mat4>& matrices) {
        if (matrices.empty()) return;
        size_t bytes = matrices.size() * sizeof(glm::mat4);
        for (auto& mat : m_skinnedMaterials) {
            if (mat) mat->setStorageBuffer(1, 2, matrices.data(), bytes, targetSlot);
        }
        if (m_outlineMaterial) m_outlineMaterial->setStorageBuffer(1, 2, matrices.data(), bytes, targetSlot);
    };

    if (auto* js = (m_renderer ? m_renderer->getJobSystem() : nullptr)) {
        js->submit([this, delta = deltaTime, upload]() {
            const auto& matrices = m_boneProducer->step(delta); 
            upload(matrices);
        });
        return;
    }

    const auto& matrices = m_boneProducer->step(deltaTime);
    upload(matrices);
}

// 标题栏实时显示摄像机状态（lookAt 三参数 + 欧拉角 + FOV，可直接粘回代码调参）
void PBRDemo::updateTitle(GLFWwindow* win, float deltaTime) {
    static float titleTimer = 0.0f;
    titleTimer += deltaTime;
    if (titleTimer < 0.1f) return;
    titleTimer = 0.0f;
    auto cam = m_scene ? m_scene->getActiveCamera() : nullptr;
    if (!cam) return;
    glm::mat4 view = cam->getViewMatrix();
    glm::vec3 eye = cam->getPosition();
    glm::vec3 fwd = -glm::normalize(glm::vec3(view[0][2], view[1][2], view[2][2]));
    glm::vec3 up  =  glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));
    glm::vec3 right = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
    glm::vec3 center = eye + fwd;
    float yaw   = glm::degrees(std::atan2(fwd.z, fwd.x));
    float pitch = glm::degrees(std::asin(glm::clamp(fwd.y, -1.0f, 1.0f)));
    float roll  = glm::degrees(std::asin(glm::clamp(glm::dot(right, glm::vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f)));
    float fovDeg = 60.0f;
    if (auto pc = std::dynamic_pointer_cast<Scene::PerspectiveCamera>(cam)) fovDeg = glm::degrees(pc->getFov());
    char title[256];
    std::snprintf(title, sizeof(title),
        "StarryEngine PBR | lookAt((%.2f,%.2f,%.2f),(%.2f,%.2f,%.2f),(%.2f,%.2f,%.2f)) | yaw=%.1f pitch=%.1f roll=%.1f | fov=%.0f",
        eye.x, eye.y, eye.z, center.x, center.y, center.z, up.x, up.y, up.z,
        yaw, pitch, roll, fovDeg);
    glfwSetWindowTitle(win, title);
}

void PBRDemo::captureFrame() {
    if (!m_frameCapture) return;
    auto graph = m_renderPath ? m_renderPath->getRenderGraph() : nullptr;
    if (!graph) return;

    auto texId = graph->getTextureId("SceneColor");
    auto handle = graph->getPhysicalTextureHandle(texId);
    auto* tex = m_rhi->getResourceManager()->getTexture(handle);
    m_frameCapture->capture(tex);

    if (m_frameExit && m_frameExitTarget != UINT32_MAX &&
        m_frameCapture->capturedCount() >= m_frameExitTarget) {
        m_frameExit = false;   
        if (m_requestExit) m_requestExit();
    }
}
