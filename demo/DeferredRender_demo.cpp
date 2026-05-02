#include "../src/renderer/passes/PassWrapper.hpp"
#include "../src/renderer/passes/GeometrySubpass.hpp"
#include "../src/renderer/passes/LightSubpass.hpp"
#include "../src/renderer/subpassRecorder/SkyboxRecorder.hpp"

#include"../src/application/Application.hpp"
#include "type.hpp"

using namespace StarryEngine;
struct MaterialUniforms {
    glm::vec4 baseColor;   // 16 字节
    float metallic;        // 4 字节
    float roughness;       // 4 字节
};
ModelData createModel(std::shared_ptr<RHI::ResourceManager> resMgr, GlobalDescriptorData data);
ModelData createGrid(std::shared_ptr<RHI::ResourceManager> resMgr, GlobalDescriptorData data);
DataSet createRenderer(std::shared_ptr<RHI::IRHI> rhi, RHI::DescriptorPoolHandle descriptorPool, uint32_t width, uint32_t height);
std::shared_ptr<Assets::MaterialInstance> createLightingMaterial(
    std::shared_ptr<RHI::ResourceManager> resMgr,
    const GlobalDescriptorData& data);
std::shared_ptr<Assets::MaterialInstance> createCopyMaterial(
    std::shared_ptr<RHI::ResourceManager> resMgr,
    GlobalDescriptorData data);
std::shared_ptr<DeferredRenderPath> createDeferredRenderPath(
    std::shared_ptr<RHI::IRHI> rhi,
    uint32_t width, uint32_t height,
    GlobalDescriptorData globalDescriptorData);  
std::shared_ptr<Assets::MaterialInstance> createSkyboxMaterial(std::shared_ptr<RHI::ResourceManager> resMgr,
    GlobalDescriptorData data);

std::shared_ptr<Assets::MaterialInstance> createPbrMaterial(std::shared_ptr<RHI::ResourceManager> resMgr, GlobalDescriptorData data);

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

    auto dataset = createRenderer(app.getRenderHardwareInterface(), app.getGlobalDescriptorPool(), app.getWidth(), app.getHeight());

    app.setRenderer(dataset.renderer);
    app.setScene(dataset.scene);
    app.initEventDispatcher();
    app.run();
    StarryEngine::Logger::shutdown();
}

std::shared_ptr<DeferredRenderPath> createDeferredRenderPath(
    std::shared_ptr<RHI::IRHI> rhi,
    uint32_t width, uint32_t height,
    GlobalDescriptorData globalDescriptorData) {

    auto renderPath = std::make_shared<DeferredRenderPath>(rhi, width, height);

    renderPath->addTextureDesc("Depth", PassWrapper::createDepthTextureDesc({ width, height, 1 }, rhi->getDepthFormat()));
    renderPath->addTextureDesc("Albedo", PassWrapper::createColorTextureDesc({ width, height, 1 }, RHI::Format::RGBA8_UNorm));
    renderPath->addTextureDesc("Normal", PassWrapper::createColorTextureDesc({ width, height, 1 }, RHI::Format::RGBA16_Float));
    renderPath->addTextureDesc("Material", PassWrapper::createColorTextureDesc({ width, height, 1 }, RHI::Format::RGBA8_UNorm));
    renderPath->addTextureDesc("Swapchain", PassWrapper::createColorTextureDesc({ width, height, 1 }, RHI::Format::BGRA8_sRGB));
    renderPath->addTextureDesc("SceneColor", PassWrapper::createColorTextureDesc({ width, height, 1 }, RHI::Format::RGBA8_UNorm));

    // ---------- 几何子通道 ----------
    auto colorAttach = PassWrapper::createColorAttachment(
        RHI::ImageLayout::Undefined, RHI::ImageLayout::ShaderReadOnly,
        RHI::AttachmentLoadOp::Clear, RHI::AttachmentStoreOp::Store);
    auto depthAttach = PassWrapper::createDepthAttachment(
        RHI::ImageLayout::Undefined, RHI::ImageLayout::DepthStencilAttachment,
        RHI::AttachmentLoadOp::Clear, RHI::AttachmentStoreOp::Store);

    Subpass geometrypass("Geometry", std::make_shared<MeshDrawRecorder>());
    geometrypass.addColorAttachment("Albedo", colorAttach)
        .addColorAttachment("Normal", colorAttach)
        .addColorAttachment("Material", colorAttach)
        .setDepthAttachment("Depth", depthAttach);

    // ---------- 光照子通道 ----------
    auto lightColorAttachment = PassWrapper::createColorAttachment(
        RHI::ImageLayout::ShaderReadOnly, RHI::ImageLayout::ShaderReadOnly,
        RHI::AttachmentLoadOp::Load, RHI::AttachmentStoreOp::Store);
    auto lightInputAttachment = PassWrapper::createColorAttachment(
        RHI::ImageLayout::ShaderReadOnly, RHI::ImageLayout::ShaderReadOnly,
        RHI::AttachmentLoadOp::Load, RHI::AttachmentStoreOp::DontCare);

    Subpass lightSubpass("Lighting", std::make_shared<DeferredLightingRecorder>());
    lightSubpass.addColorAttachment("SceneColor", lightColorAttachment)
        .addInputAttachment("Albedo", lightInputAttachment)
        .addInputAttachment("Normal", lightInputAttachment)
        .addInputAttachment("Material", lightInputAttachment);


    // ---------- 向前路径通道 ----------
    auto forwardColorAttach = PassWrapper::createColorAttachment(
        RHI::ImageLayout::Undefined, RHI::ImageLayout::ShaderReadOnly,
        RHI::AttachmentLoadOp::Clear, RHI::AttachmentStoreOp::Store);
    auto forwardDepthAttach = PassWrapper::createDepthAttachment(
        RHI::ImageLayout::Undefined, RHI::ImageLayout::DepthStencilAttachment,
        RHI::AttachmentLoadOp::Clear, RHI::AttachmentStoreOp::Store);

    Subpass forwardpass("Forward", std::make_shared<MeshDrawRecorder>());
    forwardpass.addColorAttachment("SceneColor", forwardColorAttach)
        .setDepthAttachment("Depth", forwardDepthAttach);

    auto skyboxOutputAttach = PassWrapper::createColorAttachment(
        RHI::ImageLayout::ShaderReadOnly, RHI::ImageLayout::ShaderReadOnly,
        RHI::AttachmentLoadOp::Load, RHI::AttachmentStoreOp::Store); 
    auto skyboxDepthAttachment = PassWrapper::createDepthAttachment(
        RHI::ImageLayout::DepthStencilAttachment, RHI::ImageLayout::DepthStencilAttachment,
        RHI::AttachmentLoadOp::Load, RHI::AttachmentStoreOp::Store);

    Subpass skyboxSubpass("Skybox", std::make_shared<SkyboxRecorder>()); 
    skyboxSubpass.addColorAttachment("SceneColor", skyboxOutputAttach)
        .setDepthAttachment("Depth", skyboxDepthAttachment);

    // ---------- 网格子通道 ----------
    auto gridColorAttachment = PassWrapper::createColorAttachment(
        RHI::ImageLayout::ShaderReadOnly, RHI::ImageLayout::ShaderReadOnly,
        RHI::AttachmentLoadOp::Load, RHI::AttachmentStoreOp::Store);
    auto gridDepthAttachment = PassWrapper::createDepthAttachment(
        RHI::ImageLayout::DepthStencilAttachment, RHI::ImageLayout::DepthStencilAttachment,
        RHI::AttachmentLoadOp::Load, RHI::AttachmentStoreOp::DontCare);

    Subpass gridSubpass("Grid", std::make_shared<MeshDrawRecorder>());
    gridSubpass.addColorAttachment("SceneColor", gridColorAttachment)
        .setDepthAttachment("Depth", gridDepthAttachment);


    // ---------- 复制子通道 ----------
    auto copyColorAttachment = PassWrapper::createColorAttachment(
        RHI::ImageLayout::Undefined, RHI::ImageLayout::PresentSrc,
        RHI::AttachmentLoadOp::Clear, RHI::AttachmentStoreOp::Store);
    auto copyInputAttachment = PassWrapper::createColorAttachment(
        RHI::ImageLayout::ShaderReadOnly, RHI::ImageLayout::ShaderReadOnly,
        RHI::AttachmentLoadOp::Load, RHI::AttachmentStoreOp::DontCare);


    Subpass copySubpass("Copy", std::make_shared<CopyToSwapchainRecorder>());
    copySubpass.addColorAttachment("Swapchain", copyColorAttachment)
        .addInputAttachment("SceneColor", copyInputAttachment);

    //renderPath->addSubpass(Scene::RenderStage::GBuffer, Scene::RenderQueue::Opaque, geometrypass);
    //renderPath->addSubpass(Scene::RenderStage::GBuffer, Scene::RenderQueue::Transparent, lightSubpass);
    renderPath->addSubpass(Scene::RenderStage::Forward, Scene::RenderQueue::Opaque, forwardpass);
    renderPath->addSubpass(Scene::RenderStage::PostProcess, Scene::RenderQueue::Skybox, skyboxSubpass);
    renderPath->addSubpass(Scene::RenderStage::PostProcess, Scene::RenderQueue::Opaque, gridSubpass);
    renderPath->addSubpass(Scene::RenderStage::PostProcess, Scene::RenderQueue::Transparent, copySubpass);

    if (!renderPath->initialize()) {
        LOG_ERROR("Failed to initialize deferred render path");
    }

    return renderPath;
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

    auto renderPath = createDeferredRenderPath(rhi, width, height, globalDescriptorData);
    renderer->setRenderPath(std::move(renderPath));

    //auto lightingMaterial = createLightingMaterial(rhi->getResourceManager(), globalDescriptorData);
    //auto lightEffect = std::make_shared<Scene::ProceduralEffect>();
    //lightEffect->material = lightingMaterial;
    //lightEffect->stage = Scene::RenderStage::GBuffer;
    //lightEffect->queue = Scene::RenderQueue::Transparent;
    //scene->addProceduralEffect(lightEffect);

    auto skyboxEffect = std::make_shared<Scene::ProceduralEffect>();
    auto skyboxMaterial = createSkyboxMaterial(rhi->getResourceManager(), globalDescriptorData);
    if (skyboxMaterial) {
        auto skyboxEffect = std::make_shared<Scene::ProceduralEffect>();
        skyboxEffect->material = skyboxMaterial;
        skyboxEffect->stage = Scene::RenderStage::PostProcess;
        skyboxEffect->queue = Scene::RenderQueue::Skybox;   
        skyboxEffect->vertexCount = 3;                     
        skyboxEffect->instanceCount = 1;
        scene->addProceduralEffect(skyboxEffect);
    }
    else {
        LOG_WARN("Skybox disabled due to texture loading failure");
    }

    auto copyMaterial = createCopyMaterial(rhi->getResourceManager(), globalDescriptorData);
    auto copyEffect = std::make_shared<Scene::ProceduralEffect>();
    copyEffect->material = copyMaterial;
    copyEffect->stage = Scene::RenderStage::PostProcess;
    copyEffect->queue = Scene::RenderQueue::Transparent;  
    scene->addProceduralEffect(copyEffect);

    auto modelMeshData = createModel(rhi->getResourceManager(), globalDescriptorData);
    auto modelObj = std::make_shared<Scene::RenderObject>();
    modelObj->geometry = modelMeshData.geometry;
    modelObj->materials = modelMeshData.materials;
    modelObj->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.5f));
    scene->addObject(modelObj);

    auto SphereObj = std::make_shared<Scene::RenderObject>();
    SphereObj->geometry = Assets::GeometryGenerator::createSphere(rhi->getResourceManager(),1.0f);
    //SphereObj->geometry = modelMeshData.geometry;
    SphereObj->materials = { createPbrMaterial(rhi->getResourceManager(), globalDescriptorData) };

    //const auto& submeshes = modelMeshData.geometry->getSubmeshes();
    //size_t submeshCount = submeshes.size();
    //std::vector<std::shared_ptr<Assets::MaterialInstance>> pbrMaterials;
    //for (size_t i = 0; i < submeshCount; ++i) {
    //    pbrMaterials.push_back(createPbrMaterial(rhi->getResourceManager(), globalDescriptorData));
    //}

    //SphereObj->materials = pbrMaterials;
    SphereObj->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    scene->addObject(SphereObj);

    auto QuadObj = std::make_shared<Scene::RenderObject>();
    QuadObj->geometry = Assets::GeometryGenerator::createQuad(rhi->getResourceManager());
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(20.0f, 20.0f, 20.0f));
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.01f, 0.0f));
    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); 
    QuadObj->transform = translation * rotation * scale;
    scene->addObject(QuadObj);

    //auto cubeObj = std::make_shared<Scene::RenderObject>();
    //cubeObj->geometry = Assets::GeometryGenerator::createCube(rhi->getResourceManager());
    //cubeObj->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 3.0f));
    //scene->addObject(cubeObj);

    //auto cylinderObj1 = std::make_shared<Scene::RenderObject>();
    //cylinderObj1->geometry = Assets::GeometryGenerator::createCylinder(rhi->getResourceManager(), 0.5f, 1.0f, 1.0f, 32, 32, true, true);
    //cylinderObj1->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 5.0f));
    //scene->addObject(cylinderObj1);

    //auto cylinderObj2 = std::make_shared<Scene::RenderObject>();
    //cylinderObj2->geometry = Assets::GeometryGenerator::createCylinder(rhi->getResourceManager(), 1.0f, 1.0f, 1.0f, 6, 6, true, true);
    //cylinderObj2->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 7.0f));
    //scene->addObject(cylinderObj2);

    //auto SphereMeshData = createSphere(rhi->getResourceManager(), globalDescriptorData);
    //auto SphereObj = std::make_shared<Scene::RenderObject>();
    //SphereObj->geometry = SphereMeshData.geometry;
    //SphereObj->materials = SphereMeshData.materials;
    //SphereObj->transform = glm::mat4(1.0f);
    //scene->addObject(SphereObj);

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
    gridMaterialInst->setRenderStage(Scene::RenderStage::PostProcess);
    gridMaterialInst->setRenderQueue(Scene::RenderQueue::Opaque);
    gridMaterialInst->enableDepthTest(true);

    return ModelData(Assets::GeometryGenerator::createGrid(resMgr), { gridMaterialInst });
}

std::shared_ptr<Assets::MaterialInstance> createCopyMaterial(std::shared_ptr<RHI::ResourceManager> resMgr,GlobalDescriptorData data){
    auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, data.globalSetLayout);
    tmpl->loadShaders("assets/shaders/deferred/fullscreen.vert","assets/shaders/deferred/copy.frag");

    auto material = std::make_shared<Assets::MaterialInstance>(tmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);
    material->addTextureDependency("SceneColor", 1, 0, Assets::ResourceDependencyType::Sampler);

    material->setRenderStage(Scene::RenderStage::PostProcess);
    material->setRenderQueue(Scene::RenderQueue::Opaque);
    return material;
}

std::shared_ptr<Assets::MaterialInstance> createSkyboxMaterial(std::shared_ptr<RHI::ResourceManager> resMgr,GlobalDescriptorData data) {

    auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, data.globalSetLayout);
    tmpl->loadShaders("assets/shaders/deferred/skybox.vert", "assets/shaders/deferred/skybox.frag");

    auto material = std::make_shared<Assets::MaterialInstance>(tmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);

    std::vector<std::string> skyboxFaces = {
        "assets/textures/skybox/right.jpg",
        "assets/textures/skybox/left.jpg",
        "assets/textures/skybox/top.jpg",
        "assets/textures/skybox/bottom.jpg",
        "assets/textures/skybox/front.jpg",
        "assets/textures/skybox/back.jpg"
    };

    auto loader = Assets::TextureLoader(resMgr);
    auto texResult = loader.loadTextureCube(skyboxFaces, RHI::Format::RGBA8_sRGB, "SkyboxCubeMap");
    material->setTexture("uSkybox", texResult.texture, texResult.sampler);

    material->setRenderStage(Scene::RenderStage::PostProcess);
    material->setRenderQueue(Scene::RenderQueue::Skybox);
    material->enableDepthTest(true);
    material->setDepthCompareOp(RHI::CompareOp::LessOrEqual);

    return material;
}

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

        instance->setRenderStage(Scene::RenderStage::Forward);
        instance->setRenderQueue(Scene::RenderQueue::Opaque);
        instance->setDepthTest(true);
        instance->setDepthWrite(true);
        materialInstances.push_back(instance);
    }

    return ModelData(geometry, materialInstances);
}

std::shared_ptr<Assets::MaterialInstance> createPbrMaterial(std::shared_ptr<RHI::ResourceManager> resMgr, GlobalDescriptorData data) {
    auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, data.globalSetLayout);
    if (!tmpl->loadShaders("assets/shaders/pbr/shpere_pbr.vert", "assets/shaders/pbr/shpere_pbr.frag")) {
        LOG_ERROR("Failed to load shpere_pbr shaders");
        return nullptr;
    }
    auto material = std::make_shared<Assets::MaterialInstance>(tmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);
    material->setRenderStage(Scene::RenderStage::Forward);
    material->setRenderQueue(Scene::RenderQueue::Opaque);
    material->enableDepthTest(true);
    material->enableDepthWrite(true);

    auto* lightBlock = material->getBlock("LightingUBO");
    if (lightBlock) {
        lightBlock->setVec4("lights[0].position", glm::vec4(0.2f, 0.0f, -1.0f, 0.0f));
        lightBlock->setVec4("lights[0].color", glm::vec4(0.9f, 0.9f, 0.5f, 1.0f));
        lightBlock->setFloat("lightCount", 1.0f);
        lightBlock->setFloat("ambientStrength", 0.1f);
    } else {
        LOG_ERROR("LightingUBO not found");
    }

    Assets::TextureLoader loader(resMgr);
    auto texResult0 = loader.loadTexture2D("assets/textures/pbr/seaworn_sandstone_brick_arm_1k.png", RHI::Format::RGBA8_UNorm, "arm");
    auto texResult1 = loader.loadTexture2D("assets/textures/pbr/seaworn_sandstone_brick_diff_1k.png", RHI::Format::RGBA8_UNorm, "diff");
    auto texResult2 = loader.loadTexture2D("assets/textures/pbr/seaworn_sandstone_brick_nor_dx_1k.png", RHI::Format::RGBA8_UNorm, "nor_dx");

    material->setTexture("armMap", texResult0.texture, texResult0.sampler);
    material->setTexture("albedoMap", texResult1.texture, texResult1.sampler);
    material->setTexture("normalMap", texResult2.texture, texResult2.sampler);

    material->applyAllDirtyBlocks();
	return material;
}