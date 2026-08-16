// ═══════════════════════════════════════════════════════════════
//  透明窗口冒烟测试（虚拟桌面角色 第 0 步）
//
//  验证目标：Linux 桌面上逐像素 alpha 合成是否真能跑通。
//  表现：透明窗口里一个不透明红方块 + 一个半透明蓝方块，
//        窗口其余部分全透明 —— 桌面应该从窗口透过来。
//
//  如果看到黑底（而不是桌面），说明 swapchain 回退到了 OPAQUE
//  （日志会有 "表面不支持 alpha 合成" 提示），或渲染 alpha 没写对。
//
//  运行（仓库根）：build/bin/TransparentWindow_demo
// ═══════════════════════════════════════════════════════════════
//
//  运行模式：
//    ./build/bin/TransparentWindow_demo              # 默认：Wayland 会话走原生 Wayland
//    STARRY_FORCE_X11=1 ./build/bin/TransparentWindow_demo
//        # 强制 X11 平台（验证 X11 代码路径；XWayland 下不透明，真 Xorg 会话才透明）
//    F1 切换点击穿透；穿透时保留窗口顶部一条交互把手
// ═══════════════════════════════════════════════════════════════
#include <cstdlib>
#include <renderer/passes/PassWrapper.hpp>
#include <renderer/passes/GraphicsPass.hpp>
#include <renderer/passes/MeshPass.hpp>
#include <renderer/renderPaths/DeferredRenderPath.hpp>
#include <renderer/passExecutor/SceneDrawExecutor.hpp>
#include <assets/geometry/GeometryGenerator.hpp>
#include <assets/material/DefaultMaterialTemplate.hpp>
#include <renderer/passes/ParticlePass.hpp>
#include <scene/ParticleEmitter.hpp>
#include <assets/material/MaterialInstance.hpp>
#include <assets/loader/TextureLoader.hpp>


#include <application/Application.hpp>
#include <event/Events.hpp>

using namespace StarryEngine;

// 点击穿透开关状态（F1 切换）。穿透 = 透明区不挡点击；关闭 = 窗口可正常交互
static bool g_clickThrough = true;

namespace {
    constexpr uint32_t kWinW = 560;
    constexpr uint32_t kWinH = 680;
}

class TransparentWindowDemo {
public:
    TransparentWindowDemo(std::shared_ptr<RHI::IRHI> rhi, RHI::DescriptorPoolHandle pool,
                          uint32_t width, uint32_t height)
        : m_rhi(rhi), m_descriptorPool(pool), m_width(width), m_height(height) {
        createRenderer();
        createScene();
    }

    std::shared_ptr<Renderer> getRenderer() { return m_renderer; }
    std::shared_ptr<Scene::Scene> getScene() { return m_scene; }

private:
    // ── 渲染路径：单 ForwardPass → SceneColor(alpha=0) → 合成到 swapchain ──
    void createRenderer() {
        m_scene = std::make_shared<Scene::Scene>();

        m_renderer = std::make_shared<Renderer>(m_rhi, m_descriptorPool, m_scene);
        m_renderer->createGlobalSetLayout();
        m_renderer->createGlobalUniformBuffer();
        m_renderer->initDefaultMaterials();

        m_descriptorSetLayout = m_renderer->getGlobalSetLayout();
        m_descriptorSet = m_renderer->getGlobalDescriptorSet(0);   // 构造注入的 globalSet 是废码，取槽 0 句柄

        auto renderPath = std::make_shared<DeferredRenderPath>(m_rhi, m_width, m_height);
        renderPath->setScene(m_scene.get());   // 场景数据源：粒子等 pass 建图时通过 configure 拿到
        // 透明窗口：present 阶段清屏为全透明
        renderPath->setPresentClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });

        auto colorDesc = PassWrapper::createColorTextureDesc({m_width, m_height, 1}, RHI::Format::RGBA16_Float);
        auto depthDesc = PassWrapper::createDepthTextureDesc({m_width, m_height, 1}, RHI::Format::D32_Float);
        auto swapDesc  = PassWrapper::createColorTextureDesc({m_width, m_height, 1}, RHI::Format::BGRA8_sRGB);
        renderPath->addTextureDesc("SceneColor", colorDesc);
        renderPath->addTextureDesc("Depth",      depthDesc);
        renderPath->addTextureDesc("Swapchain",  swapDesc);

        PassList passes;

        // ForwardPass —— MeshPass 封装，透明窗口清屏色 alpha=0（背景全透明）
        passes.push_back(std::make_shared<MeshPass>("ForwardPass", "Forward_Opaque",
                                                    RHI::Color{ 0.0f, 0.0f, 0.0f, 0.0f }));

        // 粒子 pass：声明式，场景里 passTag="Particles" 的 emitters 都归它（内容来自场景）
        passes.push_back(std::make_shared<ParticlePass>("Particles", "Particles"));

        renderPath->setPassList(std::move(passes));
        m_renderer->setRenderPath(std::move(renderPath));
    }

    // ── 纯色材质：RGBA 纹理决定颜色；translucent=true 时开 alpha 混合 ──
    std::shared_ptr<Assets::MaterialInstance> makeSolidMaterial(
        const std::array<uint8_t, 4>& rgba, bool translucent) {

        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(
            m_rhi->getResourceManager(), m_descriptorSetLayout);
        if (!tmpl->loadShaders("assets/shaders/core/shader.vert", "assets/shaders/core/shader.frag")) {
            LOG_ERROR("Failed to load solid shaders");
            return nullptr;
        }

        auto mat = std::make_shared<Assets::MaterialInstance>(
            tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);

        Assets::TextureLoader loader(m_rhi->getResourceManager());
        auto tex = loader.loadTextureFromMemory(rgba.data(), 1, 1, RHI::Format::RGBA8_UNorm, "SolidColor");
        if (tex.texture.isValid())
            mat->setTexture("texSampler", tex.texture, tex.sampler);

        mat->setSubpassTag("Forward_Opaque");
        mat->enableDepthTest(true);
        mat->enableDepthWrite(!translucent);   // 半透明关深度写，避免遮挡排序问题

        if (translucent) {
            RHI::BlendAttachmentState blend;
            blend.blendEnable = true;   // 默认 SrcAlpha/OneMinusSrcAlpha，正好是标准 alpha 混合
            mat->setAttachments({ blend });
            mat->enableTransparent(true);
        }
        return mat;
    }

    void addCube(const glm::vec3& pos, float size, const std::array<uint8_t, 4>& rgba, bool translucent) {
        auto obj = std::make_shared<Scene::RenderObject>();
        obj->geometry = Assets::GeometryGenerator::createCube(m_rhi->getResourceManager(), size, size, size);
        obj->materials = { makeSolidMaterial(rgba, translucent) };
        obj->transform = glm::translate(glm::mat4(1.0f), pos);
        m_scene->addObject(obj);
    }

    // 粒子渲染材质：particle.vert/frag + alpha 混合（渲染走通用材质）
    std::shared_ptr<Assets::MaterialInstance> makeParticleMaterial() {
        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(
            m_rhi->getResourceManager(), m_descriptorSetLayout);
        if (!tmpl->loadShaders("assets/shaders/test/particle.vert", "assets/shaders/test/particle.frag")) {
            LOG_ERROR("Failed to load particle shaders");
            return nullptr;
        }
        auto mat = std::make_shared<Assets::MaterialInstance>(
            tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);
        RHI::BlendAttachmentState blend;
        blend.blendEnable = true;   // 默认 SrcAlpha/OneMinusSrcAlpha
        mat->setAttachments({ blend });
        mat->setDepthTest(false);
        mat->setDepthWrite(false);
        return mat;
    }

    void addEmitter(const std::string& name, uint32_t count, ParticleParams p, const glm::vec3& pos) {
        auto em = std::make_shared<Scene::ParticleEmitter>();
        em->name = name;
        em->particleCount = count;
        em->computeShader = "assets/shaders/test/particle.comp";
        em->material = makeParticleMaterial();
        em->params = p;
        em->transform = glm::translate(glm::mat4(1.0f), pos);   // 发射器局部坐标 → 世界
        m_scene->addParticleEmitter(em);
    }

    void createScene() {
        // 粒子发射器（场景内容，可增删；增删后 renderer->setNeedRebuildGraph()）
        ParticleParams fire;
        fire.gravity  = -0.15f;
        fire.speedMin = 0.5f;
        fire.speedMax = 2.0f;
        fire.lifetime = 3.5f;
        fire.spreadXZ = 1.2f;
        fire.swayFreq = 2.7f;
        fire.swayAmp  = 0.6f;
        fire.emitterY = 0.0f;
        fire.topDiffuse   = 1.5f;
        fire.topThreshold = 2.5f;
        fire.colorYoung[0] = 1.0f; fire.colorYoung[1] = 0.9f; fire.colorYoung[2] = 0.2f;
        fire.colorMiddle[0]= 1.0f; fire.colorMiddle[1]= 0.4f; fire.colorMiddle[2]= 0.05f;
        fire.colorOld[0]   = 0.6f; fire.colorOld[1]   = 0.1f; fire.colorOld[2]   = 0.02f;
        fire.pointSizeMin  = 3.0f;
        fire.pointSizeMax  = 12.0f;
        addEmitter("Fire", 1024, fire, glm::vec3(0.0f, 0.0f, 0.0f));

        ParticleParams sparks;
        sparks.gravity  = -0.05f;
        sparks.speedMin = 0.3f;
        sparks.speedMax = 1.5f;
        sparks.lifetime = 2.0f;
        sparks.spreadXZ = 0.5f;
        sparks.swayFreq = 3.5f;
        sparks.swayAmp  = 0.4f;
        sparks.emitterY = 0.0f;
        sparks.topDiffuse   = 1.0f;
        sparks.topThreshold = 2.0f;
        sparks.colorYoung[0] = 0.0f; sparks.colorYoung[1] = 1.0f; sparks.colorYoung[2] = 0.0f;
        sparks.colorMiddle[0]= 0.0f; sparks.colorMiddle[1]= 0.8f; sparks.colorMiddle[2]= 0.0f;
        sparks.colorOld[0]   = 0.0f; sparks.colorOld[1]   = 0.4f; sparks.colorOld[2]   = 0.0f;
        sparks.pointSizeMin  = 4.0f;
        sparks.pointSizeMax  = 15.0f;
        addEmitter("Sparks", 256, sparks, glm::vec3(0.0f, 0.0f, 0.0f));

        auto cam = std::make_shared<Scene::PerspectiveCamera>();
        cam->setPerspective(glm::radians(45.0f), (float)m_width / m_height, 0.1f, 100.0f);
        cam->lookAt(glm::vec3(0.0f, 1.2f, 4.5f), glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_scene->addCamera(cam);
        m_scene->setActiveCamera(cam);
    }

    uint32_t m_width, m_height;
    std::shared_ptr<RHI::IRHI> m_rhi;
    std::shared_ptr<Renderer> m_renderer;
    std::shared_ptr<Scene::Scene> m_scene;
    RHI::DescriptorSetHandle m_descriptorSet;
    RHI::DescriptorPoolHandle m_descriptorPool;
    RHI::DescriptorSetLayoutHandle m_descriptorSetLayout;
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
#endif

    StarryEngine::Logger::init();
    StarryEngine::Logger::setLevel("info");

    // 透明 + 无边框 + 置顶的小窗口，模拟桌面角色形态
    StarryEngine::Application::Config cfg;
    cfg.width = kWinW;
    cfg.height = kWinH;
    cfg.title = "Transparent Window Smoke Test";
    cfg.resizable = false;
    cfg.transparent = true;
    cfg.borderless = true;
    cfg.alwaysOnTop = true;
    // 透明区不挡鼠标点击（穿透到桌面）；STARRY_NO_CLICKTHROUGH=1 关闭（排查 X11 崩溃用）
    cfg.clickThrough = (std::getenv("STARRY_NO_CLICKTHROUGH") == nullptr);
    // Wayland 会话下默认走原生 Wayland（XWayland 不支持 alpha 合成）；
    // STARRY_FORCE_X11=1 强制 X11 平台用于验证 X11 代码路径
    cfg.nativeWayland = (std::getenv("STARRY_FORCE_X11") == nullptr);
    LOG_INFO("[demo] 平台: {}（STARRY_FORCE_X11 强制 X11）",
             cfg.nativeWayland ? "原生 Wayland" : "X11");

    StarryEngine::Application app(cfg);

    auto demo = std::make_shared<TransparentWindowDemo>(
        app.getRenderHardwareInterface(), app.getGlobalDescriptorPool(), app.getWidth(), app.getHeight());

    app.setRenderer(demo->getRenderer());
    app.setScene(demo->getScene());
    app.initEventDispatcher();

    // F1：切换点击穿透（穿透模式保留窗口顶部一条可点把手，点了聚焦后即可按 F1）
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
    return 0;
}
