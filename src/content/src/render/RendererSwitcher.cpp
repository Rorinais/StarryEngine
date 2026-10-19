#include <content/render/RendererSwitcher.hpp>
#include <content/scenes/pbr/PbrScene.hpp>
#include <content/scenes/raytrace/RayTraceScene.hpp>
#include <app/RuntimeSettings.hpp>
#include <app/Diagnostics.hpp>
#include <application/Application.hpp>
#include <logging/Logger.hpp>

namespace StarryEngine::Content {

static std::shared_ptr<PBRDemo> g_pbr;
static std::shared_ptr<RayTracingDemo> g_rt;
static StarryEngine::App::Diagnostics g_diag;

bool RendererSwitcher::install(StarryEngine::Application& app, const StarryEngine::App::AppConfig& cfg) {
    auto& S = StarryEngine::App::runtimeSettings();
    // 命令行帧捕获 → 运行时开关（替代原环境变量）
    S.frameDump   = cfg.dumpFrames;
    S.frameDumpDir = cfg.dumpDir;
    S.frameCount  = cfg.dumpCount;
    S.frameIndex  = cfg.dumpIndex;
    S.frameExit   = cfg.exitAfterDump;
    const bool useRayTracing = (cfg.sceneId == "raytrace") || S.useRayTracing;

    g_diag.reset();

    if (useRayTracing) {
        g_rt = std::make_shared<RayTracingDemo>(app.getRenderHardwareInterface(),
            app.getGlobalDescriptorPool(), app.getWidth(), app.getHeight());
        g_rt->setExitCallback([&app]() { app.shutdown(); });
        app.setRenderer(g_rt->getRenderer());
        app.setScene(g_rt->getScene());
        app.setPostRenderCallback([&app]() {
            g_rt->offlineCapture();
            g_diag.onFrame(*app.getRenderHardwareInterface(), [&app]() { app.shutdown(); });
        });
        app.setUpdateCallback([&app](float dt) {
            g_rt->updateTitle(app.getWindow()->getHandle(), dt);
            g_diag.onFrame(*app.getRenderHardwareInterface(), [&app]() { app.shutdown(); });
        });
    }
    else {
        g_pbr = std::make_shared<PBRDemo>(app.getRenderHardwareInterface(),
            app.getGlobalDescriptorPool(), app.getWidth(), app.getHeight());
        g_pbr->setExitCallback([&app]() { app.shutdown(); });
        app.setRenderer(g_pbr->getRenderer());
        app.setScene(g_pbr->getScene());
        app.setPostRenderCallback([&app]() {
            g_pbr->captureFrame();
        });
        app.setUpdateCallback([&app](float dt) {
            g_pbr->updateTitle(app.getWindow()->getHandle(), dt);
            g_pbr->onUpdate(dt);
            g_diag.onFrame(*app.getRenderHardwareInterface(), [&app]() { app.shutdown(); });
        });
    }

    LOG_INFO("[content] 场景已装配: {}", useRayTracing ? "raytrace" : "pbr");
    return true;
}

} // namespace StarryEngine::Content
