#include <renderer/passes/PassWrapper.hpp>
#include <renderer/passes/GraphicsPass.hpp>
#include <renderer/passes/MeshPass.hpp>
#include <renderer/passes/ParticlePass.hpp>
#include <renderer/passes/ShadowPass.hpp>
#include <renderer/passes/ComputePass.hpp>
#include <scene/ParticleEmitter.hpp>
#include <renderer/renderPaths/DeferredRenderPath.hpp>
#include <renderer/passExecutor/SceneDrawExecutor.hpp>
#include <assets/geometry/GeometryGenerator.hpp>

#include <application/Application.hpp>
#include <core/JobSystem.hpp>
#include <event/Events.hpp>
#include <renderer/interface/vulkan/VulkanFrameContext.hpp>
#include <renderer/utils/FrameCapture.hpp>
#include "AsyncBoneProducer.hpp"

#include <stb_image_write.h>
#include <glm/gtc/packing.hpp>
#include <limits>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static bool g_clickThrough = true;

namespace {
    constexpr uint32_t kWinW = 1280;
    constexpr uint32_t kWinH = 720;

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

// ── 软光追离线渲染辅助：读回 + CPU 累积 + PNG ──
namespace {

    inline uint8_t clampToU8(float c) {
        return static_cast<uint8_t>(glm::clamp(c, 0.0f, 1.0f) * 255.0f + 0.5f);
    }

    // 读回 RGBA16F 纹理为 float RGB（SceneColor.alpha 是深度，只读 RGB）
    bool readbackTexture(RHI::IRHI* rhi, RHI::RHITexture* tex, uint32_t& w, uint32_t& h,
                         std::vector<float>& rgbOut) {
        if (!rhi || !tex) return false;
        auto resMgr = rhi->getResourceManager();
        auto extent = tex->getExtent();
        w = extent.width; h = extent.height;
        size_t pixelCount = static_cast<size_t>(w) * h;

        static RHI::BufferHandle s_sb;
        static uint32_t s_sbW = 0, s_sbH = 0;
        if (!s_sb.isValid() || s_sbW != w || s_sbH != h) {
            if (s_sb.isValid()) resMgr->destroy(s_sb);
            RHI::BufferDesc desc;
            desc.size = static_cast<uint64_t>(pixelCount) * 4 * 2;   // RGBA16F
            desc.type = RHI::BufferType::Staging;
            desc.memoryType = RHI::MemoryType::GPU_To_CPU;
            desc.allowReadback = true;
            desc.debugName = "OfflineCapture_Staging";
            s_sb = resMgr->createBuffer(desc);
            if (!s_sb.isValid()) { LOG_ERROR("离线：创建 staging 失败"); return false; }
            s_sbW = w; s_sbH = h;
        }

        RHI::ImageSubresourceRange range;
        range.aspectMask = RHI::ImageAspect::Color;
        range.baseMipLevel = 0; range.levelCount = 1;
        range.baseArrayLayer = 0; range.layerCount = 1;

        RHI::BufferImageCopyRegion region;
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource = range;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = extent;

        static uint32_t s_rbSlot = 0;
        uint32_t slot = s_rbSlot++ % 8;
        if (!rhi->beginAsyncReadback(slot, tex, resMgr->getBuffer(s_sb), region)) {
            LOG_ERROR("离线：beginAsyncReadback 失败");
            return false;
        }
        if (!rhi->waitAsyncReadback(slot, UINT64_MAX)) {
            LOG_ERROR("离线：读回 fence 未信号");
            return false;
        }

        auto* staging = resMgr->getBuffer(s_sb);
        auto* mapped = staging ? staging->map() : nullptr;
        if (!mapped) { rhi->releaseAsyncReadback(slot); return false; }

        const uint16_t* half = static_cast<const uint16_t*>(mapped);
        rgbOut.resize(pixelCount * 3);
        for (size_t i = 0; i < pixelCount; ++i) {
            rgbOut[i * 3 + 0] = glm::unpackHalf1x16(half[i * 4 + 0]);
            rgbOut[i * 3 + 1] = glm::unpackHalf1x16(half[i * 4 + 1]);
            rgbOut[i * 3 + 2] = glm::unpackHalf1x16(half[i * 4 + 2]);
        }
        staging->unmap();
        rhi->releaseAsyncReadback(slot);
        return true;
    }

    void writeAveragePNG(const std::string& path, uint32_t w, uint32_t h,
                         const std::vector<float>& acc, uint32_t frames, uint32_t spp) {
        size_t pixelCount = static_cast<size_t>(w) * h;
        std::vector<uint8_t> rgba8(pixelCount * 4);
        for (size_t i = 0; i < pixelCount; ++i) {
            rgba8[i * 4 + 0] = clampToU8(acc[i * 3 + 0] / static_cast<float>(frames));
            rgba8[i * 4 + 1] = clampToU8(acc[i * 3 + 1] / static_cast<float>(frames));
            rgba8[i * 4 + 2] = clampToU8(acc[i * 3 + 2] / static_cast<float>(frames));
            rgba8[i * 4 + 3] = 255;
        }
        bool ok = stbi_write_png(path.c_str(), static_cast<int>(w), static_cast<int>(h),
                                 4, rgba8.data(), static_cast<int>(w) * 4) != 0;
        if (ok) LOG_INFO("离线渲染完成: {} ({}x{}, 等效 {} spp)", path, w, h, frames * spp);
        else    LOG_ERROR("离线：PNG 写入失败: {}", path);
    }

} // namespace

class PBRDemo {
public:
    PBRDemo(std::shared_ptr<RHI::IRHI> rhi, RHI::DescriptorPoolHandle descriptorPool, uint32_t width, uint32_t height):
    m_rhi(rhi),m_descriptorPool(descriptorPool),m_width(width),m_height(height){
        // 软光追模式：STARRY_RT=1 或 STARRY_OFFLINE 切换（场景内置于 raytrace.comp，仅需相机）
        m_rtMode = std::getenv("STARRY_RT") != nullptr || std::getenv("STARRY_OFFLINE") != nullptr;
        m_offlineMode = std::getenv("STARRY_OFFLINE") != nullptr;
        m_offlineDenoise = std::getenv("STARRY_DENOISE") != nullptr;
        if (const char* op = std::getenv("STARRY_OFFLINE")) m_offlinePath = op;
        m_spp = 16u;
        if (const char* sp = std::getenv("STARRY_SPP")) { int v = std::atoi(sp); m_spp = (v > 0) ? static_cast<uint32_t>(v) : m_spp; }
        else if (m_offlineMode) m_spp = m_offlineDenoise ? 1u : 256u;
        m_offlineFrames = m_offlineDenoise ? 1u : 5u;
        if (const char* fr = std::getenv("STARRY_FRAMES")) { int v = std::atoi(fr); m_offlineFrames = (v > 0) ? static_cast<uint32_t>(v) : 1u; }

        if (m_rtMode) {
            createRayTracingRenderer();
        } else {
            m_iblBuilder = std::make_shared<Assets::IBLBuilder>(rhi->getResourceManager(), rhi);
            createRenderer();
            createScene();
        }
    }

    bool isRayTracingMode() const { return m_rtMode; }
    bool isOfflineMode() const { return m_offlineMode; }

    // 软光追渲染路径：半分辨率 RT → 时域累积 → 自适应 a-trous 降噪（GAMES202）
    void createRayTracingRenderer() {
        m_scene = std::make_shared<Scene::Scene>();

        m_renderer = std::make_shared<Renderer>(m_rhi, m_descriptorPool, m_scene);
        m_renderer->createGlobalSetLayout();
        m_renderer->createGlobalUniformBuffer();
        m_renderer->initDefaultMaterials();

        m_descriptorSetLayout = m_renderer->getGlobalSetLayout();
        m_descriptorSet = m_renderer->getGlobalDescriptorSet(0);

        auto renderPath = std::make_shared<DeferredRenderPath>(m_rhi, m_width, m_height);
        renderPath->setScene(m_scene.get());
        // 呈现输入：离线原始模式直接读 SceneColor（无降噪写 SceneColorFiltered → 链条完整防 cull 悬垂）；
        // 交互/离线降噪读 SceneColorFiltered（降噪结果，且保持 RT→累积→降噪→呈现全链可达不被裁剪）
        renderPath->setPresentInput((m_offlineMode && !m_offlineDenoise) ? "SceneColor" : "SceneColorFiltered");

        // 半分辨率 RT 输出纹理
        uint32_t rw = std::max(1u, m_width / 2), rh = std::max(1u, m_height / 2);
        auto colorDesc = PassWrapper::createColorTextureDesc({rw, rh, 1}, RHI::Format::RGBA16_Float);
        colorDesc.allowUnorderedAccess = true;
        auto accDesc = PassWrapper::createColorTextureDesc({rw, rh, 1}, RHI::Format::RGBA32_Float);
        accDesc.allowUnorderedAccess = true;
        auto dnDesc = PassWrapper::createColorTextureDesc({rw, rh, 1}, RHI::Format::RGBA16_Float);
        dnDesc.allowUnorderedAccess = true;
        // RT shader 已内置 Reinhard+gamma 编码 → 交换链必须 UNorm（sRGB 会二次编码 → 发白）
        auto swapDesc = PassWrapper::createColorTextureDesc({m_width, m_height, 1}, RHI::Format::BGRA8_UNorm);
        renderPath->addTextureDesc("SceneColor", colorDesc);
        renderPath->addTextureDesc("SceneColorAccum", accDesc);
        renderPath->addTextureDesc("SceneColorFiltered", dnDesc);
        renderPath->addTextureDesc("Swapchain", swapDesc);

        {
            PassList passes;

            // 路径追踪 pass：push constant = 相机 + sppInfo（x=SPP, y=帧种子, z=1 混帧号）
            ComputePassDesc rtDesc;
            rtDesc.name = "RayTracePass";
            rtDesc.shader = "assets/shaders/raytrace.comp";
            rtDesc.dispatchX = (rw + 15) / 16; rtDesc.dispatchY = (rh + 15) / 16;
            rtDesc.resources = {
                { RHI::DescriptorType::StorageImage, 0, "SceneColor", /*write=*/true, 0 }
            };
            rtDesc.pushConstantSize = 80;   // 4×vec4 相机 + sppInfo
            rtDesc.fillPushConstants = [scene = m_scene, spp = m_spp, frameSeed = 1.0f,
                                        frameCounter = std::make_shared<uint32_t>(0)](void* out, float) {
                auto cam = std::dynamic_pointer_cast<Scene::PerspectiveCamera>(scene->getActiveCamera());
                if (!cam) return;
                glm::mat4 view = cam->getViewMatrix();
                glm::vec3 right = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
                glm::vec3 up = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));
                glm::vec3 fwd = -glm::normalize(glm::vec3(view[0][2], view[1][2], view[2][2]));
                float tanHalf = std::tan(cam->getFov() * 0.5f);
                float aspect = cam->getAspect();
                struct RTUniforms { glm::vec4 pos, right, up, fwd, sppInfo; } pc;
                pc.pos   = glm::vec4(cam->getPosition(), 1.0f);
                pc.right = glm::vec4(right * tanHalf * aspect, 0.0f);
                pc.up    = glm::vec4(up * tanHalf, 0.0f);
                pc.fwd   = glm::vec4(fwd, 0.0f);
                uint32_t frameNo = (*frameCounter)++;
                uint32_t rnd = frameNo * 1664525u + 1013904223u;
                rnd ^= (rnd >> 13); rnd *= 0x5bd1e995u;
                pc.sppInfo = glm::vec4(static_cast<float>(spp), static_cast<float>(rnd), frameSeed, 0.0f);
                std::memcpy(out, &pc, sizeof(pc));
            };
            passes.push_back(std::make_shared<ComputePass>(rtDesc));

            // 时域累积 + 空域降噪：交互始终；离线降噪（STARRY_DENOISE）也启用
            if (!m_offlineMode || m_offlineDenoise) {
                ComputePassDesc accP;
                accP.name = "AccumulatePass";
                accP.shader = "assets/shaders/accumulate.comp";
                accP.dispatchX = (rw + 15) / 16; accP.dispatchY = (rh + 15) / 16;
                accP.resources = {
                    { RHI::DescriptorType::CombinedImageSampler, 0, "SceneColor", /*write=*/false, 0 },
                    { RHI::DescriptorType::StorageImage, 1, "SceneColorAccum", /*write=*/true, 0 }
                };
                accP.pushConstantSize = 4;
                accP.fillPushConstants = [scene = m_scene,
                                          last = std::make_shared<glm::mat4>(),
                                          first = std::make_shared<bool>(true)](void* out, float) {
                    uint32_t r = 0;
                    if (*first) { r = 1; *first = false; }
                    else {
                        auto cam = scene->getActiveCamera();
                        if (cam) {
                            glm::mat4 v = cam->getViewMatrix();
                            if (v != *last) { r = 1; *last = v; }
                        }
                    }
                    *static_cast<uint32_t*>(out) = r;
                };
                passes.push_back(std::make_shared<ComputePass>(accP));

                ComputePassDesc dnP;
                dnP.name = "DenoisePass";
                dnP.shader = "assets/shaders/denoise.comp";
                dnP.dispatchX = (rw + 15) / 16; dnP.dispatchY = (rh + 15) / 16;
                dnP.resources = {
                    { RHI::DescriptorType::CombinedImageSampler, 0, "SceneColorAccum", /*write=*/false, 0 },
                    { RHI::DescriptorType::CombinedImageSampler, 1, "SceneColor", /*write=*/false, 0 },
                    { RHI::DescriptorType::StorageImage, 2, "SceneColorFiltered", /*write=*/true, 0 }
                };
                dnP.pushConstantSize = 4;   // 自适应降噪强度：shader 用 count×spp 估计噪声
                dnP.fillPushConstants = [spp = m_spp](void* out, float) {
                    *static_cast<uint32_t*>(out) = spp;
                };
                passes.push_back(std::make_shared<ComputePass>(dnP));
            }
            renderPath->setPassList(std::move(passes));
        }

        m_renderPath = renderPath;
        m_renderer->setRenderPath(m_renderPath);

        // 相机（场景内置于 raytrace.comp）
        auto camera = std::make_shared<Scene::PerspectiveCamera>();
        camera->setPerspective(glm::radians(60.0f), static_cast<float>(m_width) / m_height, 0.1f, 100.0f);
        camera->lookAt(glm::vec3(0.03f, 1.31f, 4.0f), glm::vec3(0.0f, 0.7f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_scene->addCamera(camera);
        m_scene->setActiveCamera(camera);

        // 离线：跳过呈现（无 vsync 阻塞，读回不被 present 卡住）
        if (m_offlineMode) {
            if (auto fc = m_rhi->getFrameContext()) fc->setSkipPresent(true);
            LOG_INFO("离线模式：已跳过窗口呈现（无 vsync 阻塞）");
        }
    }

    // 离线读回：普通模式读 SceneColor 逐帧 CPU 累加；降噪模式读 SceneColorFiltered 只取最后一帧
    void offlineCapture() {
        if (!m_rtMode || !m_offlineMode || m_offlineDone) return;
        auto graph = m_renderPath ? m_renderPath->getRenderGraph() : nullptr;
        if (!graph) return;

        auto texId = graph->getTextureId(m_offlineDenoise ? "SceneColorFiltered" : "SceneColor");
        auto handle = graph->getPhysicalTextureHandle(texId);
        auto* tex = m_rhi->getResourceManager()->getTexture(handle);
        std::vector<float> frame;
        uint32_t w = 0, h = 0;
        if (!tex || !readbackTexture(m_rhi.get(), tex, w, h, frame)) {
            LOG_ERROR("离线：第 {} 帧读回失败", m_offlineFrameIdx);
            m_offlineDone = true;
            if (m_requestExit) m_requestExit();
            return;
        }
        if (m_offlineDenoise) {
            ++m_offlineFrameIdx;
            if (m_offlineFrameIdx >= m_offlineFrames) {
                m_offlineDone = true;
                writeAveragePNG(m_offlinePath, w, h, frame, 1, m_spp);
                if (m_requestExit) m_requestExit();
            }
        } else {
            if (m_offlineAcc.empty()) { m_offlineAcc.assign(frame.size(), 0.0f); m_offlineW = w; m_offlineH = h; }
            for (size_t i = 0; i < frame.size(); ++i) m_offlineAcc[i] += frame[i];
            ++m_offlineFrameIdx;
            if (m_offlineFrameIdx % 10 == 0 || m_offlineFrameIdx >= m_offlineFrames) {
                LOG_INFO("离线：已累加 {}/{} 帧", m_offlineFrameIdx, m_offlineFrames);
            }
            if (m_offlineFrameIdx >= m_offlineFrames) {
                m_offlineDone = true;
                writeAveragePNG(m_offlinePath, m_offlineW, m_offlineH, m_offlineAcc, m_offlineFrameIdx, m_spp);
                if (m_requestExit) m_requestExit();
            }
        }
    }

    void createRenderer() {
        m_scene = std::make_shared<Scene::Scene>();

        m_renderer = std::make_shared<Renderer>(m_rhi, m_descriptorPool, m_scene);
        m_renderer->createGlobalSetLayout();
        m_renderer->createGlobalUniformBuffer();
        m_renderer->initDefaultMaterials();

        m_descriptorSetLayout = m_renderer->getGlobalSetLayout();
        m_descriptorSet = m_renderer->getGlobalDescriptorSet(0);

        auto renderPath = std::make_shared<DeferredRenderPath>(m_rhi, m_width, m_height);
        renderPath->setScene(m_scene.get());   

        auto colorDesc = PassWrapper::createColorTextureDesc({m_width, m_height, 1}, RHI::Format::RGBA16_Float);
        auto depthDesc = PassWrapper::createDepthTextureDesc({m_width, m_height, 1}, RHI::Format::D24_UNorm_S8_UInt);
        auto swapDesc   = PassWrapper::createColorTextureDesc({m_width, m_height, 1}, RHI::Format::BGRA8_sRGB);
        auto shadowMapDesc = PassWrapper::createDepthTextureDesc({ShadowPass::kShadowMapSize, ShadowPass::kShadowMapSize, 1}, RHI::Format::D24_UNorm_S8_UInt);
        renderPath->addTextureDesc("SceneColor",  colorDesc);
        renderPath->addTextureDesc("Depth",       depthDesc);
        renderPath->addTextureDesc("Swapchain",   swapDesc);
        renderPath->addTextureDesc("ShadowMap",   shadowMapDesc);

        {
            PassList passes;

            passes.push_back(std::make_shared<ShadowPass>("ShadowPass"));

            auto forwardPass = std::make_shared<MeshPass>("ForwardPass", "Forward_Opaque",RHI::Color::Transparent());
            forwardPass->addReadTextureByName("ShadowMap");  
            passes.push_back(forwardPass);

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

        {
            glm::vec3 lightDir = glm::normalize(glm::vec3(0.5f, 1.0f, 0.8f));  
            glm::vec3 sceneCenter(0.0f, 1.0f, 0.0f);

            float halfExtent = 12.0f;
            float dist = 12.0f;

            glm::vec3 eye = sceneCenter + lightDir * dist;
            glm::mat4 view = glm::lookAt(eye, sceneCenter, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 proj = glm::orthoRH_ZO(-halfExtent, halfExtent, -halfExtent, halfExtent, 0.1f, 26.0f);
            proj[1][1] *= -1.0f;  
            m_renderer->setLightViewProj(proj * view);
        }

        if (const char* dumpDir = std::getenv("STARRY_FRAME_DUMP")) {
            const char* cnt = std::getenv("STARRY_FRAME_COUNT");
            const char* idx = std::getenv("STARRY_FRAME_INDEX");
            uint32_t n = cnt ? static_cast<uint32_t>(std::atoi(cnt)) : 1u;
            uint32_t fi = idx ? static_cast<uint32_t>(std::atoi(idx)) : UINT32_MAX;
            m_frameExit = std::getenv("STARRY_FRAME_EXIT") != nullptr;
            m_frameExitTarget = (fi != UINT32_MAX) ? 1u : (n > 0 ? n : UINT32_MAX);
            m_frameCapture = std::make_shared<FrameCapture>(m_rhi);
            if (!m_frameCapture->initialize({ dumpDir, n, fi })) {
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

        {        
            auto skyboxEffect = std::make_shared<Scene::ProceduralEffect>();
            skyboxEffect->material = createSkyboxMaterial();
            m_scene->addProceduralEffect(skyboxEffect);

            auto mat = createTexturedPbrMaterial();
            auto sphere = std::make_shared<Scene::RenderObject>();
            sphere->geometry = Assets::GeometryGenerator::createSphere(m_rhi->getResourceManager(), 1.0f);
            sphere->materials = { mat };
            sphere->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.5f, 0.0f));

            {
                auto clip = std::make_shared<Assets::AnimationClip>();
                clip->name = "SpinBob";
                clip->duration = 4.0f;   
                clip->looping = true;
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

        {
            for (auto& mat : m_skinnedMaterials) {
                if (!mat) continue;
                mat->setSubpassTag("StencilWrite");
                RHI::StencilOpState write;
                write.failOp = RHI::StencilOp::Keep;
                write.passOp = RHI::StencilOp::Replace; 
                write.depthFailOp = RHI::StencilOp::Keep;
                write.compareOp = RHI::CompareOp::Always;
                write.compareMask = 0xFF;
                write.writeMask = 0xFF;
                write.reference = 1;
                mat->setStencilTest(true);
                mat->setStencilOps(write, write);
            }

            bool outlineOn = (std::getenv("STARRY_NO_OUTLINE") == nullptr);
            m_outlineMaterial = outlineOn ? makeOutlineMaterial() : nullptr;
            if (m_outlineMaterial && m_modelGeometry) {
                auto outline = std::make_shared<Scene::RenderObject>();
                outline->geometry = m_modelGeometry;

                std::vector<std::shared_ptr<Assets::MaterialInstance>> outlineMats(m_modelMaterialCount, m_outlineMaterial);
                outline->materials = std::move(outlineMats);

                float footY = computeModelFootY(m_modelGeometry);
                LOG_INFO("[demo] 描边缩放中心（脚底）Y = {:.3f}", footY);
                outline->transform =
                    glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, footY, 0.0f))
                    * glm::scale(glm::mat4(1.0f), glm::vec3(1.03f))
                    * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -footY, 0.0f));
                m_scene->addObject(outline);
            }
        }

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

        if (m_boneProducer && m_renderer && m_renderer->getJobSystem()) {
            const auto& m0 = m_boneProducer->getFrame0Matrices();
            if (!m0.empty()) {
                size_t bytes = m0.size() * sizeof(glm::mat4);
                for (auto& mat : m_skinnedMaterials) if (mat) mat->setStorageBuffer(1, 2, m0.data(), bytes);
                if (m_outlineMaterial) m_outlineMaterial->setStorageBuffer(1, 2, m0.data(), bytes);
            }
        }
    }

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
        blend.blendEnable = true;   
        blend.dstAlphaBlendFactor = RHI::BlendFactor::OneMinusSrcAlpha; 
        mat->setAttachments({ blend });
        mat->setDepthTest(false);
        mat->setDepthWrite(false);
        return mat;
    }

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

    void onUpdate(float deltaTime);
    void captureFrame();

    void setExitCallback(std::function<void()> cb) { m_requestExit = std::move(cb); }
private:
    bool loadModelGeometry(Assets::Geometry& outGeometry,std::vector<Assets::MaterialParams>& outParams,Assets::Skeleton* outSkeleton = nullptr);
    std::shared_ptr<Assets::MaterialInstance> makeModelMaterial(const Assets::MaterialParams& param, bool skinned);
    std::shared_ptr<Assets::MaterialInstance> makeOutlineMaterial();
    void addGriseoModel();

    Assets::Skeleton m_modelSkeleton;
    Assets::AnimationClip m_modelClip;
    std::string m_modelTextureDir = "fuxuan";  

    std::unique_ptr<AsyncBoneProducer> m_boneProducer;
    bool m_skeletalReady = false;
    float m_lastDelta = 0.0f;   
    std::vector<std::shared_ptr<Assets::MaterialInstance>> m_skinnedMaterials;
    std::shared_ptr<Assets::Geometry> m_modelGeometry;        
    size_t m_modelMaterialCount = 0;                            
    std::shared_ptr<Assets::MaterialInstance> m_outlineMaterial; 

    uint32_t m_width, m_height;
    std::shared_ptr<RHI::IRHI> m_rhi;
    std::shared_ptr<Renderer> m_renderer;
    std::shared_ptr< Scene::Scene> m_scene;

    std::shared_ptr<DeferredRenderPath> m_renderPath;
    std::shared_ptr<FrameCapture> m_frameCapture;
    bool m_frameExit = false;       
    uint32_t m_frameExitTarget = 0;   
    std::function<void()> m_requestExit;   

    // 软光追 / 离线渲染状态
    bool m_rtMode = false;
    bool m_offlineMode = false;
    bool m_offlineDenoise = false;
    std::string m_offlinePath;
    uint32_t m_offlineFrames = 5;
    uint32_t m_offlineFrameIdx = 0;
    uint32_t m_spp = 16;
    uint32_t m_offlineW = 0, m_offlineH = 0;
    std::vector<float> m_offlineAcc;
    bool m_offlineDone = false;

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
    outline.passOp = RHI::StencilOp::Keep;       
    outline.depthFailOp = RHI::StencilOp::Keep;
    outline.compareOp = RHI::CompareOp::NotEqual;  
    outline.compareMask = 0xFF;
    outline.writeMask = 0x00;                     
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

    if (const char* ec = std::getenv("STARRY_EXTRA_CHARS")) {
        uint32_t n = std::strtoul(ec, nullptr, 10);
        for (uint32_t i = 0; i < n; ++i) {
            auto clone = std::make_shared<Scene::RenderObject>();
            clone->geometry = geometry;
            clone->materials = materials;
            clone->transform = glm::translate(glm::mat4(1.0f),
                glm::vec3(2.0f + static_cast<float>(i + 1) * 2.2f, 0.0f, 0.0f));
            m_scene->addObject(clone);
        }
        LOG_INFO("[demo] 额外角色副本 x{}（draw call 基准）", n);
    }

    m_modelGeometry = geometry;
    m_modelMaterialCount = materials.size();

    m_skeletalReady = skinned;
    if (m_skeletalReady) {
        m_skinnedMaterials = materials;

        AsyncBoneProducer::Config cfg;
        const bool poolActive = (m_renderer && m_renderer->getJobSystem() != nullptr);
        cfg.enableThread = !poolActive && (std::getenv("STARRY_ASYNC_BONES") != nullptr);
        m_boneProducer = std::make_unique<AsyncBoneProducer>(m_modelSkeleton, m_modelClip, cfg);
        LOG_INFO("[async] 骨骼动画: {}",poolActive ? "池（帧内并行 job）": (cfg.enableThread ? "ON（独立线程）" : "OFF（同步内联）"));

        if (m_renderer && m_renderer->isFrameInFlight()) {
            auto* js = m_renderer->getJobSystem();
            m_renderer->setFrameDataBoneProvider([this, js](uint32_t dataSlot) {
                float d = m_lastDelta;  
                js->submit([this, d, dataSlot]() {
                    const auto& matrices = m_boneProducer->step(d);  
                    if (matrices.empty()) return;
                    size_t bytes = matrices.size() * sizeof(glm::mat4);
                    for (auto& mat : m_skinnedMaterials) if (mat) mat->setStorageBuffer(1, 2, matrices.data(), bytes, dataSlot);
                    if (m_outlineMaterial) m_outlineMaterial->setStorageBuffer(1, 2, matrices.data(), bytes, dataSlot);
                });
            });
        }
    }
}

void PBRDemo::onUpdate(float deltaTime) {
    if (!m_boneProducer || m_skinnedMaterials.empty()) return;

    m_lastDelta = deltaTime;
    if (m_renderer && m_renderer->isFrameInFlight()) return;

    uint32_t targetSlot = m_rhi->getCurrentFrameIndex();
    if (targetSlot >= RHI::kMaxFramesInFlight) targetSlot = 0;

    auto upload = [this, targetSlot](const std::vector<glm::mat4>& matrices) {
        if (matrices.empty()) return;
        size_t bytes = matrices.size() * sizeof(glm::mat4);
        for (auto& mat : m_skinnedMaterials) {
            if (mat) mat->setStorageBuffer(1, 2, matrices.data(), bytes, targetSlot);
        }
        if (m_outlineMaterial) m_outlineMaterial->setStorageBuffer(1, 2, matrices.data(), bytes, targetSlot);
    };

    if (auto* js = (m_renderer ? m_renderer->getJobSystem() : nullptr)) {
        js->submit([this, delta = deltaTime, upload]() {
            const auto& matrices = m_boneProducer->step(delta); 
            upload(matrices);
        });
        return;
    }

    const auto& matrices = m_boneProducer->step(deltaTime);
    upload(matrices);
}

void PBRDemo::captureFrame() {
    if (!m_frameCapture) return;
    auto graph = m_renderPath ? m_renderPath->getRenderGraph() : nullptr;
    if (!graph) return;

    auto texId = graph->getTextureId("SceneColor");
    auto handle = graph->getPhysicalTextureHandle(texId);
    auto* tex = m_rhi->getResourceManager()->getTexture(handle);
    m_frameCapture->capture(tex);

    if (m_frameExit && m_frameExitTarget != UINT32_MAX &&
        m_frameCapture->capturedCount() >= m_frameExitTarget) {
        m_frameExit = false;   // 只触发一次
        if (m_requestExit) m_requestExit();
    }
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
    StarryEngine::Logger::setLevel("info");

    StarryEngine::Application::Config cfg;
    cfg.width = kWinW;
    cfg.height = kWinH;

    if (const char* w = std::getenv("STARRY_WIN_W")) cfg.width = std::strtoul(w, nullptr, 10);
    if (const char* h = std::getenv("STARRY_WIN_H")) cfg.height = std::strtoul(h, nullptr, 10);
    cfg.title = "Transparent Window Smoke Test";
    //cfg.resizable = false;
    //cfg.transparent = false;
    //cfg.borderless = true;
    //cfg.alwaysOnTop = true;

    //cfg.clickThrough = (std::getenv("STARRY_NO_CLICKTHROUGH") == nullptr);
    //cfg.nativeWayland = (std::getenv("STARRY_FORCE_X11") == nullptr);
    //LOG_INFO("[demo] 平台: {}（STARRY_FORCE_X11 强制 X11）", cfg.nativeWayland ? "原生 Wayland" : "X11");
    StarryEngine::Application app(cfg);

    auto demo = std::make_shared<PBRDemo>(app.getRenderHardwareInterface(), app.getGlobalDescriptorPool(), app.getWidth(), app.getHeight());
    demo->setExitCallback([&app]() { app.shutdown(); });  

    app.setRenderer(demo->getRenderer());
    app.setScene(demo->getScene());

    const char* autoQuitEnv = std::getenv("STARRY_AUTO_QUIT");
    uint64_t autoQuitFrames = autoQuitEnv ? std::strtoull(autoQuitEnv, nullptr, 10) : 0;
    uint64_t frameCounter = 0;
    app.setPostRenderCallback([demo, &app, &frameCounter, autoQuitFrames]() {
        if (demo->isRayTracingMode()) {
            demo->offlineCapture();   // 离线读回（交互 RT 下为空操作）
        } else {
            demo->captureFrame();
        }
        if (autoQuitFrames > 0 && ++frameCounter >= autoQuitFrames) app.shutdown();
    });
    auto perfLast = std::chrono::steady_clock::now();
    uint64_t perfLastFrames = 0;
    app.setUpdateCallback([demo, &app, &perfLast, &perfLastFrames](float deltaTime) {
        demo->onUpdate(deltaTime);

        // RT 模式：窗口标题实时显示 lookAt 三参数 + 欧拉角 + FOV（可直接粘回代码调参）
        if (demo->isRayTracingMode()) {
            static float titleTimer = 0.0f;
            titleTimer += deltaTime;
            if (titleTimer >= 0.1f) {
                titleTimer = 0.0f;
                auto scene = demo->getScene();
                if (scene && scene->getActiveCamera()) {
                    auto cam = scene->getActiveCamera();
                    glm::mat4 view = cam->getViewMatrix();
                    glm::vec3 eye = cam->getPosition();
                    glm::vec3 fwd = -glm::normalize(glm::vec3(view[0][2], view[1][2], view[2][2]));
                    glm::vec3 up  =  glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));
                    glm::vec3 right = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
                    glm::vec3 center = eye + fwd;
                    float yaw   = glm::degrees(std::atan2(fwd.z, fwd.x));
                    float pitch = glm::degrees(std::asin(glm::clamp(fwd.y, -1.0f, 1.0f)));
                    float roll  = glm::degrees(std::asin(glm::clamp(glm::dot(right, glm::vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f)));
                    float fovDeg = 60.0f;
                    if (auto pc = std::dynamic_pointer_cast<Scene::PerspectiveCamera>(cam)) fovDeg = glm::degrees(pc->getFov());
                    char title[256];
                    std::snprintf(title, sizeof(title),
                        "StarryEngine RT | lookAt((%.2f,%.2f,%.2f),(%.2f,%.2f,%.2f),(%.2f,%.2f,%.2f)) | yaw=%.1f pitch=%.1f roll=%.1f | fov=%.0f",
                        eye.x, eye.y, eye.z, center.x, center.y, center.z, up.x, up.y, up.z,
                        yaw, pitch, roll, fovDeg);
                    glfwSetWindowTitle(app.getWindow()->getHandle(), title);
                }
            }
        }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - perfLast).count() >= 5.0) {
            double wall = std::chrono::duration<double>(now - perfLast).count();
            perfLast = now;
            auto fc = app.getRenderHardwareInterface()->getFrameContext();
            if (fc) {
                const auto& s = fc->getStatistics();
                uint64_t frames = s.totalFrames - perfLastFrames;
                perfLastFrames = s.totalFrames;
 
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