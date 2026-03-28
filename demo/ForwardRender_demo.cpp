#include"../src/application/Application.hpp"
#include "type.hpp"
using namespace StarryEngine;

DataSet createRenderer(std::shared_ptr<RHI::IRHI> rhi, RHI::DescriptorPoolHandle descriptorPool,uint32_t width, uint32_t height);
std::shared_ptr<ForwardRenderPath> createRenderPathConfig(std::shared_ptr<RHI::IRHI> rhi,uint32_t width, uint32_t height,const GlobalDescriptorData& globalDescriptorData);
ModelData createModel(std::shared_ptr<RHI::ResourceManager> resMgr, GlobalDescriptorData data);
ModelData createGrid(std::shared_ptr<RHI::ResourceManager> resMgr, GlobalDescriptorData data);
ModelData createSphere(std::shared_ptr<RHI::ResourceManager> resMgr, GlobalDescriptorData data);

std::shared_ptr<Assets::MaterialInstance> createCopyMaterial(
    std::shared_ptr<RHI::ResourceManager> resMgr,
    const GlobalDescriptorData& data)
{
    std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> layoutMap;
    layoutMap[0] = data.globalSetLayout; // set 0: 全局 Uniform

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
    return material;
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
    StarryEngine::Application app;

    auto dataset = createRenderer(app.getRenderHardwareInterface(), app.getGlobalDescriptorPool(), app.getWidth(), app.getHeight());

    app.setRenderer(dataset.renderer);
    app.setScene(dataset.scene);
    app.initEventDispatcher();
    app.run();
    StarryEngine::Logger::shutdown();
}

DataSet createRenderer(std::shared_ptr<RHI::IRHI> rhi, RHI::DescriptorPoolHandle descriptorPool,uint32_t width, uint32_t height) {
    auto scene = std::make_shared<Scene::Scene>();

    auto renderer = std::make_shared<Renderer>(rhi, descriptorPool, scene);
    renderer->createGlobalSetLayout();
    renderer->createGlobalUniformBuffer();

    GlobalDescriptorData globalDescriptorData;
    globalDescriptorData.globalDescriptorPool = descriptorPool;
    globalDescriptorData.globalSetLayout = renderer->getGlobalSetLayout();
    globalDescriptorData.globalDescriptorSet = renderer->getGlobalDescriptorSet();

    //auto modelMeshData = createModel(rhi->getResourceManager(), globalDescriptorData);
    //auto modelObj = std::make_shared<Scene::RenderObject>();
    //modelObj->geometry = modelMeshData.geometry;
    //modelObj->materials = modelMeshData.materials;
    //modelObj->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.5f));
    //scene->addObject(modelObj);

    auto gridMeshData = createGrid(rhi->getResourceManager(), globalDescriptorData);
    auto gridObj = std::make_shared<Scene::RenderObject>();
    gridObj->geometry = gridMeshData.geometry;
    gridObj->materials = gridMeshData.materials;
    gridObj->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.5f));
    scene->addObject(gridObj);

    //std::vector<glm::mat4> transforms;
    //for (int i = 0; i < 2; ++i) {
    //    for (int j = 0; j < 2; ++j) {
    //        glm::vec3 pos((i - 0.5f) * 3.0f, 0.5f, (j - 0.5f) * 3.0f);
    //        glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos);
    //        transforms.push_back(transform);
    //    }
    //}
    auto SphereMeshData = createSphere(rhi->getResourceManager(), globalDescriptorData);
    auto SphereObj = std::make_shared<Scene::RenderObject>();
    SphereObj->geometry = SphereMeshData.geometry;
    SphereObj->materials = SphereMeshData.materials;
    //SphereObj->instanceTransforms = transforms;
    //SphereObj->isInstanced = true;
    SphereObj->transform = glm::mat4(1.0f);
    scene->addObject(SphereObj);

    auto perspectiveCamera = std::make_shared<Scene::PerspectiveCamera>();
    perspectiveCamera->setPerspective(glm::radians(45.0f), (float)width / height, 0.1f, 100.0f);
    perspectiveCamera->lookAt(glm::vec3(0.0f, 2.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    scene->addCamera(perspectiveCamera);
    scene->setActiveCamera(perspectiveCamera);

    auto renderPath = createRenderPathConfig(rhi, width, height,globalDescriptorData);
    renderer->setRenderPath(std::move(renderPath));

    return DataSet(scene, renderer);
}

std::shared_ptr<ForwardRenderPath> createRenderPathConfig(
    std::shared_ptr<RHI::IRHI> rhi,
    uint32_t width, uint32_t height,
    const GlobalDescriptorData& globalDescriptorData)
{
    auto renderPath = std::make_shared<ForwardRenderPath>(rhi, width, height);
    std::unordered_map<std::string, RHI::TextureDesc> textureDescs;
    
    RHI::TextureDesc colorDesc;
    colorDesc.extent = { width, height, 1 };
    colorDesc.format = RHI::Format::RGBA8_UNorm;
    colorDesc.type = RHI::TextureType::Texture2D;
    colorDesc.allowRenderTarget = true;
    colorDesc.allowInputAttachment = true;   // 关键：作为输入附件
    textureDescs["SceneColor"] = colorDesc;
    
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
    
    SubpassAttachment colorAttach;
    colorAttach.textureName = "SceneColor";
    colorAttach.params.clearColor = RHI::Color{ 0.05f, 0.05f, 0.05f, 1.0f };
    colorAttach.params.loadOp = RHI::AttachmentLoadOp::Clear;      
    colorAttach.params.storeOp = RHI::AttachmentStoreOp::Store;
    colorAttach.params.initialLayout = RHI::ImageLayout::Undefined; 
    colorAttach.params.finalLayout = RHI::ImageLayout::ColorAttachment; 
    geomSubpass.colorAttachments.push_back(colorAttach);
    
    SubpassAttachment depthAttach;
    depthAttach.textureName = "Depth";
    depthAttach.params.clearDepth = 1.0f;
    depthAttach.params.loadOp = RHI::AttachmentLoadOp::Clear;
    depthAttach.params.storeOp = RHI::AttachmentStoreOp::DontCare;
    depthAttach.params.initialLayout = RHI::ImageLayout::Undefined;
    depthAttach.params.finalLayout = RHI::ImageLayout::DepthStencilAttachment;
    geomSubpass.depthAttachment = depthAttach;
    
    geomSubpass.recorder = std::make_shared<MeshDrawRecorder>();

    // ---------- 虚拟子通道 (Lighting stage) ----------
    SubpassConfig dummySubpass;
    dummySubpass.name = "Dummy";

    SubpassAttachment dummyInput;
    dummyInput.textureName = "SceneColor";
    dummyInput.params.loadOp = RHI::AttachmentLoadOp::Load;
    dummyInput.params.initialLayout = RHI::ImageLayout::ShaderReadOnly;
    dummyInput.params.finalLayout = RHI::ImageLayout::ShaderReadOnly;
    dummySubpass.inputAttachments.push_back(dummyInput);

    // 虚拟子通道不需要绘制，但必须有一个有效的录制器（可以是一个空实现）
    class DummyRecorder : public ISubpassRecorder {
    public:
        void recordCommands(RHI::RHICommandEncoder* encoder,
            const RenderContext& rctx,
            const PassContext& pctx,
            uint32_t subpassIndex) override {
        }
        // 其他方法空实现
        void clearDrawItems() override {}
        const std::vector<std::shared_ptr<Scene::DrawItem>>& getDrawItems() override { static std::vector<std::shared_ptr<Scene::DrawItem>> empty; return empty; }
        void setPipelines(const std::vector<RHI::PipelineHandle>&) override {}
        void setDrawItems(const std::vector<std::shared_ptr<Scene::DrawItem>>&) override {}
    };
    dummySubpass.recorder = std::make_shared<DummyRecorder>();

    // ---------- 复制子通道 (Forward stage) ----------
    SubpassConfig copySubpass;
    copySubpass.name = "Copy";

    SubpassAttachment copyInput;
    copyInput.textureName = "SceneColor";
    copyInput.params.loadOp = RHI::AttachmentLoadOp::Load;
    copyInput.params.initialLayout = RHI::ImageLayout::ColorAttachment;
    copyInput.params.finalLayout = RHI::ImageLayout::ShaderReadOnly;
    copySubpass.inputAttachments.push_back(copyInput);

    SubpassAttachment outputSwapchain;
    outputSwapchain.textureName = "Swapchain";
    outputSwapchain.params.clearColor = RHI::Color{ 0.0f, 0.0f, 0.0f, 1.0f };
    outputSwapchain.params.loadOp = RHI::AttachmentLoadOp::Clear;
    outputSwapchain.params.storeOp = RHI::AttachmentStoreOp::Store;
    outputSwapchain.params.initialLayout = RHI::ImageLayout::Undefined;
    outputSwapchain.params.finalLayout = RHI::ImageLayout::PresentSrc;
    copySubpass.colorAttachments.push_back(outputSwapchain);

    auto copyMaterial = createCopyMaterial(rhi->getResourceManager(), globalDescriptorData);
    auto copyRecorder = std::make_shared<CopyToSwapchainRecorder>();
    copyRecorder->setMaterial(copyMaterial);
    copySubpass.recorder = copyRecorder;

    Scene::GraphicsPipelineState pso;
    pso.vertexShader = copyMaterial->getTemplate()->getVertexShader();
    pso.fragmentShader = copyMaterial->getTemplate()->getFragmentShader();
    pso.layout = copyMaterial->getTemplate()->getPipelineLayout(rhi->getResourceManager().get());
    pso.vertexInput = RHI::VertexInputState{};
    pso.topology = RHI::PrimitiveTopology::TriangleList;
    pso.attachments = { RHI::BlendAttachmentState{} };
    pso.cullMode = RHI::CullMode::None;
    pso.depthTestEnable = false;
    pso.depthWriteEnable = false;
    copySubpass.pipelineDesc = pso;

    TextureBinding binding;
    binding.textureName = "SceneColor";
    binding.set = 1;
    binding.binding = 0;
    binding.samplerDesc.minFilter = RHI::SamplerFilter::Linear;
    binding.samplerDesc.magFilter = RHI::SamplerFilter::Linear;
    copySubpass.textureBindings.push_back(binding);

    // 配置：Forward stage 包含几何子通道和复制子通道（同一 RenderPass），Lighting stage 包含虚拟子通道（独立 RenderPass）
    RenderPathConfig config;
    config[Scene::RenderStage::Forward][Scene::RenderQueue::Opaque] = geomSubpass;   // 几何子通道
    config[Scene::RenderStage::PostProcess][Scene::RenderQueue::Opaque] = copySubpass;       // 复制子通道（同一 RenderPass）
    config[Scene::RenderStage::Lighting][Scene::RenderQueue::Opaque] = dummySubpass; // 额外独立 RenderPass

    renderPath->setConfig(config);

    if (!renderPath->initialize()) {
        LOG_ERROR("Failed to initialize forward render path");
    }

    return renderPath;
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
        std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> layoutMap;
        layoutMap[0] = data.globalSetLayout;

        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            {0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment},
            {1, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment}
        };
        layoutMap[1] = Assets::DescriptorSetLayoutCache::getOrCreateLayout(resMgr.get(), layoutDesc);

        std::string fsPath;
        if (param.name == "body") fsPath = "assets/shaders/core/shader.frag";
        else if (param.name == "brow") fsPath = "assets/shaders/core/shader.frag";
        else if (param.name == "eyes") fsPath = "assets/shaders/core/shader.frag";
        else if (param.name == "face") fsPath = "assets/shaders/core/face.frag";
        else fsPath = "assets/shaders/core/hair.frag";

        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, layoutMap, pushConstants);
        if (!tmpl->loadShaders("assets/shaders/core/shader.vert", fsPath)) {
            LOG_ERROR("Failed to load shaders for material: {}", param.name);
            continue;
        }

        auto instance = std::make_shared<Assets::MaterialInstance>(tmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);

        Assets::MaterialUniforms uniforms{};
        instance->setUniform(1, 0, &uniforms, sizeof(Assets::MaterialUniforms));

        if (!param.albedoTexture.empty()) {
            Assets::TextureLoader loader(resMgr);
            auto texResult = loader.loadTexture2D(param.albedoTexture, RHI::Format::RGBA8_UNorm, "Albedo");
            if (texResult.texture.isValid()) {
                instance->setTexture(1, 1, texResult.texture, texResult.sampler);
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

        materialInstances.push_back(instance);
    }

    return ModelData(geometry, materialInstances);
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

    auto SphereTmpl = std::make_shared<Assets::DefaultMaterialTemplate>(resMgr, LayoutMap, pushConstants);
    SphereTmpl->loadShaders("assets/shaders/core/sphere.vert", "assets/shaders/core/sphere.frag");

    auto SphereMaterialInst = std::make_shared<Assets::MaterialInstance>(SphereTmpl, data.globalDescriptorPool, resMgr.get(), data.globalDescriptorSet);
    SphereMaterialInst->setRenderStage(Scene::RenderStage::Forward);
    SphereMaterialInst->setRenderQueue(Scene::RenderQueue::Opaque);

    return ModelData(Assets::Shape::createSphereGeometry(resMgr, 1.0f, 50, 20), { SphereMaterialInst });
}