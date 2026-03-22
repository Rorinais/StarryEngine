#include"../application/Application.hpp"
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
    GlobalDescriptorData globalDescriptorData){
    auto renderPath = std::make_shared<DeferredRenderPath>(rhi, width, height);
    std::unordered_map<std::string, RHI::TextureDesc> textureDescs;

    RHI::TextureDesc colorDesc;
    colorDesc.extent = { width, height, 1 };
    colorDesc.format = RHI::Format::RGBA8_UNorm;
    colorDesc.type = RHI::TextureType::Texture2D;
    colorDesc.allowRenderTarget = true;
    colorDesc.allowInputAttachment = true;
    textureDescs["Albedo"] = colorDesc;
    textureDescs["Normal"] = colorDesc; 
    textureDescs["Material"] = colorDesc;
    textureDescs["SceneColor"] = colorDesc; 

    RHI::TextureDesc normalDesc = colorDesc;
    normalDesc.format = RHI::Format::RGBA16_Float;
    textureDescs["Normal"] = normalDesc;

    // 深度纹理
    RHI::TextureDesc depthDesc = colorDesc;
    depthDesc.format = rhi->getDepthFormat();
    depthDesc.allowDepthStencil = true;
    depthDesc.allowRenderTarget = false;
    textureDescs["Depth"] = depthDesc;

    // 交换链纹理
    RHI::TextureDesc swapchainDesc = colorDesc;
    swapchainDesc.format = RHI::Format::BGRA8_sRGB;
    textureDescs["Swapchain"] = swapchainDesc;

    renderPath->setTextureDescs(textureDescs);

    // ---------- 几何子通道 ----------
    SubpassConfig geomSubpass;
    geomSubpass.name = "Geometry";

    // 颜色附件：Albedo
    SubpassAttachment albedoAttach;
    albedoAttach.textureName = "Albedo";
    albedoAttach.params.clearColor = RHI::Color{ 0.0f, 0.0f, 0.0f, 0.0f };
    albedoAttach.params.loadOp = RHI::AttachmentLoadOp::Clear;
    albedoAttach.params.storeOp = RHI::AttachmentStoreOp::Store;
    albedoAttach.params.initialLayout = RHI::ImageLayout::Undefined;
    albedoAttach.params.finalLayout = RHI::ImageLayout::ColorAttachment;
    geomSubpass.colorAttachments.push_back(albedoAttach);

    // 颜色附件：Normal
    SubpassAttachment normalAttach;
    normalAttach.textureName = "Normal";
    normalAttach.params.clearColor = RHI::Color{ 0.0f, 0.0f, 0.0f, 0.0f };
    normalAttach.params.loadOp = RHI::AttachmentLoadOp::Clear;
    normalAttach.params.storeOp = RHI::AttachmentStoreOp::Store;
    normalAttach.params.initialLayout = RHI::ImageLayout::Undefined;
    normalAttach.params.finalLayout = RHI::ImageLayout::ColorAttachment;
    geomSubpass.colorAttachments.push_back(normalAttach);

    // 颜色附件：Material
    SubpassAttachment materialAttach;
    materialAttach.textureName = "Material";
    materialAttach.params.clearColor = RHI::Color{ 0.0f, 0.0f, 0.0f, 0.0f };  
    materialAttach.params.loadOp = RHI::AttachmentLoadOp::Clear;
    materialAttach.params.storeOp = RHI::AttachmentStoreOp::Store;
    materialAttach.params.initialLayout = RHI::ImageLayout::Undefined;
    materialAttach.params.finalLayout = RHI::ImageLayout::ColorAttachment;
    geomSubpass.colorAttachments.push_back(materialAttach);

    // 深度附件
    SubpassAttachment depthAttach;
    depthAttach.textureName = "Depth";
    depthAttach.params.clearDepth = 1.0f;
    depthAttach.params.loadOp = RHI::AttachmentLoadOp::Clear;
    depthAttach.params.storeOp = RHI::AttachmentStoreOp::DontCare;
    depthAttach.params.initialLayout = RHI::ImageLayout::Undefined;
    depthAttach.params.finalLayout = RHI::ImageLayout::DepthStencilAttachment;
    geomSubpass.depthAttachment = depthAttach;

    geomSubpass.recorder = std::make_shared<MeshDrawRecorder>();

    // ---------- 2. 光照子通道 ----------
    SubpassConfig lightSubpass;
    lightSubpass.name = "Lighting";

    // 输入附件：Albedo, Normal, Material
    SubpassAttachment albedoInput;
    albedoInput.textureName = "Albedo";
    albedoInput.params.loadOp = RHI::AttachmentLoadOp::Load;
    albedoInput.params.initialLayout = RHI::ImageLayout::ColorAttachment;
    albedoInput.params.finalLayout = RHI::ImageLayout::ShaderReadOnly;
    lightSubpass.inputAttachments.push_back(albedoInput);

    SubpassAttachment normalInput;
    normalInput.textureName = "Normal";
    normalInput.params.loadOp = RHI::AttachmentLoadOp::Load;
    normalInput.params.initialLayout = RHI::ImageLayout::ColorAttachment;
    normalInput.params.finalLayout = RHI::ImageLayout::ShaderReadOnly;
    lightSubpass.inputAttachments.push_back(normalInput);

    SubpassAttachment materialInput;
    materialInput.textureName = "Material";
    materialInput.params.loadOp = RHI::AttachmentLoadOp::Load;
    materialInput.params.initialLayout = RHI::ImageLayout::ColorAttachment;
    materialInput.params.finalLayout = RHI::ImageLayout::ShaderReadOnly;
    lightSubpass.inputAttachments.push_back(materialInput);

    // 输出到 SceneColor
    SubpassAttachment sceneColorOutput;
    sceneColorOutput.textureName = "SceneColor";
    sceneColorOutput.params.clearColor = RHI::Color{ 0.2f, 0.3f, 0.5f, 1.0f }; 
    sceneColorOutput.params.loadOp = RHI::AttachmentLoadOp::Clear;
    sceneColorOutput.params.storeOp = RHI::AttachmentStoreOp::Store;
    sceneColorOutput.params.initialLayout = RHI::ImageLayout::Undefined;
    sceneColorOutput.params.finalLayout = RHI::ImageLayout::ColorAttachment;
    lightSubpass.colorAttachments.push_back(sceneColorOutput);

    // 光照录制器
    auto lightRecorder = std::make_shared<DeferredLightingRecorder>();
    auto lightingMaterial = createLightingMaterial(rhi->getResourceManager(), globalDescriptorData);
    lightRecorder->setLightingMaterial(lightingMaterial);
    lightSubpass.recorder = lightRecorder;

    renderPath->setLightingMaterial(lightingMaterial);

    // 固定管线描述（光照子通道）
    Scene::GraphicsPipelineState lightPSO;
    lightPSO.vertexShader = lightingMaterial->getTemplate()->getVertexShader();
    lightPSO.fragmentShader = lightingMaterial->getTemplate()->getFragmentShader();
    lightPSO.layout = lightingMaterial->getTemplate()->getPipelineLayout(rhi->getResourceManager().get());
    lightPSO.vertexInput = RHI::VertexInputState{};
    lightPSO.topology = RHI::PrimitiveTopology::TriangleList;
    lightPSO.attachments = { RHI::BlendAttachmentState{} };
    lightPSO.cullMode = RHI::CullMode::None;
    lightPSO.depthTestEnable = false;
    lightPSO.depthWriteEnable = false;
    lightSubpass.pipelineDesc = lightPSO;

    // 纹理绑定：将三个 GBuffer 纹理绑定到光照材质（set 1, binding 0,1,2）
    TextureBinding bindingAlbedo;
    bindingAlbedo.textureName = "Albedo";
    bindingAlbedo.set = 1;
    bindingAlbedo.binding = 0;
    bindingAlbedo.samplerDesc.minFilter = RHI::SamplerFilter::Linear;
    bindingAlbedo.samplerDesc.magFilter = RHI::SamplerFilter::Linear;
    lightSubpass.textureBindings.push_back(bindingAlbedo);

    TextureBinding bindingNormal;
    bindingNormal.textureName = "Normal";
    bindingNormal.set = 1;
    bindingNormal.binding = 1;
    bindingNormal.samplerDesc = bindingAlbedo.samplerDesc;
    lightSubpass.textureBindings.push_back(bindingNormal);

    TextureBinding bindingMaterial;
    bindingMaterial.textureName = "Material";
    bindingMaterial.set = 1;
    bindingMaterial.binding = 2;
    bindingMaterial.samplerDesc = bindingAlbedo.samplerDesc;
    lightSubpass.textureBindings.push_back(bindingMaterial);

    // ---------- 3. 复制子通道 ----------
    SubpassConfig copySubpass;
    copySubpass.name = "Copy";

    // 输入 SceneColor
    SubpassAttachment sceneColorInput;
    sceneColorInput.textureName = "SceneColor";
    sceneColorInput.params.loadOp = RHI::AttachmentLoadOp::Load;
    sceneColorInput.params.initialLayout = RHI::ImageLayout::ColorAttachment;
    sceneColorInput.params.finalLayout = RHI::ImageLayout::ShaderReadOnly;
    copySubpass.inputAttachments.push_back(sceneColorInput);

    // 输出到交换链
    SubpassAttachment swapchainOutput;
    swapchainOutput.textureName = "Swapchain";
    swapchainOutput.params.clearColor = RHI::Color{ 0.2f, 0.3f, 0.5f, 1.0f }; 
    swapchainOutput.params.loadOp = RHI::AttachmentLoadOp::Clear;
    swapchainOutput.params.storeOp = RHI::AttachmentStoreOp::Store;
    swapchainOutput.params.initialLayout = RHI::ImageLayout::Undefined;
    swapchainOutput.params.finalLayout = RHI::ImageLayout::PresentSrc;
    copySubpass.colorAttachments.push_back(swapchainOutput);

    auto copyMaterial = createCopyMaterial(rhi->getResourceManager(), globalDescriptorData);
    copyMaterial->setRenderStage(Scene::RenderStage::Forward);
    copyMaterial->setRenderQueue(Scene::RenderQueue::UI);
    auto copyRecorder = std::make_shared<CopyToSwapchainRecorder>();
    copyRecorder->setMaterial(copyMaterial);
    copySubpass.recorder = copyRecorder;

    Scene::GraphicsPipelineState copyPSO;
    copyPSO.vertexShader = copyMaterial->getTemplate()->getVertexShader();
    copyPSO.fragmentShader = copyMaterial->getTemplate()->getFragmentShader();
    copyPSO.layout = copyMaterial->getTemplate()->getPipelineLayout(rhi->getResourceManager().get());
    copyPSO.vertexInput = RHI::VertexInputState{};
    copyPSO.topology = RHI::PrimitiveTopology::TriangleList;
    copyPSO.attachments = { RHI::BlendAttachmentState{} };
    copyPSO.cullMode = RHI::CullMode::None;
    copyPSO.depthTestEnable = false;
    copyPSO.depthWriteEnable = false;
    copySubpass.pipelineDesc = copyPSO;

    TextureBinding bindingSceneColor;
    bindingSceneColor.textureName = "SceneColor";
    bindingSceneColor.set = 1;
    bindingSceneColor.binding = 0;
    bindingSceneColor.samplerDesc.minFilter = RHI::SamplerFilter::Linear;
    bindingSceneColor.samplerDesc.magFilter = RHI::SamplerFilter::Linear;
    copySubpass.textureBindings.push_back(bindingSceneColor);

    RenderPathConfig config;
    config[Scene::RenderStage::GBuffer][Scene::RenderQueue::Opaque] = geomSubpass;      // GBuffer
    config[Scene::RenderStage::Lighting][Scene::RenderQueue::Opaque] = lightSubpass; // Lighting
    config[Scene::RenderStage::PostProcess][Scene::RenderQueue::Opaque] = copySubpass;          // Copy
    auto depthFormat = rhi->getDepthFormat();

    renderPath->setConfig(config);

    if (!renderPath->initialize()) {
        LOG_ERROR("Failed to initialize forward render path");
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

    //auto gridMeshData = createGrid(rhi->getResourceManager(), globalDescriptorData);
    //auto gridObj = std::make_shared<Scene::RenderObject>();
    //gridObj->geometry = gridMeshData.geometry;
    //gridObj->materials = gridMeshData.materials;
    //gridObj->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.5f));
    //scene->addObject(gridObj);

    auto SphereMeshData = createSphere(rhi->getResourceManager(), globalDescriptorData);
    auto SphereObj = std::make_shared<Scene::RenderObject>();
    SphereObj->geometry = SphereMeshData.geometry;
    SphereObj->materials = SphereMeshData.materials;
    SphereObj->transform = glm::mat4(1.0f);
    scene->addObject(SphereObj);

    auto perspectiveCamera = std::make_shared<Scene::PerspectiveCamera>();
    perspectiveCamera->setPerspective(glm::radians(45.0f), (float)width / height, 0.1f, 100.0f);
    perspectiveCamera->lookAt(glm::vec3(0.0f, 2.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    scene->addCamera(perspectiveCamera);
    scene->setActiveCamera(perspectiveCamera);

    auto renderPath = createDeferredRenderPath(rhi, width, height, globalDescriptorData);
    renderer->setRenderPath(std::move(renderPath));
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
    gridMaterialInst->setRenderStage(Scene::RenderStage::Forward);
    gridMaterialInst->setRenderQueue(Scene::RenderQueue::Opaque);

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
        {0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Fragment}
    };
    auto set1Layout = Assets::DescriptorSetLayoutCache::getOrCreateLayout(resMgr.get(), materialLayoutDesc);
    LayoutMap[1] = set1Layout;

    auto SphereTmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, LayoutMap, pushConstants);
    SphereTmpl->loadShaders("assets/shaders/deferred/sphere_deferred.vert", "assets/shaders/deferred/sphere_deferred.frag");

    auto SphereMaterialInst = std::make_shared<Assets::MaterialInstance>(SphereTmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);
    SphereMaterialInst->setRenderStage(Scene::RenderStage::GBuffer);
    SphereMaterialInst->setRenderQueue(Scene::RenderQueue::Opaque);

    auto attachments = {
        RHI::BlendAttachmentState{},  // Albedo
        RHI::BlendAttachmentState{},  // Normal
        RHI::BlendAttachmentState{}   // Material
    };
    SphereMaterialInst->setAttachments(attachments);

    MaterialUniforms uniforms;
    uniforms.baseColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f); // 红色
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
        {0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment},
        {1, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment},
        {2, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment}
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

    auto material = std::make_shared<Assets::MaterialInstance>(
        tmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);

    material->setRenderStage(Scene::RenderStage::Lighting);
    material->setRenderQueue(Scene::RenderQueue::Opaque);
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

    auto material = std::make_shared<Assets::MaterialInstance>(
        tmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);

    material->setRenderStage(Scene::RenderStage::PostProcess);
    material->setRenderQueue(Scene::RenderQueue::Opaque);
    return material;
}