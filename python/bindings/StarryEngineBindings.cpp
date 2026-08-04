#include <pybind11/pybind11.h>

#include <memory>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../../src/application/Application.hpp"
#include "../../src/renderer/Renderer.hpp"
#include "../../src/scene/Scene.hpp"
#include "../../src/scene/camera/PerspectiveCamera.hpp"
#include "../../src/renderer/renderPaths/DeferredRenderPath.hpp"
#include "../../src/renderer/passes/GraphicsPass.hpp"
#include "../../src/renderer/passes/PassWrapper.hpp"
#include "../../src/renderer/passExecutor/MeshDrawExecutor.hpp"
#include "../../src/assets/geometry/GeometryGenerator.hpp"

namespace py = pybind11;

namespace {

std::unique_ptr<StarryEngine::Application> g_app;

void ensureInitialized() {
    if (!g_app) {
        throw std::runtime_error("StarryEngine not initialized: call initialize() first");
    }
}

// 默认可见场景：相机 + 球体 + 地面，Deferred 渲染路径只带一个 ForwardPass
void wireDefaultScene(StarryEngine::Application& app) {
    auto rhi = app.getRenderHardwareInterface();
    auto resMgr = rhi->getResourceManager();
    auto pool = app.getGlobalDescriptorPool();

    auto scene = std::make_shared<StarryEngine::Scene::Scene>();

    auto renderer = std::make_shared<StarryEngine::Renderer>(rhi, pool, scene);
    renderer->createGlobalSetLayout();
    renderer->createGlobalUniformBuffer();
    renderer->initDefaultMaterials();
    app.setRenderer(renderer);
    app.setScene(scene);

    // 相机
    auto camera = std::make_shared<StarryEngine::Scene::PerspectiveCamera>();
    camera->setPerspective(glm::radians(45.0f), (float)app.getWidth() / (float)app.getHeight(), 0.1f, 100.0f);
    camera->lookAt(glm::vec3(1.0f, 2.0f, 5.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    scene->addCamera(camera);
    scene->setActiveCamera(camera);

    // 默认材质（mvp.vert + default.frag，无 IBL）
    auto mat = StarryEngine::Assets::MaterialInstance::createDefault(
        resMgr, renderer->getGlobalSetLayout(), pool, renderer->getGlobalDescriptorSet());
    if (!mat) {
        throw std::runtime_error("Failed to create default material");
    }
    mat->enableDepthTest(true);
    mat->enableDepthWrite(true);

    // 球体
    auto sphere = std::make_shared<StarryEngine::Scene::RenderObject>();
    sphere->geometry = StarryEngine::Assets::GeometryGenerator::createSphere(resMgr, 1.0f);
    sphere->materials = { mat };
    sphere->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.2f, 0.0f));
    scene->addObject(sphere);

    // 地面
    auto ground = std::make_shared<StarryEngine::Scene::RenderObject>();
    ground->geometry = StarryEngine::Assets::GeometryGenerator::createCube(resMgr, 8.0f, 0.3f, 8.0f);
    ground->materials = { mat };
    ground->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.15f, 0.0f));
    scene->addObject(ground);

    // Deferred 渲染路径：ForwardPass 画到 SceneColor/Depth，BaseRenderPath 自动呈现到 Swapchain
    auto renderPath = std::make_shared<StarryEngine::DeferredRenderPath>(rhi, app.getWidth(), app.getHeight());
    renderPath->addTextureDesc("SceneColor",
        StarryEngine::PassWrapper::createColorTextureDesc({app.getWidth(), app.getHeight(), 1}, StarryEngine::RHI::Format::RGBA16_Float));
    renderPath->addTextureDesc("Depth",
        StarryEngine::PassWrapper::createDepthTextureDesc({app.getWidth(), app.getHeight(), 1}, StarryEngine::RHI::Format::D32_Float));
    renderPath->addTextureDesc("Swapchain",
        StarryEngine::PassWrapper::createColorTextureDesc({app.getWidth(), app.getHeight(), 1}, StarryEngine::RHI::Format::BGRA8_sRGB));

    auto forwardPass = std::make_shared<StarryEngine::GraphicsPass>("ForwardPass");
    {
        StarryEngine::SubpassDesc sp;
        sp.name = "OpaqueGeometry";
        sp.tag = "Forward_Opaque";
        sp.executor = std::make_shared<StarryEngine::MeshDrawExecutor>();

        StarryEngine::RenderGraph::AttachmentParams color;
        color.initialLayout = StarryEngine::RHI::ImageLayout::Undefined;
        color.finalLayout   = StarryEngine::RHI::ImageLayout::ShaderReadOnly;
        color.loadOp = StarryEngine::RHI::AttachmentLoadOp::Clear;
        color.storeOp = StarryEngine::RHI::AttachmentStoreOp::Store;
        sp.colorAttachments.push_back({"SceneColor", color});

        StarryEngine::RenderGraph::AttachmentParams depth;
        depth.initialLayout = StarryEngine::RHI::ImageLayout::Undefined;
        depth.finalLayout   = StarryEngine::RHI::ImageLayout::DepthStencilAttachment;
        depth.loadOp = StarryEngine::RHI::AttachmentLoadOp::Clear;
        depth.storeOp = StarryEngine::RHI::AttachmentStoreOp::Store;
        sp.depthAttachment = {"Depth", depth};

        forwardPass->addSubpass(sp);
    }

    StarryEngine::PassList passes;
    passes.push_back(forwardPass);
    renderPath->setPassList(std::move(passes));

    renderer->setRenderPath(std::move(renderPath));
}

} // namespace

// 阻塞式运行整个引擎（旧接口，保留兼容）
void run_engine() {
    StarryEngine::Logger::init();
    StarryEngine::Application app;
    wireDefaultScene(app);
    app.initEventDispatcher();
    app.run();
}

// 创建引擎并完成一次性初始化（窗口 / RHI / 渲染器 / 默认场景），之后可用 step() 逐帧驱动
void engine_initialize() {
    if (g_app) {
        throw std::runtime_error("StarryEngine already initialized: call shutdown() first");
    }

    StarryEngine::Logger::init();

    auto app = std::make_unique<StarryEngine::Application>();
    wireDefaultScene(*app);
    app->initEventDispatcher();
    app->initialize();

    g_app = std::move(app);
}

// 推进一帧渲染
void engine_step() {
    ensureInitialized();
    g_app->step();
}

// 请求关闭引擎并释放资源
void engine_shutdown() {
    ensureInitialized();
    g_app->shutdown();
    g_app.reset();
}

// 引擎窗口是否仍打开
bool engine_is_running() {
    return g_app && g_app->isWindowOpen();
}

PYBIND11_MODULE(starryengine_py, m) {
    m.doc() = "StarryEngine Python bindings";

    m.def("run_engine", &run_engine, "阻塞式启动引擎主循环（直到窗口关闭）");

    m.def("initialize", &engine_initialize, "创建引擎并完成一次性初始化（窗口/RHI/渲染器/默认场景）");
    m.def("step", &engine_step, "推进一帧渲染");
    m.def("shutdown", &engine_shutdown, "请求关闭引擎并释放资源");
    m.def("is_running", &engine_is_running, "引擎窗口是否仍打开");
}
