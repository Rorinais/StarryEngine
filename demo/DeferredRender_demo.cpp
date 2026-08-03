#include "../src/renderer/passes/PassWrapper.hpp"
#include "../src/renderer/passes/GraphicsPass.hpp"
#include "../src/renderer/passes/ParticleSystemPass.hpp"
#include "../src/renderer/renderPaths/DeferredRenderPath.hpp"
#include "../src/renderer/passExecutor/MeshDrawExecutor.hpp"
#include "../src/renderer/passExecutor/SkyboxExecutor.hpp"
#include "../src/assets/geometry/GeometryGenerator.hpp"

#include"../src/application/Application.hpp"

using namespace StarryEngine;

class PBRDemo {
public:
    PBRDemo(std::shared_ptr<RHI::IRHI> rhi, RHI::DescriptorPoolHandle descriptorPool, uint32_t width, uint32_t height):
    m_rhi(rhi),m_descriptorPool(descriptorPool),m_width(width),m_height(height){
        m_iblBuilder = std::make_shared<Assets::IBLBuilder>(rhi->getResourceManager(), rhi);
        createRenderer();
        createScene();
    }

    void createRenderer() {
        m_scene = std::make_shared<Scene::Scene>();

        m_renderer = std::make_shared<Renderer>(m_rhi, m_descriptorPool, m_scene);
        m_renderer->createGlobalSetLayout();
        m_renderer->createGlobalUniformBuffer();
        m_renderer->initDefaultMaterials();

        m_descriptorSetLayout = m_renderer->getGlobalSetLayout();
        m_descriptorSet = m_renderer->getGlobalDescriptorSet();

        auto renderPath = std::make_shared<DeferredRenderPath>(m_rhi, m_width, m_height);

        auto colorDesc = PassWrapper::createColorTextureDesc({m_width, m_height, 1}, RHI::Format::RGBA16_Float);
        auto depthDesc = PassWrapper::createDepthTextureDesc({m_width, m_height, 1}, RHI::Format::D32_Float);
        auto swapDesc   = PassWrapper::createColorTextureDesc({m_width, m_height, 1}, RHI::Format::BGRA8_sRGB);
        renderPath->addTextureDesc("SceneColor",  colorDesc);
        renderPath->addTextureDesc("Depth",       depthDesc);
        renderPath->addTextureDesc("Swapchain",   swapDesc);

        {
            PassList passes;

            // ForwardPass: OpaqueGeometry
            auto forwardPass = std::make_shared<GraphicsPass>("ForwardPass");
            {
                SubpassDesc sp;
                sp.name = "OpaqueGeometry"; sp.tag = "Forward_Opaque";
                sp.executor = std::make_shared<MeshDrawExecutor>();

                RenderGraph::AttachmentParams color;
                color.initialLayout = RHI::ImageLayout::Undefined;
                color.finalLayout   = RHI::ImageLayout::ShaderReadOnly;
                color.loadOp = RHI::AttachmentLoadOp::Clear;
                color.storeOp = RHI::AttachmentStoreOp::Store;
                sp.colorAttachments.push_back({"SceneColor", color});

                RenderGraph::AttachmentParams depth;
                depth.initialLayout = RHI::ImageLayout::Undefined;
                depth.finalLayout   = RHI::ImageLayout::DepthStencilAttachment;
                depth.loadOp = RHI::AttachmentLoadOp::Clear;
                depth.storeOp = RHI::AttachmentStoreOp::Store;
                sp.depthAttachment = {"Depth", depth};

                forwardPass->addSubpass(sp);
            }
            passes.push_back(forwardPass);

            // PostProcessPass: Skybox + Grid
            auto postPass = std::make_shared<GraphicsPass>("PostProcessPass");
            {
                SubpassDesc sky;
                sky.name = "Skybox"; sky.tag = "PostProcess_Skybox";
                sky.executor = std::make_shared<SkyboxExecutor>();
                RenderGraph::AttachmentParams colorLoad;
                colorLoad.initialLayout = RHI::ImageLayout::ShaderReadOnly;
                colorLoad.finalLayout   = RHI::ImageLayout::ShaderReadOnly;
                colorLoad.loadOp = RHI::AttachmentLoadOp::Load;
                colorLoad.storeOp = RHI::AttachmentStoreOp::Store;
                sky.colorAttachments.push_back({"SceneColor", colorLoad});

                RenderGraph::AttachmentParams depthLoad;
                depthLoad.initialLayout = RHI::ImageLayout::DepthStencilAttachment;
                depthLoad.finalLayout   = RHI::ImageLayout::DepthStencilAttachment;
                depthLoad.loadOp = RHI::AttachmentLoadOp::Load;
                depthLoad.storeOp = RHI::AttachmentStoreOp::DontCare;
                sky.depthAttachment = {"Depth", depthLoad};
                postPass->addSubpass(sky);

                SubpassDesc grid;
                grid.name = "Grid"; grid.tag = "PostProcess_Grid";
                grid.executor = std::make_shared<MeshDrawExecutor>();
                grid.colorAttachments.push_back({"SceneColor", colorLoad});
                grid.depthAttachment = {"Depth", depthLoad};
                postPass->addSubpass(grid);
            }
            passes.push_back(postPass);

            {
                ParticleSystemDesc desc;
                desc.name            = "Fire";
                desc.particleCount   = 1024;
                desc.perParticleFloats = 4;
                desc.computeShader   = "assets/shaders/test/particle.comp";
                desc.vertexShader    = "assets/shaders/test/particle.vert";
                desc.fragmentShader  = "assets/shaders/test/particle.frag";
                desc.params.gravity       = -0.15f;
                desc.params.speedMin      = 0.5f;
                desc.params.speedMax      = 2.0f;
                desc.params.lifetime      = 3.5f;
                desc.params.spreadXZ      = 1.2f;
                desc.params.swayFreq      = 2.7f;
                desc.params.swayAmp       = 0.6f;
                desc.params.emitterY      = 0.0f;
                desc.params.topDiffuse    = 1.5f;
                desc.params.topThreshold  = 2.5f;
                desc.params.colorYoung[0] = 1.0f; desc.params.colorYoung[1] = 0.9f; desc.params.colorYoung[2] = 0.2f;
                desc.params.colorMiddle[0]= 1.0f; desc.params.colorMiddle[1]= 0.4f; desc.params.colorMiddle[2]= 0.05f;
                desc.params.colorOld[0]   = 0.6f; desc.params.colorOld[1]   = 0.1f; desc.params.colorOld[2]   = 0.02f;
                desc.params.pointSizeMin  = 3.0f;
                desc.params.pointSizeMax  = 12.0f;
                passes.push_back(std::make_shared<ParticleSystemPass>(desc));
            }

            {
                ParticleSystemDesc desc;
                desc.name            = "Sparks";
                desc.particleCount   = 256;
                desc.perParticleFloats = 4;
                desc.computeShader   = "assets/shaders/test/particle.comp";
                desc.vertexShader    = "assets/shaders/test/particle.vert";
                desc.fragmentShader  = "assets/shaders/test/particle.frag";
                desc.params.gravity       = -0.05f;
                desc.params.speedMin      = 0.3f;
                desc.params.speedMax      = 1.5f;
                desc.params.lifetime      = 2.0f;
                desc.params.spreadXZ      = 0.5f;
                desc.params.swayFreq      = 3.5f;
                desc.params.swayAmp       = 0.4f;
                desc.params.emitterY      = 2.5f;
                desc.params.topDiffuse    = 1.0f;
                desc.params.topThreshold  = 2.0f;
                desc.params.colorYoung[0] = 0.0f; desc.params.colorYoung[1] = 1.0f;  // 纯绿，排除颜色问题
                desc.params.colorYoung[2] = 0.0f;
                desc.params.colorMiddle[0]= 0.0f; desc.params.colorMiddle[1]= 0.8f;
                desc.params.colorMiddle[2]= 0.0f;
                desc.params.colorOld[0]   = 0.0f; desc.params.colorOld[1]   = 0.4f;
                desc.params.colorOld[2]   = 0.0f;
                desc.params.pointSizeMin  = 10.0f;   // 巨大点
                desc.params.pointSizeMax  = 24.0f;
                passes.push_back(std::make_shared<ParticleSystemPass>(desc));
            }

            renderPath->setPassList(std::move(passes));
        }

        m_renderer->setRenderPath(std::move(renderPath));
    }

    void initIBL() {
        m_envCubemap      = m_iblBuilder->buildEnvCubemap("assets/textures/pbr/kloofendal_48d_partly_cloudy_puresky_1k.hdr", 128);
        m_irradianceMap   = m_iblBuilder->generateIrradianceMapCS(m_envCubemap, 64);
        m_prefilteredMap  = m_iblBuilder->generatePrefilteredMapCS(m_envCubemap, 256, 6);
        m_brdfLut         = m_iblBuilder->generateBrdfLutCS(128);

        RHI::SamplerDesc cubeSampDesc;
        cubeSampDesc.minFilter = RHI::SamplerFilter::Linear;
        cubeSampDesc.magFilter = RHI::SamplerFilter::Linear;
        cubeSampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        cubeSampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        cubeSampDesc.addressW = RHI::SamplerAddressMode::ClampToEdge;
        cubeSampDesc.maxLod = 1.0f;
        m_cubeSampler = m_rhi->getResourceManager()->createSampler(cubeSampDesc);

        RHI::SamplerDesc prefilterSampDesc = cubeSampDesc;
        prefilterSampDesc.mipFilter = RHI::SamplerFilter::Linear;
        prefilterSampDesc.maxLod = 6.0f;
        m_prefilterSampler = m_rhi->getResourceManager()->createSampler(prefilterSampDesc);

        RHI::SamplerDesc lutSampDesc;
        lutSampDesc.minFilter = RHI::SamplerFilter::Linear;
        lutSampDesc.magFilter = RHI::SamplerFilter::Linear;
        lutSampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        lutSampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        lutSampDesc.maxLod = 1.0f;
        m_lutSampler = m_rhi->getResourceManager()->createSampler(lutSampDesc);
    }

    std::shared_ptr<Assets::MaterialInstance> createSkyboxMaterial() {

        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(m_rhi->getResourceManager(), m_descriptorSetLayout);
        tmpl->loadShaders("assets/shaders/deferred/skybox.vert", "assets/shaders/deferred/skybox.frag");

        auto material = std::make_shared<Assets::MaterialInstance>(tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);

        material->setTexture("uSkybox", m_envCubemap, m_cubeSampler);
        material->setSubpassTag("PostProcess_Skybox");
        material->enableDepthTest(true);
        material->setDepthCompareOp(RHI::CompareOp::LessOrEqual);

        return material;
    }

    std::shared_ptr<Assets::MaterialInstance> createGridMaterial() {
        auto gridTmpl = std::make_shared<Assets::DefaultMaterialTemplate>(m_rhi->getResourceManager(), m_descriptorSetLayout);
        gridTmpl->loadShaders("assets/shaders/core/gridShader.vert", "assets/shaders/core/gridShader.frag");

        auto gridMaterialInst = std::make_shared<Assets::MaterialInstance>(gridTmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);
        gridMaterialInst->enableDepthTest(true);
        gridMaterialInst->setSubpassTag("PostProcess_Grid");

        return gridMaterialInst;
    }


    std::shared_ptr<Assets::MaterialInstance> createTexturedPbrMaterial() {
        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(m_rhi->getResourceManager(), m_descriptorSetLayout);
        tmpl->loadShaders("assets/shaders/pbr/shpere_pbr.vert", "assets/shaders/pbr/shpere_pbr (2).frag");

        auto material = std::make_shared<Assets::MaterialInstance>(tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);
        material->setSubpassTag("Forward_Opaque");
        material->enableDepthTest(true);
        material->enableDepthWrite(true);

        Assets::TextureLoader texLoader(m_rhi->getResourceManager());
        auto armT    = texLoader.loadTexture2D("assets/textures/pbr/metal_plate_02_arm_1k.png", RHI::Format::RGBA8_UNorm, "ARM");
        auto albedoT = texLoader.loadTexture2D("assets/textures/pbr/metal_plate_02_diff_1k.png", RHI::Format::RGBA8_sRGB, "Albedo");
        auto normalT = texLoader.loadTexture2D("assets/textures/pbr/metal_plate_02_nor_dx_1k.png", RHI::Format::RGBA8_UNorm, "Normal");

        material->setTexture("armMap", armT.texture, armT.sampler);
        material->setTexture("albedoMap", albedoT.texture, albedoT.sampler);
        material->setTexture("normalMap", normalT.texture, normalT.sampler);
        material->setTexture("uIrradianceMap", m_irradianceMap, m_cubeSampler);
        material->setTexture("uPrefilteredMap", m_prefilteredMap, m_prefilterSampler);
        material->setTexture("uBrdfLut", m_brdfLut, m_lutSampler);

        auto* lightBlock = material->getBlock("LightingUBO");
        if (lightBlock) {
            lightBlock->setVec4("lights.position", glm::vec4(-0.5f, -1.0f, -0.8f, 0.0f));  
            lightBlock->setVec4("lights.color", glm::vec4(3.0f, 2.7f, 2.3f, 1.0f));
            lightBlock->setFloat("lightCount", 1.0f);
            lightBlock->setFloat("ambientStrength", 0.15f);
        }
        material->applyAllDirtyBlocks();
        return material;
    }

    
    void createScene() {
        initIBL();

        auto skyboxEffect = std::make_shared<Scene::ProceduralEffect>();
        skyboxEffect->material = createSkyboxMaterial();
        m_scene->addProceduralEffect(skyboxEffect);

        auto mat = createTexturedPbrMaterial();
        auto sphere = std::make_shared<Scene::RenderObject>();
        sphere->geometry = Assets::GeometryGenerator::createSphere(m_rhi->getResourceManager(), 1.0f);
        sphere->materials = { mat };
        sphere->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.5f, 0.0f));

        // ── 变换动画：球体自转 + 上下浮动 ──
        {
            auto clip = std::make_shared<Assets::AnimationClip>();
            clip->name = "SpinBob";
            clip->duration = 4.0f;    // 4 秒一圈
            clip->looping = true;
            // 每 0.5s 一个关键帧：绕 Y 轴旋转 45°，并上下浮动
            for (int i = 0; i <= 8; ++i) {
                float t = i * 0.5f;
                Assets::TransformKeyframe kf;
                kf.time = t;
                kf.position = glm::vec3(0.0f, 1.5f + 0.3f * std::sin(t * glm::pi<float>() / 2.0f), 0.0f);
                kf.rotation = glm::angleAxis(t * glm::pi<float>() / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
                kf.scale = glm::vec3(1.0f);
                clip->keyframes.push_back(kf);
            }

            auto animator = std::make_shared<Scene::Animator>();
            animator->setClip(clip);
            animator->setSpeed(1.0f);
            sphere->animator = animator;
        }

        m_scene->addObject(sphere);


        auto groundMat = createTexturedPbrMaterial();
        auto ground = std::make_shared<Scene::RenderObject>();
        ground->geometry = Assets::GeometryGenerator::createCube(m_rhi->getResourceManager(), 20.0f, 20.0f, 0.3f);
        ground->materials = { groundMat };
        ground->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.15f, 0.0f));
        m_scene->addObject(ground);

        addGriseoModel();

        auto perspectiveCamera = std::make_shared<Scene::PerspectiveCamera>();
        perspectiveCamera->setPerspective(glm::radians(45.0f), (float)m_width / m_height, 0.1f, 100.0f);
        perspectiveCamera->lookAt(glm::vec3(1.0f, 2.0f, 5.0f),glm::vec3(0.0f, 1.5f, 0.0f),glm::vec3(0.0f, 1.0f, 0.0f));
        m_scene->addCamera(perspectiveCamera);
        m_scene->setActiveCamera(perspectiveCamera);
    }

    std::shared_ptr<Renderer> getRenderer() { return m_renderer; }

    std::shared_ptr< Scene::Scene> getScene() { return m_scene; }

    // 每帧骨骼动画更新：推进时间 → 采样 → 上传骨骼矩阵 SSBO
    void onUpdate(float deltaTime);
private:
    bool loadModelGeometry(Assets::Geometry& outGeometry,std::vector<Assets::MaterialParams>& outParams,Assets::Skeleton* outSkeleton = nullptr);
    std::shared_ptr<Assets::MaterialInstance> makeModelMaterial(const Assets::MaterialParams& param, bool skinned);
    void addGriseoModel();

    Assets::Skeleton m_modelSkeleton;
    Assets::AnimationClip m_modelClip;
    std::string m_modelTextureDir = "fuxuan";  

    // 骨骼动画运行时状态
    Scene::Animator m_skeletalAnimator;
    float m_skeletalTime = 0.0f;
    bool m_skeletalReady = false;
    std::vector<std::shared_ptr<Assets::MaterialInstance>> m_skinnedMaterials;

    uint32_t m_width, m_height;
    std::shared_ptr<RHI::IRHI> m_rhi;
    std::shared_ptr<Renderer> m_renderer;
    std::shared_ptr< Scene::Scene> m_scene;

    // IBL
    RHI::TextureHandle m_envCubemap;
    RHI::TextureHandle m_irradianceMap;
    RHI::TextureHandle m_prefilteredMap;
    RHI::TextureHandle m_brdfLut;
    RHI::SamplerHandle m_cubeSampler;
    RHI::SamplerHandle m_prefilterSampler;
    RHI::SamplerHandle m_lutSampler;

    StarryEngine::RHI::DescriptorSetHandle m_descriptorSet;
    StarryEngine::RHI::DescriptorPoolHandle m_descriptorPool;
    StarryEngine::RHI::DescriptorSetLayoutHandle m_descriptorSetLayout;

    std::shared_ptr<Assets::IBLBuilder> m_iblBuilder;
};

bool PBRDemo::loadModelGeometry(Assets::Geometry& outGeometry,std::vector<Assets::MaterialParams>& outParams,Assets::Skeleton* outSkeleton) {
    Assets::ModelLoader loader(m_rhi->getResourceManager());
    if (!loader.open("assets/models/fuxuan_Animation.fbx")) {
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
    outParams = std::move(materials);
    outGeometry = std::move(*geometry);

    outGeometry.uploadToGPU();
    return true;
}

std::shared_ptr<Assets::MaterialInstance> PBRDemo::makeModelMaterial(
    const Assets::MaterialParams& param, bool skinned) {
    std::string fsPath;
    if (param.name == "face" || param.name == "脸" || param.name == "表情") fsPath = "assets/shaders/core/face.frag";
    else if (param.name == "hair" || param.name == "髪") fsPath = "assets/shaders/core/hair.frag";
    else fsPath = "assets/shaders/core/shader.frag";

    const char* vsPath = skinned ? "assets/shaders/core/shader_skinned.vert": "assets/shaders/core/shader.vert";

    auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(
        m_rhi->getResourceManager(), m_descriptorSetLayout);
    if (!tmpl->loadShaders(vsPath, fsPath)) {
        LOG_ERROR("Failed to load shaders for material: {}", param.name);
        return nullptr;
    }

    auto instance = std::make_shared<Assets::MaterialInstance>(
        tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);

    Assets::TextureLoader loader(m_rhi->getResourceManager());
    if (!param.albedoTexture.empty()) {
        std::string fileName = param.albedoTexture;
        auto slashPos = fileName.find_last_of("/\\");
        if (slashPos != std::string::npos) fileName = fileName.substr(slashPos + 1);
        std::string texPath = "assets/models/textures/" + m_modelTextureDir + "/" + fileName;
        auto texResult = loader.loadTexture2D(texPath, RHI::Format::RGBA8_UNorm);
        if (texResult.texture.isValid()) {
            instance->setTexture("texSampler", texResult.texture, texResult.sampler);
        }
        else {
            LOG_ERROR("Failed to load texture: {}", texPath);
        }
    }
    else {
        static const uint8_t kWhite[4] = { 255, 255, 255, 255 };
        auto white = loader.loadTextureFromMemory(kWhite, 1, 1, RHI::Format::RGBA8_UNorm, "White");
        if (white.texture.isValid())
            instance->setTexture("texSampler", white.texture, white.sampler);
    }

    instance->setSubpassTag("Forward_Opaque");
    instance->setDepthTest(true);
    instance->setDepthWrite(true);
    return instance;
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
    obj->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.5f));
    m_scene->addObject(obj);

    m_skeletalReady = skinned;
    if (m_skeletalReady) {
        m_skinnedMaterials = materials;
        m_skeletalAnimator.updateSkeleton(m_modelSkeleton, m_modelClip, m_skeletalTime);
    }
}

void PBRDemo::onUpdate(float deltaTime) {
    if (!m_skeletalReady || m_skinnedMaterials.empty()) return;

    // 动画时间是 ticks，deltaTime 是秒 → 乘 ticksPerSecond 换算
    float tps = (m_modelClip.ticksPerSecond > 0.0f) ? m_modelClip.ticksPerSecond : 25.0f;
    m_skeletalTime += deltaTime * tps;
    m_skeletalAnimator.updateSkeleton(m_modelSkeleton, m_modelClip, m_skeletalTime);
    const auto& matrices = m_skeletalAnimator.getBoneMatrices();
    if (matrices.empty()) return;

    size_t bytes = matrices.size() * sizeof(glm::mat4);
    for (auto& mat : m_skinnedMaterials) {
        if (mat) mat->setStorageBuffer(1, 2, matrices.data(), bytes);
    }
}


int main() {
#ifdef __linux__
    char exePath[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (count != -1) {
        exePath[count] = '\0';
        char* lastSlash = strrchr(exePath, '/');
        if (lastSlash) {
            *lastSlash = '\0';
            std::string layerPath = std::string(exePath) + "/layers";
            setenv("VK_LAYER_PATH", layerPath.c_str(), 1);
            std::string libPath = std::string(exePath);
            std::string currentLdPath = getenv("LD_LIBRARY_PATH") ? getenv("LD_LIBRARY_PATH") : "";
            setenv("LD_LIBRARY_PATH", (libPath + ":" + currentLdPath).c_str(), 1);
        }
    }
#elif _WIN32
    _putenv_s("VK_LAYER_PATH", "layers");
#endif
    StarryEngine::Logger::init();
    StarryEngine::Logger::setShowSourceLoc(true);
    StarryEngine::Logger::setLevel("warn"); 
    StarryEngine::Application app;

    auto demo = std::make_shared<PBRDemo>(app.getRenderHardwareInterface(), app.getGlobalDescriptorPool(), app.getWidth(), app.getHeight());

    app.setRenderer(demo->getRenderer());
    app.setScene(demo->getScene());
    app.setUpdateCallback([demo](float deltaTime) { demo->onUpdate(deltaTime); });
    app.initEventDispatcher();
    app.run();
    StarryEngine::Logger::shutdown();
}