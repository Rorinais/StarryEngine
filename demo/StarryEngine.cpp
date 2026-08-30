#include <renderer/passes/PassWrapper.hpp>
#include <renderer/passes/GraphicsPass.hpp>
#include <renderer/passes/MeshPass.hpp>
#include <renderer/passes/ParticlePass.hpp>
#include <renderer/passes/ShadowPass.hpp>
#include <renderer/passes/ComputePass.hpp>
#include <renderer/RasterRenderer.hpp>
#include "RayTracingDemo.hpp"
#include <scene/ParticleEmitter.hpp>
#include <renderer/renderPaths/DeferredRenderPath.hpp>
#include <renderer/passExecutor/SceneDrawExecutor.hpp>
#include <assets/geometry/GeometryGenerator.hpp>

#include <application/Application.hpp>
#include <core/JobSystem.hpp>
#include <event/Events.hpp>
#include <renderer/interface/vulkan/VulkanFrameContext.hpp>
#include <renderer/utils/FrameCapture.hpp>
#include "AsyncBoneProducer.hpp"

#include <stb_image_write.h>
#include <glm/gtc/packing.hpp>
#include <limits>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static bool g_clickThrough = true;

namespace {
    constexpr uint32_t kWinW = 1280;
    constexpr uint32_t kWinH = 720;

    float computeModelFootY(const std::shared_ptr<StarryEngine::Assets::Geometry>& geometry) {
        if (!geometry) return 0.0f;
        const auto& layout = geometry->getVertexLayout();
        const auto& verts = geometry->getVertices();

        uint32_t posOffset = 0, binding = 0;
        bool found = false;
        for (const auto& a : layout.getAppAttributes()) {
            if (a.semantic == StarryEngine::Assets::VertexSemantic::Position) {
                posOffset = a.offset;
                binding = a.binding;
                found = true;
                break;
            }
        }
        if (!found) return 0.0f;

        uint32_t stride = layout.getBindingStride(binding);
        if (stride == 0) return 0.0f;

        const uint32_t strideF = stride / sizeof(float);
        const uint32_t posF = posOffset / sizeof(float) + 1;  // +1 = 位置 Y 分量
        float footY = std::numeric_limits<float>::max();
        for (size_t i = 0; i + strideF <= verts.size(); i += strideF)
            footY = std::min(footY, verts[i + posF]);
        return (footY == std::numeric_limits<float>::max()) ? 0.0f : footY;
    }
}

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

        m_renderer = std::make_shared<RasterRenderer>(m_rhi, m_descriptorPool, m_scene);
        m_renderer->createGlobalSetLayout();
        m_renderer->createGlobalUniformBuffer();
        m_renderer->initDefaultMaterials();

        m_descriptorSetLayout = m_renderer->getGlobalSetLayout();
        m_descriptorSet = m_renderer->getGlobalDescriptorSet(0);

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

        if (const char* dumpDir = std::getenv("STARRY_FRAME_DUMP")) {
            const char* cnt = std::getenv("STARRY_FRAME_COUNT");
            const char* idx = std::getenv("STARRY_FRAME_INDEX");
            uint32_t n = cnt ? static_cast<uint32_t>(std::atoi(cnt)) : 1u;
            uint32_t fi = idx ? static_cast<uint32_t>(std::atoi(idx)) : UINT32_MAX;
            m_frameExit = std::getenv("STARRY_FRAME_EXIT") != nullptr;
            m_frameExitTarget = (fi != UINT32_MAX) ? 1u : (n > 0 ? n : UINT32_MAX);
            m_frameCapture = std::make_shared<FrameCapture>(m_rhi);
            if (!m_frameCapture->initialize({ dumpDir, n, fi })) {
                m_frameCapture.reset();
            }
        }
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
        material->addTextureDependency("ShadowMap", 2, 6);

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

        {        
            auto skyboxEffect = std::make_shared<Scene::ProceduralEffect>();
            skyboxEffect->material = createSkyboxMaterial();
            m_scene->addProceduralEffect(skyboxEffect);

            auto mat = createTexturedPbrMaterial();
            auto sphere = std::make_shared<Scene::RenderObject>();
            sphere->geometry = Assets::GeometryGenerator::createSphere(m_rhi->getResourceManager(), 1.0f);
            sphere->materials = { mat };
            sphere->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.5f, 0.0f));

            {
                auto clip = std::make_shared<Assets::AnimationClip>();
                clip->name = "SpinBob";
                clip->duration = 4.0f;   
                clip->looping = true;
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
            ground->geometry = Assets::GeometryGenerator::createCube(m_rhi->getResourceManager(), 20.0f, 0.3f, 20.0f);
            ground->materials = { groundMat };
            ground->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.15f, 0.0f));

            m_scene->addObject(ground);
        }

        addGriseoModel();

        {
            for (auto& mat : m_skinnedMaterials) {
                if (!mat) continue;
                mat->setSubpassTag("StencilWrite");
                RHI::StencilOpState write;
                write.failOp = RHI::StencilOp::Keep;
                write.passOp = RHI::StencilOp::Replace; 
                write.depthFailOp = RHI::StencilOp::Keep;
                write.compareOp = RHI::CompareOp::Always;
                write.compareMask = 0xFF;
                write.writeMask = 0xFF;
                write.reference = 1;
                mat->setStencilTest(true);
                mat->setStencilOps(write, write);
            }

            bool outlineOn = (std::getenv("STARRY_NO_OUTLINE") == nullptr);
            m_outlineMaterial = outlineOn ? makeOutlineMaterial() : nullptr;
            if (m_outlineMaterial && m_modelGeometry) {
                auto outline = std::make_shared<Scene::RenderObject>();
                outline->geometry = m_modelGeometry;

                std::vector<std::shared_ptr<Assets::MaterialInstance>> outlineMats(m_modelMaterialCount, m_outlineMaterial);
                outline->materials = std::move(outlineMats);

                float footY = computeModelFootY(m_modelGeometry);
                LOG_INFO("[demo] 描边缩放中心（脚底）Y = {:.3f}", footY);
                outline->transform =
                    glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, footY, 0.0f))
                    * glm::scale(glm::mat4(1.0f), glm::vec3(1.03f))
                    * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -footY, 0.0f));
                m_scene->addObject(outline);
            }
        }

        {
            auto fire = std::make_shared<Scene::ParticleEmitter>();
            fire->name = "Fire";
            fire->particleCount = 1024;
            fire->computeShader = "assets/shaders/test/particle.comp";
            fire->material = makeParticleMaterial();
            fire->params.gravity  = -0.15f;
            fire->params.speedMin = 0.5f;
            fire->params.speedMax = 2.0f;
            fire->params.lifetime = 3.5f;
            fire->params.spreadXZ = 1.2f;
            fire->params.swayFreq = 2.7f;
            fire->params.swayAmp  = 0.6f;
            fire->params.emitterY = 0.0f;
            fire->params.topDiffuse   = 1.5f;
            fire->params.topThreshold = 2.5f;
            fire->params.colorYoung[0] = 1.0f; fire->params.colorYoung[1] = 0.9f; fire->params.colorYoung[2] = 0.2f;
            fire->params.colorMiddle[0]= 1.0f; fire->params.colorMiddle[1]= 0.4f; fire->params.colorMiddle[2]= 0.05f;
            fire->params.colorOld[0]   = 0.6f; fire->params.colorOld[1]   = 0.1f; fire->params.colorOld[2]   = 0.02f;
            fire->params.pointSizeMin  = 3.0f;
            fire->params.pointSizeMax  = 12.0f;
            m_scene->addParticleEmitter(fire);

            auto sparks = std::make_shared<Scene::ParticleEmitter>();
            sparks->name = "Sparks";
            sparks->particleCount = 256;
            sparks->computeShader = "assets/shaders/test/particle.comp";
            sparks->material = makeParticleMaterial();
            sparks->params.gravity  = -0.05f;
            sparks->params.speedMin = 0.3f;
            sparks->params.speedMax = 1.5f;
            sparks->params.lifetime = 2.0f;
            sparks->params.spreadXZ = 0.5f;
            sparks->params.swayFreq = 3.5f;
            sparks->params.swayAmp  = 0.4f;
            sparks->params.emitterY = 2.5f;
            sparks->params.topDiffuse   = 1.0f;
            sparks->params.topThreshold = 2.0f;
            sparks->params.colorYoung[0] = 0.0f; sparks->params.colorYoung[1] = 1.0f; sparks->params.colorYoung[2] = 0.0f;
            sparks->params.colorMiddle[0]= 0.0f; sparks->params.colorMiddle[1]= 0.8f; sparks->params.colorMiddle[2]= 0.0f;
            sparks->params.colorOld[0]   = 0.0f; sparks->params.colorOld[1]   = 0.4f; sparks->params.colorOld[2]   = 0.0f;
            sparks->params.pointSizeMin  = 10.0f;
            sparks->params.pointSizeMax  = 24.0f;
            m_scene->addParticleEmitter(sparks);
        }

        auto perspectiveCamera = std::make_shared<Scene::PerspectiveCamera>();
        perspectiveCamera->setPerspective(glm::radians(45.0f), (float)m_width / m_height, 0.1f, 100.0f);
        perspectiveCamera->lookAt(glm::vec3(0.0f, 2.0f, 7.0f), glm::vec3(0.0f, 0.8f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_scene->addCamera(perspectiveCamera);
        m_scene->setActiveCamera(perspectiveCamera);

        if (m_boneProducer && m_renderer && m_renderer->getJobSystem()) {
            const auto& m0 = m_boneProducer->getFrame0Matrices();
            if (!m0.empty()) {
                size_t bytes = m0.size() * sizeof(glm::mat4);
                for (auto& mat : m_skinnedMaterials) if (mat) mat->setStorageBuffer(1, 2, m0.data(), bytes);
                if (m_outlineMaterial) m_outlineMaterial->setStorageBuffer(1, 2, m0.data(), bytes);
            }
        }
    }

    std::shared_ptr<Assets::MaterialInstance> makeParticleMaterial() {
        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(
            m_rhi->getResourceManager(), m_descriptorSetLayout);
        if (!tmpl->loadShaders("assets/shaders/test/particle.vert", "assets/shaders/test/particle.frag")) {
            LOG_ERROR("Failed to load particle shaders");
            return nullptr;
        }
        auto mat = std::make_shared<Assets::MaterialInstance>(
            tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);
        RHI::BlendAttachmentState blend;
        blend.blendEnable = true;   
        blend.dstAlphaBlendFactor = RHI::BlendFactor::OneMinusSrcAlpha; 
        mat->setAttachments({ blend });
        mat->setDepthTest(false);
        mat->setDepthWrite(false);
        return mat;
    }

    std::shared_ptr<Assets::MaterialInstance> makeStencilQuadMaterial(const std::string& fragPath) {
        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(m_rhi->getResourceManager(), m_descriptorSetLayout);
        if (!tmpl->loadShaders("assets/shaders/core/shader.vert", fragPath)) {
            LOG_ERROR("Failed to load stencil quad shaders: {}", fragPath);
            return nullptr;
        }
        auto mat = std::make_shared<Assets::MaterialInstance>(
            tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);
        Assets::TextureLoader loader(m_rhi->getResourceManager());
        static const uint8_t kWhite[4] = { 255, 255, 255, 255 };
        auto white = loader.loadTextureFromMemory(kWhite, 1, 1, RHI::Format::RGBA8_UNorm, "White");
        if (white.texture.isValid())
            mat->setTexture("texSampler", white.texture, white.sampler);
        mat->setDepthTest(false);
        mat->setDepthWrite(false);
        return mat;
    }

    std::shared_ptr<RasterRenderer> getRenderer() { return m_renderer; }

    std::shared_ptr< Scene::Scene> getScene() { return m_scene; }

    void onUpdate(float deltaTime);
    void captureFrame();

    void setExitCallback(std::function<void()> cb) { m_requestExit = std::move(cb); }
private:
    bool loadModelGeometry(Assets::Geometry& outGeometry,std::vector<Assets::MaterialParams>& outParams,Assets::Skeleton* outSkeleton = nullptr);
    std::shared_ptr<Assets::MaterialInstance> makeModelMaterial(const Assets::MaterialParams& param, bool skinned);
    std::shared_ptr<Assets::MaterialInstance> makeOutlineMaterial();
    void addGriseoModel();

    Assets::Skeleton m_modelSkeleton;
    Assets::AnimationClip m_modelClip;
    std::string m_modelTextureDir = "fuxuan";  

    std::unique_ptr<AsyncBoneProducer> m_boneProducer;
    bool m_skeletalReady = false;
    float m_lastDelta = 0.0f;   
    std::vector<std::shared_ptr<Assets::MaterialInstance>> m_skinnedMaterials;
    std::shared_ptr<Assets::Geometry> m_modelGeometry;        
    size_t m_modelMaterialCount = 0;                            
    std::shared_ptr<Assets::MaterialInstance> m_outlineMaterial; 

    uint32_t m_width, m_height;
    std::shared_ptr<RHI::IRHI> m_rhi;
    std::shared_ptr<RasterRenderer> m_renderer;
    std::shared_ptr< Scene::Scene> m_scene;

    std::shared_ptr<DeferredRenderPath> m_renderPath;
    std::shared_ptr<FrameCapture> m_frameCapture;
    bool m_frameExit = false;       
    uint32_t m_frameExitTarget = 0;   
    std::function<void()> m_requestExit;   

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
    if (!loader.open("assets/models/fuxuan_Animation3.fbx")) {
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

std::shared_ptr<Assets::MaterialInstance> PBRDemo::makeOutlineMaterial() {
    auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(
        m_rhi->getResourceManager(), m_descriptorSetLayout);
    if (!tmpl->loadShaders("assets/shaders/core/shader_skinned.vert", "assets/shaders/core/outline.frag")) {
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

    mat->setSubpassTag("StencilTest");
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

    if (const char* ec = std::getenv("STARRY_EXTRA_CHARS")) {
        uint32_t n = std::strtoul(ec, nullptr, 10);
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
        cfg.enableThread = !poolActive && (std::getenv("STARRY_ASYNC_BONES") != nullptr);
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
        m_frameExit = false;   // 只触发一次
        if (m_requestExit) m_requestExit();
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
    StarryEngine::Logger::setLevel("info");

    StarryEngine::Application::Config cfg;
    cfg.width = kWinW;
    cfg.height = kWinH;

    if (const char* w = std::getenv("STARRY_WIN_W")) cfg.width = std::strtoul(w, nullptr, 10);
    if (const char* h = std::getenv("STARRY_WIN_H")) cfg.height = std::strtoul(h, nullptr, 10);
    cfg.title = "Transparent Window Smoke Test";
    //cfg.resizable = false;
    //cfg.transparent = false;
    //cfg.borderless = true;
    //cfg.alwaysOnTop = true;

    //cfg.clickThrough = (std::getenv("STARRY_NO_CLICKTHROUGH") == nullptr);
    //cfg.nativeWayland = (std::getenv("STARRY_FORCE_X11") == nullptr);
    //LOG_INFO("[demo] 平台: {}（STARRY_FORCE_X11 强制 X11）", cfg.nativeWayland ? "原生 Wayland" : "X11");
    StarryEngine::Application app(cfg);

    // demo 选择：STARRY_RT / STARRY_OFFLINE → 软光追 demo（RayTracingRenderer + RayTracingRenderPath）；
    // 否则 PBR demo（RasterRenderer + DeferredRenderPath）。渲染器按技术分化（IRenderer/BaseRenderer 共享）。
    const bool rtMode = std::getenv("STARRY_RT") != nullptr || std::getenv("STARRY_OFFLINE") != nullptr;

    const char* autoQuitEnv = std::getenv("STARRY_AUTO_QUIT");
    uint64_t autoQuitFrames = autoQuitEnv ? std::strtoull(autoQuitEnv, nullptr, 10) : 0;
    uint64_t frameCounter = 0;
    auto perfLast = std::chrono::steady_clock::now();
    uint64_t perfLastFrames = 0;

    auto perfLog = [&app, &perfLast, &perfLastFrames]() {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - perfLast).count() < 5.0) return;
        double wall = std::chrono::duration<double>(now - perfLast).count();
        perfLast = now;
        auto fc = app.getRenderHardwareInterface()->getFrameContext();
        if (!fc) return;
        const auto& s = fc->getStatistics();
        uint64_t frames = s.totalFrames - perfLastFrames;
        perfLastFrames = s.totalFrames;
        LOG_INFO("[perf] 窗内帧={} 实际FPS={:.1f} | 平均帧={:.2f}ms 最大帧={:.2f}ms | CPU均={:.2f}ms GPU均={:.2f}ms",
                 frames, (wall > 0.0) ? static_cast<double>(frames) / wall : 0.0,
                 s.averageFrameTime, s.maxFrameTime,
                 s.averageCPUTime, s.averageGPUTime);
    };

    if (rtMode) {
        auto demo = std::make_shared<RayTracingDemo>(app.getRenderHardwareInterface(), app.getGlobalDescriptorPool(), app.getWidth(), app.getHeight());
        demo->setExitCallback([&app]() { app.shutdown(); });
        app.setRenderer(demo->getRenderer());
        app.setScene(demo->getScene());
        app.setPostRenderCallback([demo, &app, &frameCounter, autoQuitFrames]() {
            demo->offlineCapture();   // 离线读回（交互 RT 下为空操作）
            if (std::getenv("STARRY_FRAME_DUMP")) { auto rp = demo->getRenderer()->getRenderPath(); }
            if (autoQuitFrames > 0 && ++frameCounter >= autoQuitFrames) app.shutdown();
        });
        app.setUpdateCallback([demo, &app, &perfLog](float deltaTime) {
            demo->updateTitle(app.getWindow()->getHandle(), deltaTime);
            perfLog();
        });
    } else {
        auto demo = std::make_shared<PBRDemo>(app.getRenderHardwareInterface(), app.getGlobalDescriptorPool(), app.getWidth(), app.getHeight());
        demo->setExitCallback([&app]() { app.shutdown(); });
        app.setRenderer(demo->getRenderer());
        app.setScene(demo->getScene());
        app.setPostRenderCallback([demo, &app, &frameCounter, autoQuitFrames]() {
            demo->captureFrame();
            if (autoQuitFrames > 0 && ++frameCounter >= autoQuitFrames) app.shutdown();
        });
        app.setUpdateCallback([demo, &app, &perfLog](float deltaTime) {
            demo->onUpdate(deltaTime);
            perfLog();
        });
    }
    app.initEventDispatcher();

    GetEventDispatcher().subscribe(EventType::KeyPressed, [&app](IEvent& e) {
        auto& ev = static_cast<KeyEvent&>(e);
        if (ev.getKey() == GLFW_KEY_F1 && ev.getAction() == GLFW_PRESS) {
            g_clickThrough = !g_clickThrough;
            if (app.getWindow()->setClickThrough(g_clickThrough)) {
                LOG_INFO("[demo] 点击穿透: {}", g_clickThrough ? "ON（透明区不挡点击）" : "OFF（可交互）");
            } else {
                LOG_WARN("[demo] 当前平台切换点击穿透失败");
            }
        }
    });

    app.run();
    StarryEngine::Logger::shutdown();
}