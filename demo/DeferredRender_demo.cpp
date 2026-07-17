#include "../src/renderer/passes/PassWrapper.hpp"
#include "../src/renderer/passes/GeometrySubpass.hpp"
#include "../src/renderer/passes/LightSubpass.hpp"
#include "../src/renderer/subpassRecorder/SkyboxRecorder.hpp"
#include "../src/renderer/passes/RenderPathFactory.hpp"

#include"../src/application/Application.hpp"
#include "type.hpp"

using namespace StarryEngine;
ModelData createModel(std::shared_ptr<RHI::ResourceManager> resMgr, GlobalDescriptorData data);
ModelData createGrid(std::shared_ptr<RHI::ResourceManager> resMgr, GlobalDescriptorData data);
DataSet createRenderer(std::shared_ptr<RHI::IRHI> rhi, RHI::DescriptorPoolHandle descriptorPool, uint32_t width, uint32_t height);
std::shared_ptr<Assets::MaterialInstance> createCopyMaterial(std::shared_ptr<RHI::ResourceManager> resMgr,GlobalDescriptorData data);
//std::shared_ptr<Assets::MaterialInstance> createSkyboxMaterial(std::shared_ptr<RHI::ResourceManager> resMgr,GlobalDescriptorData data);
std::shared_ptr<Assets::MaterialInstance> createPbrMaterial(std::shared_ptr<RHI::ResourceManager> resMgr, GlobalDescriptorData data);


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

        auto renderPath = RenderPathFactory::createRenderPathFromJSON("assets/configs/forward_render_path.json", m_rhi, m_width, m_height);
        m_renderer->setRenderPath(std::move(renderPath));
    }

    std::shared_ptr<Assets::MaterialInstance> createSkyboxMaterial() {

        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(m_rhi->getResourceManager(), m_descriptorSetLayout);
        tmpl->loadShaders("assets/shaders/deferred/skybox.vert", "assets/shaders/deferred/skybox.frag");

        auto material = std::make_shared<Assets::MaterialInstance>(tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);

        auto cubemap = m_iblBuilder->buildEnvCubemap("assets/textures/pbr/kloofendal_48d_partly_cloudy_puresky_1k.hdr", 128);
        auto sampler = Assets::TextureLoader(m_rhi->getResourceManager()).createDefaultSampler();
        material->setTexture("uSkybox", cubemap, sampler);

        material->setSubpassTag("PostProcess_Skybox");
        material->enableDepthTest(true);
        material->setDepthCompareOp(RHI::CompareOp::LessOrEqual);

        return material;
    }

    void initIBL() {
        // 只创建一次，所有材质共用
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

    std::shared_ptr<Assets::MaterialInstance> createPbrMaterial(float roughness, float metallic) {
        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(m_rhi->getResourceManager(), m_descriptorSetLayout);
        tmpl->loadShaders("assets/shaders/pbr/shpere_pbr.vert", "assets/shaders/pbr/shpere_pbr.frag");

        auto material = std::make_shared<Assets::MaterialInstance>(tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);
        material->setSubpassTag("Forward_Opaque");
        material->enableDepthTest(true);
        material->enableDepthWrite(true);

        material->setTexture("uIrradianceMap", m_irradianceMap, m_cubeSampler);
        material->setTexture("uPrefilteredMap", m_prefilteredMap, m_prefilterSampler);
        material->setTexture("uBrdfLut", m_brdfLut, m_lutSampler);

        auto* lightBlock = material->getBlock("LightingUBO");
        if (lightBlock) {
            lightBlock->setVec4("lights.position", glm::vec4(0.2f, 0.0f, -1.0f, 0.0f));
            lightBlock->setVec4("lights.color", glm::vec4(0.9f, 0.1f, 0.5f, 1.0f));
            lightBlock->setFloat("lightCount", 1.0f);
            lightBlock->setFloat("ambientStrength", 0.1f);
            lightBlock->setFloat("roughness", roughness);
            lightBlock->setFloat("metallic", metallic);
        }

        material->applyAllDirtyBlocks();
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


    void createScene() {
        initIBL();  // 只生成一次 IBL 贴图

        auto skyboxEffect = std::make_shared<Scene::ProceduralEffect>();
        auto skyboxMaterial = createSkyboxMaterial();
        skyboxEffect->material = skyboxMaterial;
        m_scene->addProceduralEffect(skyboxEffect);

        // ── 6×6 粗糙度 × 金属度矩阵 ──
        const int GRID_SIZE = 6;
        const float SPACING = 2.2f;
        const float GRID_CENTER_X = (GRID_SIZE - 1) * SPACING * 0.5f;
        const float GRID_CENTER_Y = (GRID_SIZE - 1) * SPACING * 0.5f + 1.0f;

        for (int row = 0; row < GRID_SIZE; ++row) {
            for (int col = 0; col < GRID_SIZE; ++col) {
                float roughness = float(row) / float(GRID_SIZE - 1);
                float metallic  = float(col) / float(GRID_SIZE - 1);

                auto mat = createPbrMaterial(roughness, metallic);

                auto sphere = std::make_shared<Scene::RenderObject>();
                sphere->geometry = Assets::GeometryGenerator::createSphere(m_rhi->getResourceManager(), 1.0f);
                sphere->materials = { mat };

                float x = col * SPACING - GRID_CENTER_X;
                float y = GRID_CENTER_Y - row * SPACING;  // roughness 自上而下增大
                sphere->transform = glm::translate(glm::mat4(1.0f), glm::vec3(x, y+5.0f, -1.0f));
                m_scene->addObject(sphere);
            }
        }

        // 参考地平面
        auto gridObj = std::make_shared<Scene::RenderObject>();
        gridObj->geometry = Assets::GeometryGenerator::createGrid(m_rhi->getResourceManager());
        gridObj->materials = { createGridMaterial() };
        gridObj->transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, -5.0f, 0)), glm::vec3(10, 10, 10));
        m_scene->addObject(gridObj);

        // 摄像机拉远看全景
        auto perspectiveCamera = std::make_shared<Scene::PerspectiveCamera>();
        perspectiveCamera->setPerspective(glm::radians(45.0f), (float)m_width / m_height, 0.1f, 100.0f);
        perspectiveCamera->lookAt(glm::vec3(0.0f, GRID_CENTER_Y, 15.0f),
                                  glm::vec3(0.0f, GRID_CENTER_Y, 0.0f),
                                  glm::vec3(0.0f, 1.0f, 0.0f));
        m_scene->addCamera(perspectiveCamera);
        m_scene->setActiveCamera(perspectiveCamera);
    }

    std::shared_ptr<Renderer> getRenderer() { return m_renderer; }

    std::shared_ptr< Scene::Scene> getScene() { return m_scene; }
private:
    uint32_t m_width, m_height;
    std::shared_ptr<RHI::IRHI> m_rhi;
    std::shared_ptr<Renderer> m_renderer;
    std::shared_ptr< Scene::Scene> m_scene;

    // 共享 IBL 资源
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
    StarryEngine::Application app;

    //auto dataset = createRenderer(app.getRenderHardwareInterface(), app.getGlobalDescriptorPool(), app.getWidth(), app.getHeight());

    auto demo = std::make_shared<PBRDemo>(app.getRenderHardwareInterface(), app.getGlobalDescriptorPool(), app.getWidth(), app.getHeight());

    app.setRenderer(demo->getRenderer());
    app.setScene(demo->getScene());
    app.initEventDispatcher();
    app.run();
    StarryEngine::Logger::shutdown();
}

DataSet createRenderer(std::shared_ptr<RHI::IRHI> rhi, RHI::DescriptorPoolHandle descriptorPool, uint32_t width, uint32_t height) {
    auto scene = std::make_shared<Scene::Scene>();

    auto renderer = std::make_shared<Renderer>(rhi, descriptorPool, scene);
    renderer->createGlobalSetLayout();
    renderer->createGlobalUniformBuffer();
	renderer->initDefaultMaterials();

    GlobalDescriptorData globalDescriptorData;
    globalDescriptorData.globalDescriptorPool = descriptorPool;
    globalDescriptorData.globalSetLayout = renderer->getGlobalSetLayout();
    globalDescriptorData.globalDescriptorSet = renderer->getGlobalDescriptorSet();

    auto renderPath = RenderPathFactory::createRenderPathFromJSON("assets/configs/forward_render_path.json",rhi, width, height);
    renderer->setRenderPath(std::move(renderPath));

    //auto skyboxEffect = std::make_shared<Scene::ProceduralEffect>();
    //auto skyboxMaterial = createSkyboxMaterial(rhi->getResourceManager(), globalDescriptorData);
    //skyboxEffect->material = skyboxMaterial;  
    //scene->addProceduralEffect(skyboxEffect);

    //auto copyMaterial = createCopyMaterial(rhi->getResourceManager(), globalDescriptorData);
    //auto copyEffect = std::make_shared<Scene::ProceduralEffect>();
    //copyEffect->material = copyMaterial;
    //scene->addProceduralEffect(copyEffect);

    //auto modelMeshData = createModel(rhi->getResourceManager(), globalDescriptorData);
    //auto modelObj = std::make_shared<Scene::RenderObject>();
    //modelObj->geometry = modelMeshData.geometry;
    //modelObj->materials = modelMeshData.materials;
    //modelObj->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.5f));
    //scene->addObject(modelObj);

    auto SphereObj = std::make_shared<Scene::RenderObject>();
    SphereObj->geometry = Assets::GeometryGenerator::createSphere(rhi->getResourceManager(),1.0f);
    SphereObj->materials = { createPbrMaterial(rhi->getResourceManager(), globalDescriptorData) };
    SphereObj->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    scene->addObject(SphereObj);

    //auto QuadObj = std::make_shared<Scene::RenderObject>();
    //QuadObj->geometry = Assets::GeometryGenerator::createQuad(rhi->getResourceManager());
    //glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(20.0f, 20.0f, 20.0f));
    //glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.01f, 0.0f));
    //glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); 
    //QuadObj->transform = translation * rotation * scale;
    //scene->addObject(QuadObj);

    auto gridMeshData = createGrid(rhi->getResourceManager(), globalDescriptorData);
    auto gridObj = std::make_shared<Scene::RenderObject>();
    gridObj->geometry = gridMeshData.geometry;
    gridObj->materials = gridMeshData.materials;
    gridObj->transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0)), glm::vec3(5, 5, 5));
    scene->addObject(gridObj);

    auto perspectiveCamera = std::make_shared<Scene::PerspectiveCamera>();
    perspectiveCamera->setPerspective(glm::radians(45.0f), (float)width / height, 0.1f, 100.0f);
    perspectiveCamera->lookAt(glm::vec3(1.0f, 2.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    scene->addCamera(perspectiveCamera);
    scene->setActiveCamera(perspectiveCamera);
    return DataSet(scene, renderer);
}

ModelData createGrid(std::shared_ptr<RHI::ResourceManager> resMgr, GlobalDescriptorData data) {
    auto gridTmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, data.globalSetLayout);
    gridTmpl->loadShaders("assets/shaders/core/gridShader.vert", "assets/shaders/core/gridShader.frag");

    auto gridMaterialInst = std::make_shared<Assets::MaterialInstance>(gridTmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);
    gridMaterialInst->enableDepthTest(true);

    gridMaterialInst->setSubpassTag("PostProcess_Grid");

    return ModelData(Assets::GeometryGenerator::createGrid(resMgr), { gridMaterialInst });
}

std::shared_ptr<Assets::MaterialInstance> createCopyMaterial(std::shared_ptr<RHI::ResourceManager> resMgr,GlobalDescriptorData data){
    auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, data.globalSetLayout);
    tmpl->loadShaders("assets/shaders/deferred/fullscreen.vert","assets/shaders/deferred/copy.frag");

    auto material = std::make_shared<Assets::MaterialInstance>(tmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);
    material->addTextureDependency("uSceneColor","SceneColor", Assets::ResourceDependencyType::Sampler);
    material->setSubpassTag("PostProcess_Copy");
    return material;
}

//std::shared_ptr<Assets::MaterialInstance> createSkyboxMaterial(std::shared_ptr<RHI::ResourceManager> resMgr,GlobalDescriptorData data) {
//
//    auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, data.globalSetLayout);
//    tmpl->loadShaders("assets/shaders/deferred/skybox.vert", "assets/shaders/deferred/skybox.frag");
//
//    auto material = std::make_shared<Assets::MaterialInstance>(tmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);
//
//    //std::vector<std::string> skyboxFaces = {
//    //    "assets/textures/skybox/right.jpg",
//    //    "assets/textures/skybox/left.jpg",
//    //    "assets/textures/skybox/top.jpg",
//    //    "assets/textures/skybox/bottom.jpg",
//    //    "assets/textures/skybox/front.jpg",
//    //    "assets/textures/skybox/back.jpg"
//    //};
//
//    auto loader = Assets::TextureLoader(rhi->getResourceManager());
//
//    Assets::IBLBuilder iblBuilder(rhi->getResourceManager(), rhi);
//    auto cubemap = iblBuilder.buildEnvCubemap("assets/textures/pbr/kloofendal_48d_partly_cloudy_puresky_4k.hdr", 1024);
//    auto sampler = loader.createDefaultSampler();
//
//    material->setTexture("uSkybox", cubemap, sampler);
//    material->setSubpassTag("PostProcess_Skybox");
//    material->enableDepthTest(true);
//    material->setDepthCompareOp(RHI::CompareOp::LessOrEqual);
//
//    return material;
//}

ModelData createModel(std::shared_ptr<RHI::ResourceManager> resMgr, GlobalDescriptorData data) {
    std::vector<RHI::PushConstantRange> pushConstants = {
        {RHI::ShaderStage::Vertex, 0, sizeof(glm::mat4)}
    };

    auto geometry = std::make_shared<Assets::Geometry>(resMgr);
    std::vector<Assets::MaterialParams> params;
    if (!Assets::ModelLoader::loadFromFile(resMgr, "assets/models/Griseo.obj", *geometry, params)) {
        LOG_ERROR("Failed to load model");
    }
    geometry->uploadToGPU();

    std::vector<std::shared_ptr<Assets::MaterialInstance>> materialInstances;

    for (auto& param : params) {
        std::string fsPath;
        if (param.name == "body") fsPath = "assets/shaders/core/shader.frag";
        else if (param.name == "brow") fsPath = "assets/shaders/core/shader.frag";
        else if (param.name == "eyes") fsPath = "assets/shaders/core/shader.frag";
        else if (param.name == "face") fsPath = "assets/shaders/core/face.frag";
        else fsPath = "assets/shaders/core/hair.frag";

        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, data.globalSetLayout);
        if (!tmpl->loadShaders("assets/shaders/core/shader.vert", fsPath)) {
            LOG_ERROR("Failed to load shaders for material: {}", param.name);
            continue;
        }

        LOG_INFO("createModel:fsPath{}", fsPath);
        auto instance = std::make_shared<Assets::MaterialInstance>(tmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);

        if (!param.albedoTexture.empty()) {
            Assets::TextureLoader loader(resMgr);
            LOG_INFO("createModel:param.albedoTexture{}", param.albedoTexture);
            auto texResult = loader.loadTexture2D(param.albedoTexture, RHI::Format::RGBA8_UNorm, "Albedo");

            
            if (texResult.texture.isValid()) {
                instance->setTexture("texSampler", texResult.texture, texResult.sampler);
            }
            else {
                LOG_ERROR("Failed to load texture: {}", param.albedoTexture);
            }
        }
        else {
            LOG_WARN("Material {} has no albedo texture", param.name);
        }
        instance->setSubpassTag("Forward_Opaque");
        instance->setDepthTest(true);
        instance->setDepthWrite(true);
        materialInstances.push_back(instance);
    }

    return ModelData(geometry, materialInstances);
}

std::shared_ptr<Assets::MaterialInstance> createPbrMaterial(std::shared_ptr<RHI::ResourceManager> resMgr, GlobalDescriptorData data) {
    auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, data.globalSetLayout);
    tmpl->loadShaders("assets/shaders/pbr/shpere_pbr.vert", "assets/shaders/pbr/shpere_pbr.frag");

    auto material = std::make_shared<Assets::MaterialInstance>(tmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);
    material->setSubpassTag("Forward_Opaque");

    material->enableDepthTest(true);
    material->enableDepthWrite(true);

    Assets::TextureLoader loader(resMgr);
    auto texResult0 = loader.loadTexture2D("assets/textures/pbr/seaworn_sandstone_brick_arm_1k.png", RHI::Format::RGBA8_UNorm, "arm");
    auto texResult1 = loader.loadTexture2D("assets/textures/pbr/seaworn_sandstone_brick_diff_1k.png", RHI::Format::RGBA8_UNorm, "diff");
    auto texResult2 = loader.loadTexture2D("assets/textures/pbr/seaworn_sandstone_brick_nor_dx_1k.png", RHI::Format::RGBA8_UNorm, "nor_dx");

    material->setTexture("armMap", texResult0.texture, texResult0.sampler);
    material->setTexture("albedoMap", texResult1.texture, texResult1.sampler);
    material->setTexture("normalMap", texResult2.texture, texResult2.sampler);

    auto* lightBlock = material->getBlock("LightingUBO");
    lightBlock->setVec4("lights.position", glm::vec4(0.2f, 0.0f, -1.0f, 0.0f));
    lightBlock->setVec4("lights.color", glm::vec4(0.9f, 0.1f, 0.5f, 1.0f));
    lightBlock->setFloat("lightCount", 1.0f);
    lightBlock->setFloat("ambientStrength", 0.1f);

    material->applyAllDirtyBlocks();
	return material;
}