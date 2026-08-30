#include <renderer/renderPaths/RayTracingRenderPath.hpp>
#include <renderer/passes/ComputePass.hpp>
#include <renderer/passes/PassWrapper.hpp>
#include <scene/camera/PerspectiveCamera.hpp>
#include <logging/Logger.hpp>
#include <cstring>

namespace StarryEngine {

    RayTracingRenderPath::RayTracingRenderPath(std::shared_ptr<RHI::IRHI> rhi,
        uint32_t width, uint32_t height, const Config& cfg)
        : BaseRenderPath(rhi, width, height), m_cfg(cfg) {

        // 半分辨率 RT 输出纹理
        uint32_t rw = std::max(1u, width / 2), rh = std::max(1u, height / 2);
        auto colorDesc = PassWrapper::createColorTextureDesc({rw, rh, 1}, RHI::Format::RGBA16_Float);
        colorDesc.allowUnorderedAccess = true;
        auto accDesc = PassWrapper::createColorTextureDesc({rw, rh, 1}, RHI::Format::RGBA32_Float);
        accDesc.allowUnorderedAccess = true;
        auto dnDesc = PassWrapper::createColorTextureDesc({rw, rh, 1}, RHI::Format::RGBA16_Float);
        dnDesc.allowUnorderedAccess = true;
        auto swapDesc = PassWrapper::createColorTextureDesc({width, height, 1}, RHI::Format::BGRA8_UNorm);
        addTextureDesc("SceneColor", colorDesc);
        addTextureDesc("SceneColorAccum", accDesc);
        addTextureDesc("SceneColorFiltered", dnDesc);
        addTextureDesc("Swapchain", swapDesc);

        // 呈现输入：离线原始读 SceneColor（无降噪写 SceneColorFiltered → 链条完整防 cull 悬垂）；
        // 交互/离线降噪读 SceneColorFiltered（且保持 RT→累积→降噪→呈现全链可达不被裁剪）
        setPresentInput((m_cfg.offline && !m_cfg.denoise) ? "SceneColor" : "SceneColorFiltered");
    }

    void RayTracingRenderPath::buildConfigPasses(
        std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap)
    {
        // 1) 构建 RT/累积/降噪 pass 列表（本路径无光栅 draw items）
        uint32_t rw = std::max(1u, m_width / 2), rh = std::max(1u, m_height / 2);
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
        rtDesc.fillPushConstants = [this, spp = m_cfg.spp,
                                    frameCounter = std::make_shared<uint32_t>(0)](void* out, float) {
            auto scene = m_blackboard.get<Scene::Scene*>() ? *m_blackboard.get<Scene::Scene*>() : nullptr;
            if (!scene) return;
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
            pc.sppInfo = glm::vec4(static_cast<float>(spp), static_cast<float>(rnd), 1.0f, 0.0f);
            std::memcpy(out, &pc, sizeof(pc));
        };
        passes.push_back(std::make_shared<ComputePass>(rtDesc));

        // 时域累积 + 空域降噪：交互始终；离线降噪也启用（离线原始跳过）
        if (!m_cfg.offline || m_cfg.denoise) {
            ComputePassDesc accP;
            accP.name = "AccumulatePass";
            accP.shader = "assets/shaders/accumulate.comp";
            accP.dispatchX = (rw + 15) / 16; accP.dispatchY = (rh + 15) / 16;
            accP.resources = {
                { RHI::DescriptorType::CombinedImageSampler, 0, "SceneColor", /*write=*/false, 0 },
                { RHI::DescriptorType::StorageImage, 1, "SceneColorAccum", /*write=*/true, 0 }
            };
            accP.pushConstantSize = 4;
            accP.fillPushConstants = [this,
                                      last = std::make_shared<glm::mat4>(),
                                      first = std::make_shared<bool>(true)](void* out, float) {
                uint32_t r = 0;
                if (*first) { r = 1; *first = false; }
                else {
                    auto scene = m_blackboard.get<Scene::Scene*>() ? *m_blackboard.get<Scene::Scene*>() : nullptr;
                    if (scene) {
                        auto cam = scene->getActiveCamera();
                        if (cam) {
                            glm::mat4 v = cam->getViewMatrix();
                            if (v != *last) { r = 1; *last = v; }
                        }
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
            dnP.fillPushConstants = [spp = m_cfg.spp](void* out, float) {
                *static_cast<uint32_t*>(out) = spp;
            };
            passes.push_back(std::make_shared<ComputePass>(dnP));
        }

        setPassList(std::move(passes));

        // 2) 配置每个 pass（注册到渲染图节点；compute pass 无 subpass，跳过 tag 收集）
        for (auto& pass : m_passes) {
            pass->setDataProvider(&m_blackboard);
            if (!pass->configure(*m_renderGraph, texIdMap, m_width, m_height, m_resMgr, m_globalSetLayout)) {
                LOG_ERROR("Pass '{}' configuration failed — skipping", pass->getName());
                continue;
            }
        }
    }

    void RayTracingRenderPath::onAfterCompile() {
        // 编译后创建每个 compute pass 的管线/描述符/执行器（基类默认空实现）
        IPass::CompileContext ctx;
        ctx.resMgr = m_resMgr;
        ctx.rhi = m_rhi;
        ctx.renderGraph = m_renderGraph.get();
        ctx.globalDescSets = m_globalDescSets;
        ctx.globalSetLayout = m_globalSetLayout;

        for (auto& pass : m_passes) {
            pass->onAfterCompile(ctx);
        }
    }

} // namespace StarryEngine
