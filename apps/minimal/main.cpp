// ────────────────────────────────────────────────────────────────
// 最小外部应用：main 只做连接。
//   场景   → SceneBuilder（默认自带天空盒 + 网格，可继续加物体/相机）
//   渲染器 → RasterRenderer（构造自包含：布局/UBO/默认材质）
//   路径   → RenderPathRegistry 映射表按名创建（默认只登记，不默认创建）
// ────────────────────────────────────────────────────────────────
#include <app/AppConfig.hpp>
#include <application/Application.hpp>
#include <renderer/RasterRenderer.hpp>
#include <scene/Scene.hpp>
#include <content/creation/SceneBuilder.hpp>
#include <content/creation/RenderPathRegistry.hpp>
#include <renderer/utils/FrameCapture.hpp>
#include <logging/Logger.hpp>

int main(int argc, char** argv) {
    StarryEngine::Logger::init();
    StarryEngine::Logger::setLevel("info");

    const auto cfg = StarryEngine::App::AppConfig::load(argc, argv);

    StarryEngine::Application::Config appCfg;
    appCfg.width  = cfg.window.width;
    appCfg.height = cfg.window.height;
    appCfg.title  = "StarryEngine Minimal";

    StarryEngine::Application app(appCfg);
    auto rhi  = app.getRenderHardwareInterface();
    auto pool = app.getGlobalDescriptorPool();
    if (!rhi) { LOG_ERROR("RHI 创建失败"); return 1; }

    // 1) 场景：渲染器先持有场景，再由 SceneBuilder 填充基础件与内容
    auto scene    = std::make_shared<StarryEngine::Scene::Scene>();
    auto renderer = std::make_shared<StarryEngine::RasterRenderer>(rhi, pool, scene);

    StarryEngine::Content::SceneResources res{ rhi, pool,
        renderer->getGlobalSetLayout(), renderer->getGlobalDescriptorSet(0) };
    StarryEngine::Content::SceneBuilder(scene, res)
        .addSkybox()
        .addGrid(10.0f, 10)
        .addCamera({ 45.0f, (float)app.getWidth() / app.getHeight(), 0.1f, 100.0f,
                     {0.0f, 1.2f, 4.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} })
        .build();

    // 2) 渲染路径：映射表按名创建（"deferred" / "raytrace" / 自定义名）
    auto renderPath = StarryEngine::Content::RenderPathRegistry::instance().create(
        "deferred", rhi, app.getWidth(), app.getHeight());

    // 3) 一步注册 + 跑循环
    app.setRenderer(renderer, renderPath);
    app.setScene(scene);
    app.initEventDispatcher();

    // 可选：--dump <dir> 截图（调试用）
    std::shared_ptr<StarryEngine::FrameCapture> capture;
    if (cfg.dumpFrames && !cfg.dumpDir.empty()) {
        capture = std::make_shared<StarryEngine::FrameCapture>(rhi);
        capture->initialize({ cfg.dumpDir.c_str(), cfg.dumpCount,
            cfg.dumpIndex < 0 ? UINT32_MAX : static_cast<uint32_t>(cfg.dumpIndex) });
        app.setPostRenderCallback([&]() {
            if (!capture || !renderPath) return;
            auto graph = renderPath->getRenderGraph();
            if (!graph) return;
            capture->capture(rhi->getResourceManager()->getTexture(
                graph->getPhysicalTextureHandle(graph->getTextureId("SceneColor"))));
        });
    }

    LOG_INFO("[minimal] main 只做连接：场景/渲染器/路径 分别由创建层装配");
    app.run();
    StarryEngine::Logger::shutdown();
    return 0;
}
