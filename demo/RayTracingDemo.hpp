#pragma once
// 软光追 demo：RayTracingRenderer + RayTracingRenderPath + 离线读回/PNG + 标题栏
#include <renderer/RayTracingRenderer.hpp>
#include <renderer/renderPaths/RayTracingRenderPath.hpp>
#include <renderer/interface/RHIFactory.hpp>
#include <scene/Scene.hpp>
#include <scene/camera/PerspectiveCamera.hpp>
#include <logging/Logger.hpp>
#include <stb_image_write.h>
#include <glm/gtc/packing.hpp>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <functional>
#include <cstdio>
#include <cstring>

namespace StarryEngine {

    namespace rtdetail {
        inline uint8_t clampToU8(float c) {
            return static_cast<uint8_t>(glm::clamp(c, 0.0f, 1.0f) * 255.0f + 0.5f);
        }

        // 读回 RGBA16F 纹理为 float RGB（SceneColor.alpha 是深度，只读 RGB）
        inline bool readbackTexture(RHI::IRHI* rhi, RHI::RHITexture* tex, uint32_t& w, uint32_t& h,
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

        inline void writeAveragePNG(const std::string& path, uint32_t w, uint32_t h,
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
    } // namespace rtdetail

    // ── 软件路径追踪 demo ──
    class RayTracingDemo {
    public:
        RayTracingDemo(std::shared_ptr<RHI::IRHI> rhi, RHI::DescriptorPoolHandle descriptorPool,
                       uint32_t width, uint32_t height)
            : m_rhi(rhi), m_descriptorPool(descriptorPool), m_width(width), m_height(height) {
            // 环境参数
            m_offlineMode = std::getenv("STARRY_OFFLINE") != nullptr;
            m_offlineDenoise = std::getenv("STARRY_DENOISE") != nullptr;
            if (const char* op = std::getenv("STARRY_OFFLINE")) m_offlinePath = op;
            m_spp = m_offlineMode ? 256u : 64u;   // 交互默认 64 spp（降噪尽早停止滤波，画面更接近离线）；离线默认 256
            if (const char* sp = std::getenv("STARRY_SPP")) { int v = std::atoi(sp); m_spp = (v > 0) ? static_cast<uint32_t>(v) : m_spp; }
            else if (m_offlineMode && m_offlineDenoise) m_spp = 1u;
            m_offlineFrames = m_offlineDenoise ? 1u : 5u;
            if (const char* fr = std::getenv("STARRY_FRAMES")) { int v = std::atoi(fr); m_offlineFrames = (v > 0) ? static_cast<uint32_t>(v) : 1u; }

            m_scene = std::make_shared<Scene::Scene>();
            m_renderer = std::make_shared<RayTracingRenderer>(rhi, m_descriptorPool, m_scene);
            m_renderer->createGlobalSetLayout();
            m_renderer->createGlobalUniformBuffer();

            RayTracingRenderPath::Config cfg;
            cfg.spp = m_spp;
            cfg.denoise = !m_offlineMode || m_offlineDenoise;
            cfg.offline = m_offlineMode;
            auto renderPath = std::make_shared<RayTracingRenderPath>(rhi, width, height, cfg);
            renderPath->setScene(m_scene.get());
            m_renderer->setRenderPath(renderPath);

            // 相机（场景内置于 raytrace.comp）
            auto camera = std::make_shared<Scene::PerspectiveCamera>();
            camera->setPerspective(glm::radians(60.0f), static_cast<float>(width) / height, 0.1f, 100.0f);
            camera->lookAt(glm::vec3(0.03f, 1.31f, 4.0f), glm::vec3(0.0f, 0.7f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            m_scene->addCamera(camera);
            m_scene->setActiveCamera(camera);

            // 离线：跳过呈现（无 vsync 阻塞，读回不被 present 卡住）
            if (m_offlineMode) {
                if (auto fc = rhi->getFrameContext()) fc->setSkipPresent(true);
                LOG_INFO("离线模式：已跳过窗口呈现（无 vsync 阻塞）");
            }
        }

        std::shared_ptr<RayTracingRenderer> getRenderer() { return m_renderer; }
        std::shared_ptr<Scene::Scene> getScene() { return m_scene; }
        bool isOfflineMode() const { return m_offlineMode; }
        void setExitCallback(std::function<void()> fn) { m_requestExit = std::move(fn); }

        // 离线读回：普通模式逐帧 CPU 累加读 SceneColor；降噪模式只取最后一帧读 SceneColorFiltered
        void offlineCapture() {
            if (!m_offlineMode || m_offlineDone) return;
            auto rp = m_renderer->getRenderPath();
            auto* bp = dynamic_cast<BaseRenderPath*>(rp.get());
            auto graph = bp ? bp->getRenderGraph() : nullptr;
            if (!graph) return;

            auto texId = graph->getTextureId(m_offlineDenoise ? "SceneColorFiltered" : "SceneColor");
            auto handle = graph->getPhysicalTextureHandle(texId);
            auto* tex = m_rhi->getResourceManager()->getTexture(handle);
            std::vector<float> frame;
            uint32_t w = 0, h = 0;
            if (!tex || !rtdetail::readbackTexture(m_rhi.get(), tex, w, h, frame)) {
                LOG_ERROR("离线：第 {} 帧读回失败", m_offlineFrameIdx);
                m_offlineDone = true;
                if (m_requestExit) m_requestExit();
                return;
            }
            if (m_offlineDenoise) {
                ++m_offlineFrameIdx;
                if (m_offlineFrameIdx >= m_offlineFrames) {
                    m_offlineDone = true;
                    rtdetail::writeAveragePNG(m_offlinePath, w, h, frame, 1, m_spp);
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
                    rtdetail::writeAveragePNG(m_offlinePath, m_offlineW, m_offlineH, m_offlineAcc, m_offlineFrameIdx, m_spp);
                    if (m_requestExit) m_requestExit();
                }
            }
        }

        // 标题栏实时显示 lookAt 三参数 + 欧拉角 + FOV（可直接粘回代码调参）
        void updateTitle(GLFWwindow* win, float deltaTime) {
            static float titleTimer = 0.0f;
            titleTimer += deltaTime;
            if (titleTimer < 0.1f) return;
            titleTimer = 0.0f;
            auto cam = m_scene->getActiveCamera();
            if (!cam) return;
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
            glfwSetWindowTitle(win, title);
        }

    private:
        std::shared_ptr<RHI::IRHI> m_rhi;
        RHI::DescriptorPoolHandle m_descriptorPool;
        uint32_t m_width, m_height;
        std::shared_ptr<RayTracingRenderer> m_renderer;
        std::shared_ptr<Scene::Scene> m_scene;
        std::function<void()> m_requestExit;

        bool m_offlineMode = false;
        bool m_offlineDenoise = false;
        std::string m_offlinePath;
        uint32_t m_offlineFrames = 5;
        uint32_t m_offlineFrameIdx = 0;
        uint32_t m_spp = 16;
        uint32_t m_offlineW = 0, m_offlineH = 0;
        std::vector<float> m_offlineAcc;
        bool m_offlineDone = false;
    };

} // namespace StarryEngine
