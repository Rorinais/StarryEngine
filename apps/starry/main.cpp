#include <renderer/passes/PassWrapper.hpp>
#include <app/AppConfig.hpp>
#include <app/RuntimeSettings.hpp>
#include <renderer/passes/GraphicsPass.hpp>
#include <renderer/passes/MeshPass.hpp>
#include <renderer/passes/ParticlePass.hpp>
#include <renderer/passes/ShadowPass.hpp>
#include <renderer/passes/ComputePass.hpp>
#include <renderer/RasterRenderer.hpp>
#include <content/scenes/raytrace/RayTraceScene.hpp>
#include <content/render/RendererSwitcher.hpp>
#include <content/scenes/pbr/PbrScene.hpp>
#include <scene/ParticleEmitter.hpp>
#include <renderer/renderPaths/DeferredRenderPath.hpp>
#include <renderer/passExecutor/SceneDrawExecutor.hpp>
#include <assets/geometry/GeometryGenerator.hpp>

#include <application/Application.hpp>
#include <core/JobSystem.hpp>
#include <event/Events.hpp>
#include <renderer/interface/vulkan/VulkanFrameContext.hpp>
#include <renderer/utils/FrameCapture.hpp>
#include <content/scenes/pbr/AsyncBoneProducer.hpp>

#include <stb_image_write.h>
#include <glm/gtc/packing.hpp>
#include <limits>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static bool g_clickThrough = true;


int main(int argc, char** argv) {
    const auto appConfig = StarryEngine::App::AppConfig::load(argc, argv);
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
    cfg.width = appConfig.window.width;
    cfg.height = appConfig.window.height;
    cfg.title = "Transparent Window Smoke Test";

    StarryEngine::Application app(cfg);
    auto perfLast = std::chrono::steady_clock::now();
    uint64_t perfLastFrames = 0;

    static std::shared_ptr<RayTracingDemo> rtDemo;
    static std::shared_ptr<PBRDemo> pbrDemo;


    // 场景/渲染器切换交给内容层（RendererSwitcher）；设备丢失恢复时用同一入口重建
    StarryEngine::Content::RendererSwitcher::install(app, appConfig);
    app.setWorldFactory([&app, &appConfig]() { StarryEngine::Content::RendererSwitcher::install(app, appConfig); });
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