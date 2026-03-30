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

ModelData createSphere(std::shared_ptr<RHI::ResourceManager> resMgr, GlobalDescriptorData data);
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
    renderPath->addTextureDesc("LightAccum", PassWrapper::createColorTextureDesc({ width, height, 1 }, RHI::Format::RGBA16_Float));

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
        RHI::ImageLayout::Undefined, RHI::ImageLayout::ShaderReadOnly,
        RHI::AttachmentLoadOp::Clear, RHI::AttachmentStoreOp::Store);
    auto lightInputAttachment = PassWrapper::createColorAttachment(
        RHI::ImageLayout::ShaderReadOnly, RHI::ImageLayout::ShaderReadOnly,
        RHI::AttachmentLoadOp::Load, RHI::AttachmentStoreOp::DontCare);

    Subpass lightSubpass("Lighting", std::make_shared<DeferredLightingRecorder>());
    lightSubpass.addColorAttachment("LightAccum", lightColorAttachment)
        .addInputAttachment("Albedo", lightInputAttachment)
        .addInputAttachment("Normal", lightInputAttachment)
        .addInputAttachment("Material", lightInputAttachment);

    auto skyboxOutputAttach = PassWrapper::createColorAttachment(
        RHI::ImageLayout::ShaderReadOnly, RHI::ImageLayout::ShaderReadOnly,
        RHI::AttachmentLoadOp::Clear, RHI::AttachmentStoreOp::Store);  
    auto skyboxInputAttach = PassWrapper::createColorAttachment(
        RHI::ImageLayout::ShaderReadOnly, RHI::ImageLayout::ShaderReadOnly,
        RHI::AttachmentLoadOp::Load, RHI::AttachmentStoreOp::DontCare);      

    Subpass skyboxSubpass("Skybox", std::make_shared<SkyboxRecorder>()); 
    skyboxSubpass.addColorAttachment("SceneColor", skyboxOutputAttach)
        .addInputAttachment("LightAccum", skyboxInputAttach);

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

    renderPath->addSubpass(Scene::RenderStage::GBuffer, Scene::RenderQueue::Opaque, geometrypass);
    renderPath->addSubpass(Scene::RenderStage::Lighting, Scene::RenderQueue::Opaque, lightSubpass);
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

    GlobalDescriptorData globalDescriptorData;
    globalDescriptorData.globalDescriptorPool = descriptorPool;
    globalDescriptorData.globalSetLayout = renderer->getGlobalSetLayout();
    globalDescriptorData.globalDescriptorSet = renderer->getGlobalDescriptorSet();

    auto renderPath = createDeferredRenderPath(rhi, width, height, globalDescriptorData);
    renderer->setRenderPath(std::move(renderPath));

    auto lightingMaterial = createLightingMaterial(rhi->getResourceManager(), globalDescriptorData);
    auto lightEffect = std::make_shared<Scene::ProceduralEffect>();
    lightEffect->material = lightingMaterial;
    lightEffect->stage = Scene::RenderStage::Lighting;
    lightEffect->queue = Scene::RenderQueue::Opaque;
    scene->addProceduralEffect(lightEffect);

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


    auto SphereMeshData = createSphere(rhi->getResourceManager(), globalDescriptorData);
    auto SphereObj = std::make_shared<Scene::RenderObject>();
    SphereObj->geometry = SphereMeshData.geometry;
    SphereObj->materials = SphereMeshData.materials;
    SphereObj->transform = glm::mat4(1.0f);
    scene->addObject(SphereObj);

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
    std::vector<RHI::PushConstantRange> pushConstants = {
        {RHI::ShaderStage::Vertex, 0, sizeof(glm::mat4)}
    };

    std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> gridLayoutMap;
    gridLayoutMap[0] = data.globalSetLayout;
    auto gridTmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, gridLayoutMap, pushConstants);
    gridTmpl->loadShaders("assets/shaders/core/gridShader.vert", "assets/shaders/core/gridShader.frag");

    auto gridMaterialInst = std::make_shared<Assets::MaterialInstance>(gridTmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);
    gridMaterialInst->setRenderStage(Scene::RenderStage::PostProcess);
    gridMaterialInst->setRenderQueue(Scene::RenderQueue::Opaque);
    gridMaterialInst->enableDepthTest(true);

    return ModelData(Assets::Shape::createGridGeometry(resMgr), { gridMaterialInst });
}

ModelData createSphere(std::shared_ptr<RHI::ResourceManager> resMgr, GlobalDescriptorData data) {
    std::vector<RHI::PushConstantRange> pushConstants = {
        {RHI::ShaderStage::Vertex, 0, sizeof(glm::mat4)}
    };

    std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> LayoutMap;
    LayoutMap[0] = data.globalSetLayout;

    RHI::DescriptorSetLayoutDesc materialLayoutDesc;
    materialLayoutDesc.bindings = {
        {0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Fragment},
        { 1, RHI::DescriptorType::InputAttachment, 1, RHI::ShaderStage::Fragment }
    };
    auto set1Layout = Assets::DescriptorSetLayoutCache::getOrCreateLayout(resMgr.get(), materialLayoutDesc);
    LayoutMap[1] = set1Layout;

    auto SphereTmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, LayoutMap, pushConstants);
    SphereTmpl->loadShaders("assets/shaders/deferred/sphere_deferred.vert", "assets/shaders/deferred/sphere_deferred.frag");

    auto SphereMaterialInst = std::make_shared<Assets::MaterialInstance>(SphereTmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);
    SphereMaterialInst->setRenderStage(Scene::RenderStage::GBuffer);
    SphereMaterialInst->setRenderQueue(Scene::RenderQueue::Opaque);
    SphereMaterialInst->enableDepthTest(true);
    SphereMaterialInst->enableDepthWrite(true);

    auto attachments = {
        RHI::BlendAttachmentState{},  // Albedo
        RHI::BlendAttachmentState{},  // Normal
        RHI::BlendAttachmentState{}   // Material
    };
    SphereMaterialInst->setAttachments(attachments);

    MaterialUniforms uniforms;
    uniforms.baseColor = glm::vec4(0.0f, 1.0f, 0.0f, 0.5f); // 半透明绿色
    uniforms.metallic = 0.0f;
    uniforms.roughness = 0.5f;

    SphereMaterialInst->setUniform(1, 0, &uniforms, sizeof(MaterialUniforms));

    return ModelData(Assets::Shape::createSphereGeometry(resMgr, 1.0f, 50, 20), { SphereMaterialInst });
}

std::shared_ptr<Assets::MaterialInstance> createLightingMaterial(
    std::shared_ptr<RHI::ResourceManager> resMgr,
    const GlobalDescriptorData& data)
{
    std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> layoutMap;
    layoutMap[0] = data.globalSetLayout;

    RHI::DescriptorSetLayoutDesc layoutDesc;
    layoutDesc.bindings = {
        {0, RHI::DescriptorType::InputAttachment, 1, RHI::ShaderStage::Fragment},
        {1, RHI::DescriptorType::InputAttachment, 1, RHI::ShaderStage::Fragment},
        {2, RHI::DescriptorType::InputAttachment, 1, RHI::ShaderStage::Fragment}
    };
    auto set1Layout = Assets::DescriptorSetLayoutCache::getOrCreateLayout(resMgr.get(), layoutDesc);
    layoutMap[1] = set1Layout;

    auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(
        resMgr, layoutMap, std::vector<RHI::PushConstantRange>{});
    if (!tmpl->loadShaders("assets/shaders/deferred/fullscreen.vert",
        "assets/shaders/deferred/lighting.frag")) {
        LOG_ERROR("Failed to load lighting shaders");
        return nullptr;
    }

    auto material = std::make_shared<Assets::MaterialInstance>(tmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);

    material->addTextureDependency("Albedo", 1, 0, Assets::ResourceDependencyType::InputAttachment);
    material->addTextureDependency("Normal", 1, 1, Assets::ResourceDependencyType::InputAttachment);
    material->addTextureDependency("Material", 1, 2, Assets::ResourceDependencyType::InputAttachment);

    material->setRenderStage(Scene::RenderStage::Lighting);
    material->setRenderQueue(Scene::RenderQueue::Opaque);
    material->enableDepthTest(false);
    material->enableDepthWrite(false);
    return material;
}

std::shared_ptr<Assets::MaterialInstance> createCopyMaterial(
    std::shared_ptr<RHI::ResourceManager> resMgr,
    GlobalDescriptorData data)
{
    std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> layoutMap;
    layoutMap[0] = data.globalSetLayout;

    // 添加 set 1 布局，用于纹理采样
    RHI::DescriptorSetLayoutDesc layoutDesc;
    layoutDesc.bindings = {
        {0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment}
    };
    auto set1Layout = Assets::DescriptorSetLayoutCache::getOrCreateLayout(resMgr.get(), layoutDesc);
    layoutMap[1] = set1Layout;

    auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(
        resMgr, layoutMap, std::vector<RHI::PushConstantRange>{});
    if (!tmpl->loadShaders("assets/shaders/deferred/fullscreen.vert",
        "assets/shaders/deferred/copy.frag")) {
        LOG_ERROR("Failed to load copy shaders");
        return nullptr;
    }

    auto material = std::make_shared<Assets::MaterialInstance>(tmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);
    material->addTextureDependency("SceneColor", 1, 0, Assets::ResourceDependencyType::Sampler);

    material->setRenderStage(Scene::RenderStage::PostProcess);
    material->setRenderQueue(Scene::RenderQueue::Opaque);
    return material;
}

std::shared_ptr<Assets::MaterialInstance> createSkyboxMaterial(std::shared_ptr<RHI::ResourceManager> resMgr,
    GlobalDescriptorData data) {
    std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> layoutMap;
    layoutMap[0] = data.globalSetLayout;

    // set 1 布局：绑定 0 为立方体贴图采样器
    RHI::DescriptorSetLayoutDesc layoutDesc;
    layoutDesc.bindings = {
        {0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment},
        {1, RHI::DescriptorType::InputAttachment, 1, RHI::ShaderStage::Fragment}
    };
    auto set1Layout = Assets::DescriptorSetLayoutCache::getOrCreateLayout(resMgr.get(), layoutDesc);
    layoutMap[1] = set1Layout;

    auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, layoutMap, std::vector<RHI::PushConstantRange>{});

    if (!tmpl->loadShaders("assets/shaders/deferred/skybox.vert","assets/shaders/deferred/skybox.frag")) {
        LOG_ERROR("Failed to load skybox shaders");
        return nullptr;
    }

    std::vector<std::string> skyboxFaces = {
        "assets/textures/skybox/right.jpg",
        "assets/textures/skybox/left.jpg",
        "assets/textures/skybox/top.jpg",
        "assets/textures/skybox/bottom.jpg",
        "assets/textures/skybox/front.jpg",
        "assets/textures/skybox/back.jpg"
    };
    auto loader = Assets::TextureLoader(resMgr);
    auto [skyboxTex, skyboxSampler] = loader.loadTextureCube(skyboxFaces, RHI::Format::RGBA8_sRGB, "SkyboxCubeMap");
    LOG_INFO("Skybox texture handle = {}", skyboxTex.getIndex());
    if (!skyboxTex.isValid()) {
        LOG_ERROR("Failed to load skybox texture, skybox will be disabled");
        return nullptr;  
    }

    auto material = std::make_shared<Assets::MaterialInstance>(tmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);
    material->setTexture(1, 0, skyboxTex, skyboxSampler);
    material->addTextureDependency("LightAccum", 1, 1, Assets::ResourceDependencyType::InputAttachment);

    material->setRenderStage(Scene::RenderStage::PostProcess);
    material->setRenderQueue(Scene::RenderQueue::Skybox);
    material->enableDepthTest(false);          
    material->enableDepthWrite(false);
    material->setDepthCompareOp(RHI::CompareOp::LessOrEqual);

    return material;
}