#include <renderer/passes/PassWrapper.hpp>
#include <renderer/passes/GraphicsPass.hpp>
#include <renderer/passes/MeshPass.hpp>
#include <renderer/passes/ParticlePass.hpp>
#include <renderer/passes/ShadowPass.hpp>
#include <scene/ParticleEmitter.hpp>
#include <renderer/renderPaths/DeferredRenderPath.hpp>
#include <renderer/passExecutor/SceneDrawExecutor.hpp>
#include <assets/geometry/GeometryGenerator.hpp>

#include <application/Application.hpp>
#include <event/Events.hpp>
#include <renderer/backend/vulkan/FrameContext.hpp>
#include <renderer/utils/FrameCapture.hpp>

#include <limits>
#include <chrono>
#include <cstdlib>

static bool g_clickThrough = true;

namespace {
    constexpr uint32_t kWinW = 560;
    constexpr uint32_t kWinH = 680;

    float computeModelFootY(const std::shared_ptr<StarryEngine::Assets::Geometry>& geometry) {
        if (!geometry) return 0.0f;
        const auto& layout = geometry->getVertexLayout();
        const auto& verts = geometry->getVertices();

        uint32_t posOffset = 0, binding = 0;
        bool found = false;
        for (const auto& a : layout.getAppAttributes()) {
            if (a.semantic == StarryEngine::Assets::VertexSemantic::Position) {
                posOffset = a.offset;
                binding = a.binding;
                found = true;
                break;
            }
        }
        if (!found) return 0.0f;

        uint32_t stride = layout.getBindingStride(binding);
        if (stride == 0) return 0.0f;

        const uint32_t strideF = stride / sizeof(float);
        const uint32_t posF = posOffset / sizeof(float) + 1;  // +1 = 位置 Y 分量
        float footY = std::numeric_limits<float>::max();
        for (size_t i = 0; i + strideF <= verts.size(); i += strideF)
            footY = std::min(footY, verts[i + posF]);
        return (footY == std::numeric_limits<float>::max()) ? 0.0f : footY;
    }
}

using namespace StarryEngine;

class PBRDemo {
public:
    PBRDemo(std::shared_ptr<RHI::IRHI> rhi, RHI::DescriptorPoolHandle descriptorPool, uint32_t width, uint32_t height):
    m_rhi(rhi),m_descriptorPool(descriptorPool),m_width(width),m_height(height){
        m_iblBuilder = std::make_shared<Assets::IBLBuilder>(rhi->getResourceManager(), rhi);
        createRenderer();
        createScene();
    }

    void createRenderer() {
        m_scene = std::make_shared<Scene::Scene>();

        m_renderer = std::make_shared<Renderer>(m_rhi, m_descriptorPool, m_scene);
        m_renderer->createGlobalSetLayout();
        m_renderer->createGlobalUniformBuffer();
        m_renderer->initDefaultMaterials();

        m_descriptorSetLayout = m_renderer->getGlobalSetLayout();
        m_descriptorSet = m_renderer->getGlobalDescriptorSet();

        auto renderPath = std::make_shared<DeferredRenderPath>(m_rhi, m_width, m_height);
        renderPath->setScene(m_scene.get());   // 场景数据源：粒子等 pass 建图时通过 configure 拿到
        renderPath->setPresentClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });

        auto colorDesc = PassWrapper::createColorTextureDesc({m_width, m_height, 1}, RHI::Format::RGBA16_Float);
        auto depthDesc = PassWrapper::createDepthTextureDesc({m_width, m_height, 1}, RHI::Format::D24_UNorm_S8_UInt);
        auto swapDesc   = PassWrapper::createColorTextureDesc({m_width, m_height, 1}, RHI::Format::BGRA8_sRGB);
        // 阴影贴图：光源视角 depth-only，分辨率固定 2048²（与窗口无关）
        auto shadowMapDesc = PassWrapper::createDepthTextureDesc({ShadowPass::kShadowMapSize, ShadowPass::kShadowMapSize, 1}, RHI::Format::D24_UNorm_S8_UInt);
        renderPath->addTextureDesc("SceneColor",  colorDesc);
        renderPath->addTextureDesc("Depth",       depthDesc);
        renderPath->addTextureDesc("Swapchain",   swapDesc);
        renderPath->addTextureDesc("ShadowMap",   shadowMapDesc);

        {
            PassList passes;

            // ShadowPass：先用光源 VP 把网格重渲成深度贴图（供 ForwardPass 的 PBR 材质采样）
            passes.push_back(std::make_shared<ShadowPass>("ShadowPass"));

            // ForwardPass: OpaqueGeometry —— MeshPass 封装标准几何 pass（SceneColor+Depth 清屏 + Mesh 绘制）
            auto forwardPass = std::make_shared<MeshPass>("ForwardPass", "Forward_Opaque",RHI::Color::Transparent());
            forwardPass->addReadTextureByName("ShadowMap");  // PBR 材质采样阴影贴图（shader 级读，显式声明依赖）
            passes.push_back(forwardPass);

            //PostProcessPass: Skybox + Grid（后续写者，loadOp/布局由渲染图推断为 Load）
            auto postPass = std::make_shared<GraphicsPass>("PostProcessPass");
            {
                SubpassDesc sky;
                sky.tag = "PostProcess_Skybox";
                sky.executor = std::make_shared<SceneDrawExecutor>(); 
                sky.colorAttachments.push_back({"SceneColor"});
                sky.depthAttachment = {"Depth"};
                postPass->addSubpass(sky);

                SubpassDesc grid;
                grid.tag = "PostProcess_Grid";
                grid.executor = std::make_shared<SceneDrawExecutor>();
                grid.colorAttachments.push_back({"SceneColor"});
                grid.depthAttachment = {"Depth"};
                postPass->addSubpass(grid);
            }
            passes.push_back(postPass);

            // ★模板描边 pass（放在粒子前：角色本体+外圈先画，粒子盖其上）：
            //   subpass "StencilWrite"：角色本体正常画，同时 stencil Replace 写 1
            //   subpass "StencilTest" ：放大 1.08 的角色副本，stencil NotEqual 反选 → 只留外圈描边
            // 同一 render pass 内 subpass 间 stencil 读写靠 Vulkan 隐式 framebuffer-local 依赖；
            // 本 pass 的深度附件 stencilLoadOp=Clear → 开 pass 时 stencil 清零，本体从 0 写 1。
            {
                auto stencilPass = std::make_shared<GraphicsPass>("StencilPass");

                RenderGraph::AttachmentParams ds;
                ds.clearDepth = 1.0f;
                ds.clearStencil = 0;

                SubpassDesc sw;
                sw.tag = "StencilWrite";
                sw.executor = std::make_shared<SceneDrawExecutor>();
                sw.colorAttachments.push_back({ "SceneColor", RenderGraph::AttachmentParams{} });
                sw.depthAttachment = { "Depth", ds };
                stencilPass->addSubpass(sw);

                SubpassDesc st;
                st.tag = "StencilTest";
                st.executor = std::make_shared<SceneDrawExecutor>();
                st.colorAttachments.push_back({ "SceneColor", RenderGraph::AttachmentParams{} });
                st.depthAttachment = { "Depth", ds };
                stencilPass->addSubpass(st);

                passes.push_back(stencilPass);
            }

            passes.push_back(std::make_shared<ParticlePass>("Particles", "Particles"));

            renderPath->setPassList(std::move(passes));
        }

        m_renderPath = renderPath;
        m_renderer->setRenderPath(m_renderPath);

        // 平行光阴影：从光源方向构建正交 view*proj（与 PBR shader 的 L=normalize(-lights.position) 一致）。
        // 上传到 Renderer → GlobalUniforms.lightVP，阴影贴图 pass 与 PBR 采样共用同一矩阵。
        {
            glm::vec3 lightDir = glm::normalize(glm::vec3(0.5f, 1.0f, 0.8f));  // = normalize(-(-0.5,-1,-0.8))
            glm::vec3 sceneCenter(0.0f, 1.0f, 0.0f);
            // 视锥必须盖住可见地板（含球阴影区域），否则 shadowmap 边界线会露在画面里（摄像机左侧一条线）。
            // 可见地板在光空间的跨度约 ±9，取 12 保证边界推出画面外（shadowmap 分辨率损失可忽略）。
            float halfExtent = 12.0f;
            float dist = 12.0f;
            // 阴影相机必须放在“光源侧”朝场景看（太阳在 L 方向上方 → 相机在中心+L*dist 向下看）。
            // 若放 -L*dist（光源到达侧）则从场景下方仰视，阴影贴图只拍到背面 → 地面永远收不到球影。
            glm::vec3 eye = sceneCenter + lightDir * dist;
            glm::mat4 view = glm::lookAt(eye, sceneCenter, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 proj = glm::orthoRH_ZO(-halfExtent, halfExtent, -halfExtent, halfExtent, 0.1f, 26.0f);
            proj[1][1] *= -1.0f;   // 与引擎投影约定一致（Vulkan Y 翻转）
            m_renderer->setLightViewProj(proj * view);
        }

        // 帧序列输出：STARRY_FRAME_DUMP=<目录> 激活，STARRY_FRAME_COUNT=<帧数>（默认 1，0=持续）
        if (const char* dumpDir = std::getenv("STARRY_FRAME_DUMP")) {
            const char* cnt = std::getenv("STARRY_FRAME_COUNT");
            uint32_t n = cnt ? static_cast<uint32_t>(std::atoi(cnt)) : 1u;
            m_frameCapture = std::make_shared<FrameCapture>(m_rhi->getResourceManager());
            if (!m_frameCapture->initialize({ dumpDir, n })) {
                m_frameCapture.reset();
            }
        }
    }

    void initIBL() {
        m_envCubemap      = m_iblBuilder->buildEnvCubemap("assets/textures/pbr/kloofendal_48d_partly_cloudy_puresky_1k.hdr", 128);
        m_irradianceMap   = m_iblBuilder->generateIrradianceMapCS(m_envCubemap, 64);
        m_prefilteredMap  = m_iblBuilder->generatePrefilteredMapCS(m_envCubemap, 256, 6);
        m_brdfLut         = m_iblBuilder->generateBrdfLutCS(128);

        RHI::SamplerDesc cubeSampDesc;
        cubeSampDesc.minFilter = RHI::SamplerFilter::Linear;
        cubeSampDesc.magFilter = RHI::SamplerFilter::Linear;
        cubeSampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        cubeSampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        cubeSampDesc.addressW = RHI::SamplerAddressMode::ClampToEdge;
        cubeSampDesc.maxLod = 1.0f;
        m_cubeSampler = m_rhi->getResourceManager()->createSampler(cubeSampDesc);

        RHI::SamplerDesc prefilterSampDesc = cubeSampDesc;
        prefilterSampDesc.mipFilter = RHI::SamplerFilter::Linear;
        prefilterSampDesc.maxLod = 6.0f;
        m_prefilterSampler = m_rhi->getResourceManager()->createSampler(prefilterSampDesc);

        RHI::SamplerDesc lutSampDesc;
        lutSampDesc.minFilter = RHI::SamplerFilter::Linear;
        lutSampDesc.magFilter = RHI::SamplerFilter::Linear;
        lutSampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        lutSampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        lutSampDesc.maxLod = 1.0f;
        m_lutSampler = m_rhi->getResourceManager()->createSampler(lutSampDesc);
    }

    std::shared_ptr<Assets::MaterialInstance> createSkyboxMaterial() {

        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(m_rhi->getResourceManager(), m_descriptorSetLayout);
        tmpl->loadShaders("assets/shaders/deferred/skybox.vert", "assets/shaders/deferred/skybox.frag");

        auto material = std::make_shared<Assets::MaterialInstance>(tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);

        material->setTexture("uSkybox", m_envCubemap, m_cubeSampler);
        material->setSubpassTag("PostProcess_Skybox");
        material->enableDepthTest(true);
        material->setDepthCompareOp(RHI::CompareOp::LessOrEqual);

        return material;
    }

    std::shared_ptr<Assets::MaterialInstance> createGridMaterial() {
        auto gridTmpl = std::make_shared<Assets::DefaultMaterialTemplate>(m_rhi->getResourceManager(), m_descriptorSetLayout);
        gridTmpl->loadShaders("assets/shaders/core/gridShader.vert", "assets/shaders/core/gridShader.frag");

        auto gridMaterialInst = std::make_shared<Assets::MaterialInstance>(gridTmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);
        gridMaterialInst->enableDepthTest(true);
        gridMaterialInst->setSubpassTag("PostProcess_Grid");

        return gridMaterialInst;
    }

    std::shared_ptr<Assets::MaterialInstance> createTexturedPbrMaterial() {
        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(m_rhi->getResourceManager(), m_descriptorSetLayout);
        tmpl->loadShaders("assets/shaders/pbr/shpere_pbr.vert", "assets/shaders/pbr/shpere_pbr (2).frag");

        auto material = std::make_shared<Assets::MaterialInstance>(tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);
        material->setSubpassTag("Forward_Opaque");
        material->enableDepthTest(true);
        material->enableDepthWrite(true);

        Assets::TextureLoader texLoader(m_rhi->getResourceManager());
        auto armT    = texLoader.loadTexture2D("assets/textures/pbr/metal_plate_02_arm_1k.png", RHI::Format::RGBA8_UNorm, "ARM");
        auto albedoT = texLoader.loadTexture2D("assets/textures/pbr/metal_plate_02_diff_1k.png", RHI::Format::RGBA8_sRGB, "Albedo");
        auto normalT = texLoader.loadTexture2D("assets/textures/pbr/metal_plate_02_nor_dx_1k.png", RHI::Format::RGBA8_UNorm, "Normal");

        material->setTexture("armMap", armT.texture, armT.sampler);
        material->setTexture("albedoMap", albedoT.texture, albedoT.sampler);
        material->setTexture("normalMap", normalT.texture, normalT.sampler);
        material->setTexture("uIrradianceMap", m_irradianceMap, m_cubeSampler);
        material->setTexture("uPrefilteredMap", m_prefilteredMap, m_prefilterSampler);
        material->setTexture("uBrdfLut", m_brdfLut, m_lutSampler);
        // 阴影贴图：图形虚拟纹理 "ShadowMap" 建图后由 updateMaterialTextures 自动绑定到 set2 binding6（uShadowMap）
        material->addTextureDependency("ShadowMap", 2, 6);

        auto* lightBlock = material->getBlock("LightingUBO");
        if (lightBlock) {
            lightBlock->setVec4("lights.position", glm::vec4(-0.5f, -1.0f, -0.8f, 0.0f));  
            lightBlock->setVec4("lights.color", glm::vec4(3.0f, 2.7f, 2.3f, 1.0f));
            lightBlock->setFloat("lightCount", 1.0f);
            lightBlock->setFloat("ambientStrength", 0.15f);
        }
        material->applyAllDirtyBlocks();
        return material;
    }

    
    void createScene() {
        initIBL();

        auto skyboxEffect = std::make_shared<Scene::ProceduralEffect>();
        skyboxEffect->material = createSkyboxMaterial();
        m_scene->addProceduralEffect(skyboxEffect);

        auto mat = createTexturedPbrMaterial();
        auto sphere = std::make_shared<Scene::RenderObject>();
        sphere->geometry = Assets::GeometryGenerator::createSphere(m_rhi->getResourceManager(), 1.0f);
        sphere->materials = { mat };
        sphere->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.5f, 0.0f));

        // ── 变换动画：球体自转 + 上下浮动 ──
        {        
            auto skyboxEffect = std::make_shared<Scene::ProceduralEffect>();
            skyboxEffect->material = createSkyboxMaterial();
            m_scene->addProceduralEffect(skyboxEffect);

            auto mat = createTexturedPbrMaterial();
            auto sphere = std::make_shared<Scene::RenderObject>();
            sphere->geometry = Assets::GeometryGenerator::createSphere(m_rhi->getResourceManager(), 1.0f);
            sphere->materials = { mat };
            sphere->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.5f, 0.0f));

            // ── 变换动画：球体自转 + 上下浮动 ──
            {
                auto clip = std::make_shared<Assets::AnimationClip>();
                clip->name = "SpinBob";
                clip->duration = 4.0f;    // 4 秒一圈
                clip->looping = true;
                // 每 0.5s 一个关键帧：绕 Y 轴旋转 45°，并上下浮动
                for (int i = 0; i <= 8; ++i) {
                    float t = i * 0.5f;
                    Assets::TransformKeyframe kf;
                    kf.time = t;
                    kf.position = glm::vec3(0.0f, 1.5f + 0.3f * std::sin(t * glm::pi<float>() / 2.0f), 0.0f);
                    kf.rotation = glm::angleAxis(t * glm::pi<float>() / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
                    kf.scale = glm::vec3(1.0f);
                    clip->keyframes.push_back(kf);
                }

                auto animator = std::make_shared<Scene::Animator>();
                animator->setClip(clip);
                animator->setSpeed(1.0f);
                sphere->animator = animator;
            }

            m_scene->addObject(sphere);

            auto groundMat = createTexturedPbrMaterial();
            auto ground = std::make_shared<Scene::RenderObject>();
            ground->geometry = Assets::GeometryGenerator::createCube(m_rhi->getResourceManager(), 20.0f, 0.3f, 20.0f);
            ground->materials = { groundMat };
            ground->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.15f, 0.0f));

            m_scene->addObject(ground);
        }

        addGriseoModel();

        // ── 模板描边：角色本体在 StencilWrite 画 + stencil Replace 写 1；放大副本在 StencilTest 测 NotEqual 反选 → 只留外圈 ──
        {
            // 1) 角色本体材质：tag 从 Forward_Opaque 改到 StencilWrite（本体移到描边 pass 里画），开 stencil 写 1
            for (auto& mat : m_skinnedMaterials) {
                if (!mat) continue;
                mat->setSubpassTag("StencilWrite");
                RHI::StencilOpState write;
                write.failOp = RHI::StencilOp::Keep;
                write.passOp = RHI::StencilOp::Replace;   // 测试通过（Always）→ 写 stencil=1
                write.depthFailOp = RHI::StencilOp::Keep;
                write.compareOp = RHI::CompareOp::Always;
                write.compareMask = 0xFF;
                write.writeMask = 0xFF;
                write.reference = 1;
                mat->setStencilTest(true);
                mat->setStencilOps(write, write);
            }

            // 2) 描边副本：同一几何（同一骨骼矩阵）+ 放大变换，全 submesh 同一描边材质
            //    STARRY_NO_OUTLINE=1 关闭描边 → 与开着 A/B 对比描边的真实开销
            bool outlineOn = (std::getenv("STARRY_NO_OUTLINE") == nullptr);
            m_outlineMaterial = outlineOn ? makeOutlineMaterial() : nullptr;
            if (m_outlineMaterial && m_modelGeometry) {
                auto outline = std::make_shared<Scene::RenderObject>();
                outline->geometry = m_modelGeometry;
                // 材质列表长度对齐角色 submesh 索引（长度不够会解析到 default 材质 → 无蒙皮 → 破相）
                std::vector<std::shared_ptr<Assets::MaterialInstance>> outlineMats(m_modelMaterialCount, m_outlineMaterial);
                outline->materials = std::move(outlineMats);
                // 放大 1.08，围绕脚底 Y 缩放 → 外圈在脚下收拢、与脚重合（脚下不悬青色条），看起来像踩在地上
                float footY = computeModelFootY(m_modelGeometry);
                LOG_INFO("[demo] 描边缩放中心（脚底）Y = {:.3f}", footY);
                outline->transform =
                    glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, footY, 0.0f))
                    * glm::scale(glm::mat4(1.0f), glm::vec3(1.03f))
                    * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -footY, 0.0f));
                m_scene->addObject(outline);
            }
        }

        //粒子发射器（场景内容，可增删；增删后 renderer->setNeedRebuildGraph()）
        {
            auto fire = std::make_shared<Scene::ParticleEmitter>();
            fire->name = "Fire";
            fire->particleCount = 1024;
            fire->computeShader = "assets/shaders/test/particle.comp";
            fire->material = makeParticleMaterial();
            fire->params.gravity  = -0.15f;
            fire->params.speedMin = 0.5f;
            fire->params.speedMax = 2.0f;
            fire->params.lifetime = 3.5f;
            fire->params.spreadXZ = 1.2f;
            fire->params.swayFreq = 2.7f;
            fire->params.swayAmp  = 0.6f;
            fire->params.emitterY = 0.0f;
            fire->params.topDiffuse   = 1.5f;
            fire->params.topThreshold = 2.5f;
            fire->params.colorYoung[0] = 1.0f; fire->params.colorYoung[1] = 0.9f; fire->params.colorYoung[2] = 0.2f;
            fire->params.colorMiddle[0]= 1.0f; fire->params.colorMiddle[1]= 0.4f; fire->params.colorMiddle[2]= 0.05f;
            fire->params.colorOld[0]   = 0.6f; fire->params.colorOld[1]   = 0.1f; fire->params.colorOld[2]   = 0.02f;
            fire->params.pointSizeMin  = 3.0f;
            fire->params.pointSizeMax  = 12.0f;
            m_scene->addParticleEmitter(fire);

            auto sparks = std::make_shared<Scene::ParticleEmitter>();
            sparks->name = "Sparks";
            sparks->particleCount = 256;
            sparks->computeShader = "assets/shaders/test/particle.comp";
            sparks->material = makeParticleMaterial();
            sparks->params.gravity  = -0.05f;
            sparks->params.speedMin = 0.3f;
            sparks->params.speedMax = 1.5f;
            sparks->params.lifetime = 2.0f;
            sparks->params.spreadXZ = 0.5f;
            sparks->params.swayFreq = 3.5f;
            sparks->params.swayAmp  = 0.4f;
            sparks->params.emitterY = 2.5f;
            sparks->params.topDiffuse   = 1.0f;
            sparks->params.topThreshold = 2.0f;
            sparks->params.colorYoung[0] = 0.0f; sparks->params.colorYoung[1] = 1.0f; sparks->params.colorYoung[2] = 0.0f;
            sparks->params.colorMiddle[0]= 0.0f; sparks->params.colorMiddle[1]= 0.8f; sparks->params.colorMiddle[2]= 0.0f;
            sparks->params.colorOld[0]   = 0.0f; sparks->params.colorOld[1]   = 0.4f; sparks->params.colorOld[2]   = 0.0f;
            sparks->params.pointSizeMin  = 10.0f;
            sparks->params.pointSizeMax  = 24.0f;
            m_scene->addParticleEmitter(sparks);
        }

        auto perspectiveCamera = std::make_shared<Scene::PerspectiveCamera>();
        perspectiveCamera->setPerspective(glm::radians(45.0f), (float)m_width / m_height, 0.1f, 100.0f);
        perspectiveCamera->lookAt(glm::vec3(0.0f, 2.0f, 7.0f), glm::vec3(0.0f, 0.8f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_scene->addCamera(perspectiveCamera);
        m_scene->setActiveCamera(perspectiveCamera);
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
        blend.dstAlphaBlendFactor = RHI::BlendFactor::OneMinusSrcAlpha;  // alpha 累加而非替换：粒子叠在不透明角色上封口(a=1)，帧输出不再"角色前变黑"
        mat->setAttachments({ blend });
        mat->setDepthTest(false);
        mat->setDepthWrite(false);
        return mat;
    }

    // 模板测试 quad 通用材质：复用 shader.vert（set0 global UBO + push mat4）+ 指定 frag，
    // 绑白 1×1 纹理给 texSampler（保证 set1 描述符有效）。深度测试/写入默认关闭。
    std::shared_ptr<Assets::MaterialInstance> makeStencilQuadMaterial(const std::string& fragPath) {
        auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(m_rhi->getResourceManager(), m_descriptorSetLayout);
        if (!tmpl->loadShaders("assets/shaders/core/shader.vert", fragPath)) {
            LOG_ERROR("Failed to load stencil quad shaders: {}", fragPath);
            return nullptr;
        }
        auto mat = std::make_shared<Assets::MaterialInstance>(
            tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);
        Assets::TextureLoader loader(m_rhi->getResourceManager());
        static const uint8_t kWhite[4] = { 255, 255, 255, 255 };
        auto white = loader.loadTextureFromMemory(kWhite, 1, 1, RHI::Format::RGBA8_UNorm, "White");
        if (white.texture.isValid())
            mat->setTexture("texSampler", white.texture, white.sampler);
        mat->setDepthTest(false);
        mat->setDepthWrite(false);
        return mat;
    }

    std::shared_ptr<Renderer> getRenderer() { return m_renderer; }

    std::shared_ptr< Scene::Scene> getScene() { return m_scene; }

    // 每帧骨骼动画更新：推进时间 → 采样 → 上传骨骼矩阵 SSBO
    void onUpdate(float deltaTime);

    // 每帧渲染+呈现完成后读回 SceneColor 写 PNG（STARRY_FRAME_DUMP 时激活）
    void captureFrame();
private:
    bool loadModelGeometry(Assets::Geometry& outGeometry,std::vector<Assets::MaterialParams>& outParams,Assets::Skeleton* outSkeleton = nullptr);
    std::shared_ptr<Assets::MaterialInstance> makeModelMaterial(const Assets::MaterialParams& param, bool skinned);
    std::shared_ptr<Assets::MaterialInstance> makeOutlineMaterial();
    void addGriseoModel();

    Assets::Skeleton m_modelSkeleton;
    Assets::AnimationClip m_modelClip;
    std::string m_modelTextureDir = "fuxuan";  

    // 骨骼动画运行时状态
    Scene::Animator m_skeletalAnimator;
    float m_skeletalTime = 0.0f;
    bool m_skeletalReady = false;
    std::vector<std::shared_ptr<Assets::MaterialInstance>> m_skinnedMaterials;
    std::shared_ptr<Assets::Geometry> m_modelGeometry;          // 角色几何（描边副本复用同一几何+骨骼数据）
    size_t m_modelMaterialCount = 0;                             // 角色材质数（描边副本材质列表对齐 submesh 索引）
    std::shared_ptr<Assets::MaterialInstance> m_outlineMaterial; // 描边副本材质（onUpdate 同步喂骨骼矩阵）

    uint32_t m_width, m_height;
    std::shared_ptr<RHI::IRHI> m_rhi;
    std::shared_ptr<Renderer> m_renderer;
    std::shared_ptr< Scene::Scene> m_scene;

    // 帧序列捕获（STARRY_FRAME_DUMP 激活；保留 renderPath 引用以取 SceneColor 物理纹理）
    std::shared_ptr<DeferredRenderPath> m_renderPath;
    std::shared_ptr<FrameCapture> m_frameCapture;

    // IBL
    RHI::TextureHandle m_envCubemap;
    RHI::TextureHandle m_irradianceMap;
    RHI::TextureHandle m_prefilteredMap;
    RHI::TextureHandle m_brdfLut;
    RHI::SamplerHandle m_cubeSampler;
    RHI::SamplerHandle m_prefilterSampler;
    RHI::SamplerHandle m_lutSampler;

    StarryEngine::RHI::DescriptorSetHandle m_descriptorSet;
    StarryEngine::RHI::DescriptorPoolHandle m_descriptorPool;
    StarryEngine::RHI::DescriptorSetLayoutHandle m_descriptorSetLayout;

    std::shared_ptr<Assets::IBLBuilder> m_iblBuilder;
};

bool PBRDemo::loadModelGeometry(Assets::Geometry& outGeometry,std::vector<Assets::MaterialParams>& outParams,Assets::Skeleton* outSkeleton) {
    Assets::ModelLoader loader(m_rhi->getResourceManager());
    if (!loader.open("assets/models/fuxuan_Animation3.fbx")) {
        LOG_ERROR("Failed to load model");
        return false;
    }

    auto skeleton = loader.loadSkeleton();
    auto geometry = loader.loadGeometry();
    auto clip     = loader.loadClip();
    auto materials = loader.loadMaterials();
    if (!skeleton || !geometry || !clip) {
        LOG_ERROR("Failed to extract model data");
        return false;
    }

    *outSkeleton = std::move(*skeleton);
    m_modelClip = std::move(*clip);
    m_modelClip.looping = true;

    outParams = std::move(materials);
    outGeometry = std::move(*geometry);

    outGeometry.uploadToGPU();
    return true;
}

std::shared_ptr<Assets::MaterialInstance> PBRDemo::makeModelMaterial(
    const Assets::MaterialParams& param, bool skinned) {
    std::string fsPath;
    if (param.name == "face" || param.name == "脸" || param.name == "表情") fsPath = "assets/shaders/core/face.frag";
    else if (param.name == "hair" || param.name == "髪") fsPath = "assets/shaders/core/hair.frag";
    else fsPath = "assets/shaders/core/shader.frag";

    const char* vsPath = skinned ? "assets/shaders/core/shader_skinned.vert": "assets/shaders/core/shader.vert";

    auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(
        m_rhi->getResourceManager(), m_descriptorSetLayout);
    if (!tmpl->loadShaders(vsPath, fsPath)) {
        LOG_ERROR("Failed to load shaders for material: {}", param.name);
        return nullptr;
    }

    auto instance = std::make_shared<Assets::MaterialInstance>(
        tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);

    Assets::TextureLoader loader(m_rhi->getResourceManager());
    if (!param.albedoTexture.empty()) {
        std::string fileName = param.albedoTexture;
        auto slashPos = fileName.find_last_of("/\\");
        if (slashPos != std::string::npos) fileName = fileName.substr(slashPos + 1);
        std::string texPath = "assets/models/textures/" + m_modelTextureDir + "/" + fileName;
        auto texResult = loader.loadTexture2D(texPath, RHI::Format::RGBA8_UNorm);
        if (texResult.texture.isValid()) {
            instance->setTexture("texSampler", texResult.texture, texResult.sampler);
        }
        else {
            LOG_ERROR("Failed to load texture: {}", texPath);
        }
    }
    else {
        static const uint8_t kWhite[4] = { 255, 255, 255, 255 };
        auto white = loader.loadTextureFromMemory(kWhite, 1, 1, RHI::Format::RGBA8_UNorm, "White");
        if (white.texture.isValid())
            instance->setTexture("texSampler", white.texture, white.sampler);
    }

    instance->setSubpassTag("Forward_Opaque");
    instance->setDepthTest(true);
    instance->setDepthWrite(true);
    return instance;
}

// 描边副本材质：蒙皮顶点 + 高亮纯色 frag；stencil NotEqual 反选本体区域 → 只画外圈。
// 深度测试开（外圈被本体/前方物体正确遮挡）、深度写关；不写 stencil（writeMask=0）。
std::shared_ptr<Assets::MaterialInstance> PBRDemo::makeOutlineMaterial() {
    auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(
        m_rhi->getResourceManager(), m_descriptorSetLayout);
    if (!tmpl->loadShaders("assets/shaders/core/shader_skinned.vert", "assets/shaders/core/outline.frag")) {
        LOG_ERROR("Failed to load outline shaders");
        return nullptr;
    }
    auto mat = std::make_shared<Assets::MaterialInstance>(
        tmpl, m_descriptorPool, m_rhi->getResourceManager().get(), m_descriptorSet);

    Assets::TextureLoader loader(m_rhi->getResourceManager());
    static const uint8_t kWhite[4] = { 255, 255, 255, 255 };
    auto white = loader.loadTextureFromMemory(kWhite, 1, 1, RHI::Format::RGBA8_UNorm, "White");
    if (white.texture.isValid())
        mat->setTexture("texSampler", white.texture, white.sampler);

    mat->setSubpassTag("StencilTest");
    mat->setDepthTest(true);
    mat->setDepthWrite(false);

    RHI::StencilOpState outline;
    outline.failOp = RHI::StencilOp::Keep;
    outline.passOp = RHI::StencilOp::Keep;         // 只读 stencil，不改
    outline.depthFailOp = RHI::StencilOp::Keep;
    outline.compareOp = RHI::CompareOp::NotEqual;  // stencil != 1 → 本体外圈才通过
    outline.compareMask = 0xFF;
    outline.writeMask = 0x00;                      // 描边不写 stencil
    outline.reference = 1;
    mat->setStencilTest(true);
    mat->setStencilOps(outline, outline);
    return mat;
}

void PBRDemo::addGriseoModel() {
    auto geometry = std::make_shared<Assets::Geometry>(m_rhi->getResourceManager());
    std::vector<Assets::MaterialParams> params;
    if (!loadModelGeometry(*geometry, params, &m_modelSkeleton)) return;

    bool skinned = m_modelClip.isSkeletal() && !m_modelSkeleton.bones.empty();

    std::vector<std::shared_ptr<Assets::MaterialInstance>> materials;
    for (auto& param : params) {
        auto inst = makeModelMaterial(param, skinned);
        if (inst) materials.push_back(inst);
    }

    auto obj = std::make_shared<Scene::RenderObject>();
    obj->geometry = geometry;
    obj->materials = materials;
    obj->transform = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f));
    m_scene->addObject(obj);

    m_modelGeometry = geometry;
    m_modelMaterialCount = materials.size();

    m_skeletalReady = skinned;
    if (m_skeletalReady) {
        m_skinnedMaterials = materials;
        m_skeletalAnimator.updateSkeleton(m_modelSkeleton, m_modelClip, m_skeletalTime);
    }
}

void PBRDemo::onUpdate(float deltaTime) {
    if (!m_skeletalReady || m_skinnedMaterials.empty()) return;

    // 动画时间是 ticks，deltaTime 是秒 → 乘 ticksPerSecond 换算
    float tps = (m_modelClip.ticksPerSecond > 0.0f) ? m_modelClip.ticksPerSecond : 25.0f;
    // 非循环动画播到 duration 就停住（保持最后一帧，不再推进/绕回）
    if (m_modelClip.looping || m_skeletalTime < m_modelClip.duration) {
        m_skeletalTime += deltaTime * tps;
    }
    m_skeletalAnimator.updateSkeleton(m_modelSkeleton, m_modelClip, m_skeletalTime);
    const auto& matrices = m_skeletalAnimator.getBoneMatrices();
    if (matrices.empty()) return;

    size_t bytes = matrices.size() * sizeof(glm::mat4);
    for (auto& mat : m_skinnedMaterials) {
        if (mat) mat->setStorageBuffer(1, 2, matrices.data(), bytes);
    }
    // 描边副本复用同一批骨骼矩阵（同一几何同一 pose，仅模型矩阵放大）
    if (m_outlineMaterial) m_outlineMaterial->setStorageBuffer(1, 2, matrices.data(), bytes);
}

void PBRDemo::captureFrame() {
    if (!m_frameCapture) return;
    auto graph = m_renderPath ? m_renderPath->getRenderGraph() : nullptr;
    if (!graph) return;

    // 每次现解析：graph 重建（resize/rebuild）后 SceneColor 物理句柄会变
    auto texId = graph->getTextureId("SceneColor");
    auto handle = graph->getPhysicalTextureHandle(texId);
    auto* tex = m_rhi->getResourceManager()->getTexture(handle);
    m_frameCapture->capture(tex);
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
    // 默认 warn(静默);STARRY_VERBOSE=1 恢复 info(看 [perf] 帧数据 / 验证层信息时开)
    StarryEngine::Logger::setLevel(std::getenv("STARRY_VERBOSE") ? "info" : "warn");

    StarryEngine::Application::Config cfg;
    cfg.width = kWinW;
    cfg.height = kWinH;
    cfg.title = "Transparent Window Smoke Test";
    //cfg.resizable = false;
    //cfg.transparent = false;
    //cfg.borderless = true;
    //cfg.alwaysOnTop = true;

    //cfg.clickThrough = (std::getenv("STARRY_NO_CLICKTHROUGH") == nullptr);
    //cfg.nativeWayland = (std::getenv("STARRY_FORCE_X11") == nullptr);
    //LOG_INFO("[demo] 平台: {}（STARRY_FORCE_X11 强制 X11）", cfg.nativeWayland ? "原生 Wayland" : "X11");
    StarryEngine::Application app(cfg);

    // ── 性能测量（优化闭环第 1 步：先测基线，改完复测对比；关掉即恢复无痕）──
    // 开 GPU 时间戳查询；窗口标题每 1s 刷 FPS/GPU/CPU，日志每 5s 打一行平均/最大帧时间
    // 时间戳：enableTimestamps(true) 会触发首帧卡死 bug（见会话汇报），修复前保持关闭
    // if (auto frameCtx = app.getRenderHardwareInterface()->getFrameContext())
    //     frameCtx->enableTimestamps(true);

    auto demo = std::make_shared<PBRDemo>(app.getRenderHardwareInterface(), app.getGlobalDescriptorPool(), app.getWidth(), app.getHeight());

    app.setRenderer(demo->getRenderer());
    app.setScene(demo->getScene());

    // 帧序列捕获：每帧渲染+呈现完成后读回 SceneColor（STARRY_FRAME_DUMP 激活）
    app.setPostRenderCallback([demo]() { demo->captureFrame(); });
    auto perfLast = std::chrono::steady_clock::now();
    uint64_t perfLastFrames = 0;
    app.setUpdateCallback([demo, &app, &perfLast, &perfLastFrames](float deltaTime) {
        demo->onUpdate(deltaTime);
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - perfLast).count() >= 5.0) {
            double wall = std::chrono::duration<double>(now - perfLast).count();
            perfLast = now;
            auto fc = app.getRenderHardwareInterface()->getFrameContext();
            if (fc) {
                const auto& s = fc->getStatistics();
                uint64_t frames = s.totalFrames - perfLastFrames;
                perfLastFrames = s.totalFrames;
                // 真实 FPS 按墙钟 totalFrames 增量算;平均帧是渲染节奏(不含限帧睡眠),两者差即"每帧睡多少"
                LOG_INFO("[perf] 窗内帧={} 实际FPS={:.1f} | 平均帧={:.2f}ms 最大帧={:.2f}ms | CPU均={:.2f}ms GPU均={:.2f}ms",
                         frames, (wall > 0.0) ? static_cast<double>(frames) / wall : 0.0,
                         s.averageFrameTime, s.maxFrameTime,
                         s.averageCPUTime, s.averageGPUTime);
            }
        }
    });
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