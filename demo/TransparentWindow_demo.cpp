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
#include "../src/renderer/passes/PassWrapper.hpp"
#include "../src/renderer/passes/GraphicsPass.hpp"
#include "../src/renderer/renderPaths/DeferredRenderPath.hpp"
#include "../src/renderer/passExecutor/MeshDrawExecutor.hpp"
#include "../src/assets/geometry/GeometryGenerator.hpp"
#include "../src/assets/material/DefaultMaterialTemplate.hpp"
#include "../src/assets/material/MaterialInstance.hpp"
#include "../src/assets/loader/TextureLoader.hpp"

#include "../src/application/Application.hpp"
#include "../src/event/Events.hpp"

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
        m_descriptorSet = m_renderer->getGlobalDescriptorSet();

        auto renderPath = std::make_shared<DeferredRenderPath>(m_rhi, m_width, m_height);
        // 透明窗口：present 阶段清屏为全透明
        renderPath->setPresentClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });

        auto colorDesc = PassWrapper::createColorTextureDesc({m_width, m_height, 1}, RHI::Format::RGBA16_Float);
        auto depthDesc = PassWrapper::createDepthTextureDesc({m_width, m_height, 1}, RHI::Format::D32_Float);
        auto swapDesc  = PassWrapper::createColorTextureDesc({m_width, m_height, 1}, RHI::Format::BGRA8_sRGB);
        renderPath->addTextureDesc("SceneColor", colorDesc);
        renderPath->addTextureDesc("Depth",      depthDesc);
        renderPath->addTextureDesc("Swapchain",  swapDesc);

        PassList passes;

        auto forwardPass = std::make_shared<GraphicsPass>("ForwardPass");
        {
            SubpassDesc sp;
            sp.name = "OpaqueGeometry"; sp.tag = "Forward_Opaque";
            sp.executor = std::make_shared<MeshDrawExecutor>();

            RenderGraph::AttachmentParams color;
            color.initialLayout = RHI::ImageLayout::Undefined;
            color.finalLayout   = RHI::ImageLayout::ShaderReadOnly;
            color.loadOp = RHI::AttachmentLoadOp::Clear;
            color.storeOp = RHI::AttachmentStoreOp::Store;
            color.clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };  // 背景全透明
            sp.colorAttachments.push_back({"SceneColor", color});

            RenderGraph::AttachmentParams depth;
            depth.initialLayout = RHI::ImageLayout::Undefined;
            depth.finalLayout   = RHI::ImageLayout::DepthStencilAttachment;
            depth.loadOp = RHI::AttachmentLoadOp::Clear;
            depth.storeOp = RHI::AttachmentStoreOp::Store;
            depth.clearDepth = 1.0f;
            sp.depthAttachment = {"Depth", depth};

            forwardPass->addSubpass(sp);
        }
        passes.push_back(forwardPass);

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

    void createScene() {
        // 左：不透明红方块；右：半透明蓝方块
        addCube(glm::vec3(-1.8f, 0.5f, 0.0f), 1.6f, { 230, 60, 60, 255 }, false);
        addCube(glm::vec3( 1.8f, 0.5f, 0.0f), 1.6f, {  60, 120, 255, 140 }, true);

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
