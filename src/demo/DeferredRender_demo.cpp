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

    textureDescs["Albedo"] = {
        .extent = { width, height, 1 },
        .format = RHI::Format::RGBA8_UNorm,
        .type = RHI::TextureType::Texture2D,
        .allowRenderTarget = true,
        .allowInputAttachment = true
    };

    textureDescs["Material"] = {
        .extent = { width, height, 1 },
        .format = RHI::Format::RGBA8_UNorm,
        .type = RHI::TextureType::Texture2D,
        .allowRenderTarget = true,
        .allowInputAttachment = true
    };

    textureDescs["SceneColor"] = {
        .extent = { width, height, 1 },
        .format = RHI::Format::RGBA8_UNorm,
        .type = RHI::TextureType::Texture2D,
        .allowRenderTarget = true,
        .allowInputAttachment = true
    };

    textureDescs["Normal"] = {
        .extent = { width, height, 1 },
        .format = RHI::Format::RGBA16_Float,
        .type = RHI::TextureType::Texture2D,
        .allowRenderTarget = true,
        .allowInputAttachment = true
    };

    textureDescs["Depth"] = {
        .extent = { width, height, 1 },
        .format = rhi->getDepthFormat(),
        .type = RHI::TextureType::Texture2D,
        .allowRenderTarget = false,
        .allowDepthStencil = true,
        .allowInputAttachment = true
    };

    textureDescs["Swapchain"] = {
        .extent = { width, height, 1 },
        .format = RHI::Format::BGRA8_sRGB,
        .type = RHI::TextureType::Texture2D,
        .allowRenderTarget = true,
        .allowDepthStencil = true
    };
    renderPath->setTextureDescs(textureDescs);

    // ---------- 几何子通道 ----------
    RenderGraph::AttachmentParams geomColorAttachments{
        .clearColor = RHI::Color{ 0.0f, 0.0f, 0.0f, 0.0f },
        .loadOp = RHI::AttachmentLoadOp::Clear,
        .storeOp = RHI::AttachmentStoreOp::Store,
        .initialLayout = RHI::ImageLayout::Undefined,
        .finalLayout = RHI::ImageLayout::ColorAttachment,
    };
    RenderGraph::AttachmentParams geomDepthAttachment{
        .clearDepth = 1.0f,
        .loadOp = RHI::AttachmentLoadOp::Clear,
        .storeOp = RHI::AttachmentStoreOp::DontCare,
        .initialLayout = RHI::ImageLayout::Undefined,
        .finalLayout = RHI::ImageLayout::DepthStencilAttachment,
    };

    // ---------- 几何子通道 ----------
    SubpassConfig geomSubpass = {
        .name = "Geometry",
        .colorAttachments = {
            { "Albedo",geomColorAttachments },
            { "Normal",geomColorAttachments },
            { "Material",geomColorAttachments },
        },
        .depthAttachment = {
            { "Depth",geomDepthAttachment }
        },
        .recorder = std::make_shared<MeshDrawRecorder>()
    };

    // ---------- 2. 光照子通道 ----------
    RenderGraph::AttachmentParams lightColorAttachment{
        .clearColor = RHI::Color{ 0.0f, 0.0f, 0.0f, 0.0f },
        .loadOp = RHI::AttachmentLoadOp::Clear,        // ✅ 第一次写入，必须 Clear
        .storeOp = RHI::AttachmentStoreOp::Store,
        .initialLayout = RHI::ImageLayout::Undefined,  // 首次使用
        .finalLayout = RHI::ImageLayout::ColorAttachment,
    };

    RenderGraph::AttachmentParams lightInputAttachment{
        .loadOp = RHI::AttachmentLoadOp::Load,
        .storeOp = RHI::AttachmentStoreOp::DontCare,
        .initialLayout = RHI::ImageLayout::ColorAttachment,
        .finalLayout = RHI::ImageLayout::ShaderReadOnly,
    };

    SubpassConfig lightSubpass = {
        .name = "Lighting",
        .colorAttachments{
            { "SceneColor",lightColorAttachment },
        },
        .inputAttachments = {
            { "Albedo",lightInputAttachment },
            { "Normal",lightInputAttachment },
            { "Material",lightInputAttachment },
        },
        .recorder = std::make_shared<DeferredLightingRecorder>()

    };
    auto lightingMaterial = createLightingMaterial(rhi->getResourceManager(), globalDescriptorData);
    lightSubpass.recorder->setMaterial(lightingMaterial);
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
    RenderGraph::AttachmentParams copyInputAttachment{
        .loadOp = RHI::AttachmentLoadOp::Load,
        .storeOp = RHI::AttachmentStoreOp::DontCare,
        .initialLayout = RHI::ImageLayout::ColorAttachment,
        .finalLayout = RHI::ImageLayout::ShaderReadOnly,
    };

    RenderGraph::AttachmentParams copyColorAttachment{
        .clearColor = RHI::Color{ 0.2f, 0.3f, 0.5f, 1.0f },
        .loadOp = RHI::AttachmentLoadOp::Clear,
        .storeOp = RHI::AttachmentStoreOp::Store,
        .initialLayout = RHI::ImageLayout::Undefined,
        .finalLayout = RHI::ImageLayout::PresentSrc,
    };

    SubpassConfig copySubpass = {
        .name = "Copy",
        .colorAttachments = { { "Swapchain", copyColorAttachment } },
        .inputAttachments = { { "SceneColor", copyInputAttachment } },
        .recorder = std::make_shared<CopyToSwapchainRecorder>()
    };

    auto copyMaterial = createCopyMaterial(rhi->getResourceManager(), globalDescriptorData);
    copySubpass.recorder->setMaterial(copyMaterial);

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

    RenderGraph::AttachmentParams gridColorAttachment{
        .clearColor = RHI::Color{ 1.0f, 0.1f, 0.1f, 1.0f },  // 测试用红色
        .loadOp = RHI::AttachmentLoadOp::Load,                // ✅ 加载光照写入的颜色
        .storeOp = RHI::AttachmentStoreOp::Store,
        .initialLayout = RHI::ImageLayout::ColorAttachment,   // ✅ 匹配光照 finalLayout
        .finalLayout = RHI::ImageLayout::ColorAttachment,
    };

    RenderGraph::AttachmentParams gridDepthAttachment{
    .loadOp = RHI::AttachmentLoadOp::Load,
    .storeOp = RHI::AttachmentStoreOp::DontCare,
    .initialLayout = RHI::ImageLayout::DepthStencilAttachment,
    .finalLayout = RHI::ImageLayout::DepthStencilAttachment,
    };

    SubpassConfig gridSubpass = {
        .name = "Grid",
        .colorAttachments = { { "SceneColor", gridColorAttachment } },
        .depthAttachment = { { "Depth", gridDepthAttachment } },
        .recorder = std::make_shared<TestRecorder>()
    };

    RenderPathConfig config;
    config[Scene::RenderStage::GBuffer][Scene::RenderQueue::Opaque] = geomSubpass;          // 几何
    config[Scene::RenderStage::Lighting][Scene::RenderQueue::Opaque] = lightSubpass;        // 光照
    config[Scene::RenderStage::PostProcess][Scene::RenderQueue::Opaque] = gridSubpass;      // 网格（后处理阶段）
    config[Scene::RenderStage::PostProcess][Scene::RenderQueue::Transparent] = copySubpass;

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
    gridObj->transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 2.5)), glm::vec3(5, 5, 1));
    scene->addObject(gridObj);


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
    gridMaterialInst->setRenderStage(Scene::RenderStage::PostProcess);
    gridMaterialInst->setRenderQueue(Scene::RenderQueue::Opaque);
    gridMaterialInst->enableDepthTest(false);
    gridMaterialInst->enableDepthWrite(false);
    gridMaterialInst->setCullMode(RHI::CullMode::None);

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
    SphereMaterialInst->enableDepthTest(true);
    SphereMaterialInst->enableDepthWrite(true);

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

    auto material = std::make_shared<Assets::MaterialInstance>(
        tmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);

    material->setRenderStage(Scene::RenderStage::PostProcess);
    material->setRenderQueue(Scene::RenderQueue::Opaque);
    return material;
}