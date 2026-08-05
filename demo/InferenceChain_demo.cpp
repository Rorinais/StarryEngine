// ═══════════════════════════════════════════════════════════════
//  异构交错 pass 测试（信息级日志可见附件推断/依赖/布局转换结果）
//
//  真实管线形态：不同 pass 类型交错 + 跨 pass 依赖，不止"同类链"。
//     BasePass(网格, Clear) → Process(compute: 读 SceneColor 写 Processed)
//     → PostProcess(2 subpass: A 读 Processed 输入附件, B 网格)
//     → ParticlePass(compute+渲染进 SceneColor) → Presentation
//
//  验证点：
//    - 异构类型交错：mesh / compute / 读输入附件后处理 / 粒子 / 呈现
//    - 跨 pass 依赖：SceneColor(mesh) → compute 采样 → 后处理输入附件
//    - 布局转换：SceneColor ShaderReadOnly→compute→ShaderReadOnly；Processed General→ShaderReadOnly
//    - 多 subpass + 附件推断（Post_Grid 不写显式参数，靠推断）
//    - 粒子无材质 → 走 ParticlePass 默认 Sprite 材质
// ═══════════════════════════════════════════════════════════════
#include <cstdlib>
#include "../src/renderer/passes/PassWrapper.hpp"
#include "../src/renderer/passes/GraphicsPass.hpp"
#include "../src/renderer/passes/MeshPass.hpp"
#include "../src/renderer/passes/ComputePass.hpp"
#include "../src/renderer/passes/ParticlePass.hpp"
#include "../src/renderer/renderPaths/DeferredRenderPath.hpp"
#include "../src/renderer/passExecutor/SceneDrawExecutor.hpp"
#include "../src/scene/ParticleEmitter.hpp"
#include "../src/assets/geometry/GeometryGenerator.hpp"
#include "../src/assets/material/DefaultMaterialTemplate.hpp"
#include "../src/assets/material/MaterialInstance.hpp"
#include "../src/assets/loader/TextureLoader.hpp"
#include "../src/application/Application.hpp"

using namespace StarryEngine;

namespace { constexpr uint32_t kWin = 400; }

static SubpassDesc makeChainSubpass(const std::string& tag) {
    SubpassDesc sp;
    sp.tag = tag;
    sp.executor = std::make_shared<SceneDrawExecutor>();
    sp.colorAttachments.push_back({ "SceneColor" });   // loadOp/布局交给渲染图推断
    sp.depthAttachment = { "Depth" };
    return sp;
}

class InferenceChainDemo {
public:
    InferenceChainDemo(std::shared_ptr<RHI::IRHI> rhi, RHI::DescriptorPoolHandle pool,
                       uint32_t w, uint32_t h)
        : m_rhi(rhi), m_pool(pool), m_w(w), m_h(h) {
        m_scene = std::make_shared<Scene::Scene>();

        m_renderer = std::make_shared<Renderer>(m_rhi, m_pool, m_scene);
        m_renderer->createGlobalSetLayout();
        m_renderer->createGlobalUniformBuffer();
        m_renderer->initDefaultMaterials();
        m_setLayout = m_renderer->getGlobalSetLayout();
        m_descSet = m_renderer->getGlobalDescriptorSet();

        auto path = std::make_shared<DeferredRenderPath>(m_rhi, m_w, m_h);
        path->setScene(m_scene.get());
        path->addTextureDesc("SceneColor", PassWrapper::createColorTextureDesc({m_w, m_h, 1}, RHI::Format::RGBA16_Float));
        path->addTextureDesc("Depth",      PassWrapper::createDepthTextureDesc({m_w, m_h, 1}, RHI::Format::D32_Float));
        path->addTextureDesc("Processed",  PassWrapper::createColorTextureDesc({m_w, m_h, 1}, RHI::Format::RGBA16_Float));
        path->addTextureDesc("Swapchain",  PassWrapper::createColorTextureDesc({m_w, m_h, 1}, RHI::Format::BGRA8_sRGB));

        PassList passes;

        // 1. 首写：网格 → SceneColor+Depth（Clear）
        passes.push_back(std::make_shared<MeshPass>("BasePass", "Forward_Opaque"));

        // 2. compute：读 SceneColor(sampler) → 反色 → 写 Processed(storage image)
        {
            ComputePassDesc pd;
            pd.name = "Process";
            pd.shader = "assets/shaders/test/process.comp";
            pd.dispatchX = (m_w + 15) / 16;
            pd.dispatchY = (m_h + 15) / 16;
            pd.resources = {
                { RHI::DescriptorType::CombinedImageSampler, 0, "SceneColor", false, 0 },
                { RHI::DescriptorType::StorageImage,         1, "Processed",  true,  0 },
            };
            passes.push_back(std::make_shared<ComputePass>(pd));
        }

        // 3. 后处理（2 subpass）：A 读 Processed 输入附件写 SceneColor；B 网格写 SceneColor（推断）
        {
            auto post = std::make_shared<GraphicsPass>("PostProcess");

            RenderGraph::AttachmentParams load;
            load.initialLayout = RHI::ImageLayout::ShaderReadOnly;
            load.finalLayout   = RHI::ImageLayout::ShaderReadOnly;
            load.loadOp = RHI::AttachmentLoadOp::Load;
            load.storeOp = RHI::AttachmentStoreOp::Store;

            RenderGraph::AttachmentParams in;
            in.initialLayout = RHI::ImageLayout::ShaderReadOnly;
            in.finalLayout   = RHI::ImageLayout::ShaderReadOnly;
            in.loadOp = RHI::AttachmentLoadOp::Load;
            in.storeOp = RHI::AttachmentStoreOp::DontCare;

            SubpassDesc read;
            read.tag = "Post_Read";
            read.executor = std::make_shared<SceneDrawExecutor>();
            read.colorAttachments.push_back({ "SceneColor", load });
            read.inputAttachments.push_back({ "Processed", in });
            post->addSubpass(read);

            SubpassDesc grid = makeChainSubpass("Post_Grid");   // 附件推断：SceneColor 后续写者 Load
            post->addSubpass(grid);
            passes.push_back(post);
        }

        // 4. 粒子：compute+渲染进 SceneColor（emitter 无材质 → 默认 Sprite 材质）
        passes.push_back(std::make_shared<ParticlePass>("Particles", "Particles"));

        path->setPassList(std::move(passes));
        m_renderer->setRenderPath(std::move(path));

        // ── 场景内容 ──
        // 红色立方体（进 BasePass）
        {
            auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(m_rhi->getResourceManager(), m_setLayout);
            tmpl->loadShaders("assets/shaders/core/shader.vert", "assets/shaders/core/shader.frag");
            auto mat = std::make_shared<Assets::MaterialInstance>(tmpl, m_pool, m_rhi->getResourceManager().get(), m_descSet);
            static const uint8_t red[4] = { 200, 60, 60, 255 };
            Assets::TextureLoader loader(m_rhi->getResourceManager());
            auto tex = loader.loadTextureFromMemory(red, 1, 1, RHI::Format::RGBA8_UNorm, "Red");
            mat->setTexture("texSampler", tex.texture, tex.sampler);
            mat->setSubpassTag("Forward_Opaque");
            mat->enableDepthTest(true);
            mat->enableDepthWrite(true);

            auto cube = std::make_shared<Scene::RenderObject>();
            cube->geometry = Assets::GeometryGenerator::createCube(m_rhi->getResourceManager(), 1.5f, 1.5f, 1.5f);
            cube->materials = { mat };
            m_scene->addObject(cube);
        }

        // 后处理材质（读 Processed 输入附件，进 Post_Read）
        {
            auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(m_rhi->getResourceManager(), m_setLayout);
            tmpl->loadShaders("assets/shaders/deferred/fullscreen.vert", "assets/shaders/test/postprocess.frag");
            auto mat = std::make_shared<Assets::MaterialInstance>(tmpl, m_pool, m_rhi->getResourceManager().get(), m_descSet);
            mat->setSubpassTag("Post_Read");
            // 输入附件按名依赖 → render path 把物理纹理绑进来
            mat->addTextureDependency("Processed", 1, 0, Assets::ResourceDependencyType::InputAttachment);

            auto effect = std::make_shared<Scene::ProceduralEffect>();
            effect->material = mat;
            effect->vertexCount = 3;
            m_scene->addProceduralEffect(effect);
        }

        // 粒子发射器（无材质 → ParticlePass 默认 Sprite 材质；无 passTag → 默认 "Particles"）
        {
            auto fire = std::make_shared<Scene::ParticleEmitter>();
            fire->name = "Fire";
            fire->particleCount = 512;
            fire->params.gravity  = -0.1f;
            fire->params.speedMin = 0.5f;
            fire->params.speedMax = 1.5f;
            fire->params.lifetime = 3.0f;
            fire->params.colorYoung[0] = 1.0f; fire->params.colorYoung[1] = 0.6f; fire->params.colorYoung[2] = 0.2f;
            fire->params.colorMiddle[0]= 1.0f; fire->params.colorMiddle[1]= 0.3f; fire->params.colorMiddle[2]= 0.05f;
            fire->params.colorOld[0]   = 0.5f; fire->params.colorOld[1]   = 0.1f; fire->params.colorOld[2]   = 0.02f;
            fire->params.pointSizeMin  = 3.0f;
            fire->params.pointSizeMax  = 8.0f;
            m_scene->addParticleEmitter(fire);
        }

        auto cam = std::make_shared<Scene::PerspectiveCamera>();
        cam->setPerspective(glm::radians(45.0f), (float)m_w / m_h, 0.1f, 100.0f);
        cam->lookAt(glm::vec3(0, 1, 4), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        m_scene->addCamera(cam);
        m_scene->setActiveCamera(cam);
    }

    std::shared_ptr<Renderer> getRenderer() { return m_renderer; }
    std::shared_ptr<Scene::Scene> getScene() { return m_scene; }

private:
    std::shared_ptr<RHI::IRHI> m_rhi;
    RHI::DescriptorPoolHandle m_pool;
    RHI::DescriptorSetHandle m_descSet;
    RHI::DescriptorSetLayoutHandle m_setLayout;
    std::shared_ptr<Renderer> m_renderer;
    std::shared_ptr<Scene::Scene> m_scene;
    uint32_t m_w, m_h;
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
            setenv("VK_LAYER_PATH", (std::string(exePath) + "/layers").c_str(), 1);
            setenv("LD_LIBRARY_PATH", (std::string(exePath) + ":" + (getenv("LD_LIBRARY_PATH") ?: "")).c_str(), 1);
        }
    }
#endif
    StarryEngine::Logger::init();
    StarryEngine::Logger::setLevel("info");

    StarryEngine::Application::Config cfg;
    cfg.width = kWin; cfg.height = kWin;
    cfg.title = "Heterogeneous Pass Chain";
    cfg.resizable = false;
    cfg.transparent = false;
    StarryEngine::Application app(cfg);

    auto demo = std::make_shared<InferenceChainDemo>(
        app.getRenderHardwareInterface(), app.getGlobalDescriptorPool(), app.getWidth(), app.getHeight());
    app.setRenderer(demo->getRenderer());
    app.setScene(demo->getScene());
    app.initEventDispatcher();
    app.run();
    StarryEngine::Logger::shutdown();
    return 0;
}
