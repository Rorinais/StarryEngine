#include <renderer/utils/FrameCapture.hpp>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>

#include <logging/Logger.hpp>
#include <renderer/interface/RHI_RESOURCE_MANAGER.hpp>
#include <stb_image_write.h>

namespace StarryEngine {

    namespace {
        // 线性 → sRGB 8bit（与 swapchain BGRA8_sRGB 硬件编码一致，得到屏幕所见）
        inline uint8_t linearToSrgb8(float c) {
            c = glm::clamp(c, 0.0f, 1.0f);
            float s = (c <= 0.0031308f) ? (12.92f * c)
                                        : (1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f);
            return static_cast<uint8_t>(s * 255.0f + 0.5f);
        }

        inline uint8_t linearToUint8(float c) {
            return static_cast<uint8_t>(glm::clamp(c, 0.0f, 1.0f) * 255.0f + 0.5f);
        }
    }

    FrameCapture::FrameCapture(std::shared_ptr<RHI::ResourceManager> resMgr)
        : m_resMgr(std::move(resMgr)) {}

    FrameCapture::~FrameCapture() {
        destroyStaging();
    }

    bool FrameCapture::initialize(const Config& cfg) {
        m_cfg = cfg;
        if (m_cfg.outputDir.empty()) {
            LOG_WARN("[FrameCapture] 输出目录为空，帧捕获未激活");
            return false;
        }
        std::error_code ec;
        if (!std::filesystem::create_directories(m_cfg.outputDir, ec) && ec) {
            LOG_ERROR("[FrameCapture] 创建输出目录失败: {} ({})", m_cfg.outputDir, ec.message());
            return false;
        }
        m_active = true;
        LOG_INFO("[FrameCapture] 帧序列输出 → {}（{}）", m_cfg.outputDir,
                 m_cfg.frameCount == 0 ? "持续录制" : (std::to_string(m_cfg.frameCount) + " 帧"));
        return true;
    }

    bool FrameCapture::ensureStaging(const RHI::Extent3D& extent) {
        uint32_t w = extent.width, h = extent.height;
        if (m_staging.isValid() && m_width == w && m_height == h) return true;

        destroyStaging();
        RHI::BufferDesc desc;
        desc.size = static_cast<uint64_t>(w) * h * 4 * 2;   // RGBA16_Float = 每像素 8 字节
        desc.type = RHI::BufferType::Staging;
        desc.memoryType = RHI::MemoryType::GPU_To_CPU;
        desc.allowReadback = true;
        desc.debugName = "FrameCapture_Staging";
        m_staging = m_resMgr->createBuffer(desc, "FrameCapture_Staging");
        if (!m_staging.isValid()) {
            LOG_ERROR("[FrameCapture] 创建读回 staging buffer 失败 ({}x{})", w, h);
            return false;
        }
        m_width = w;
        m_height = h;
        return true;
    }

    void FrameCapture::destroyStaging() {
        if (m_staging.isValid()) {
            m_resMgr->destroy(m_staging);
            m_staging = RHI::BufferHandle{};
        }
        m_width = m_height = 0;
    }

    bool FrameCapture::capture(RHI::RHITexture* tex) {
        if (!m_active) return false;
        if (m_cfg.frameCount > 0 && m_captured >= m_cfg.frameCount) return false;
        if (!tex || tex->getType() != RHI::TextureType::Texture2D) return false;
        if (tex->getFormat() != RHI::Format::RGBA16_Float) {
            if (!m_formatWarned) {
                LOG_WARN("[FrameCapture] 期望 RGBA16_Float 最终纹理（引擎 SceneColor 约定），当前格式不匹配，帧捕获跳过");
                m_formatWarned = true;
            }
            return false;
        }

        auto extent = tex->getExtent();
        if (!ensureStaging(extent)) return false;
        auto* staging = m_resMgr->getBuffer(m_staging);
        if (!staging) return false;

        RHI::ImageSubresourceRange range;
        range.aspectMask = RHI::ImageAspect::Color;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        // 1. ShaderReadOnly → TransferSrc。
        //    graph 每帧结束 SceneColor 停在 ShaderReadOnly；RHI 布局追踪靠本类"拷完转回"
        //    保持与实际一致（首帧追踪为 Undefined，barrier 同样合法）。
        tex->transitionLayout(
            RHI::ImageLayout::TransferSrc,
            RHI::PipelineStage::FragmentShader,
            RHI::PipelineStage::Transfer,
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderRead),
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::TransferRead),
            range);

        // 2. 纹理 → staging。copyToBuffer 内部单次命令 buffer 提交到 graphics 队列并
        //    vkQueueWaitIdle，且调用时机在本帧 submit 之后（队列序靠后）→ 同步读到本帧内容。
        RHI::BufferImageCopyRegion region;
        region.bufferOffset = 0;
        region.bufferRowLength = 0;       // 0 = 紧密打包（行宽 = width * 每像素字节）
        region.bufferImageHeight = 0;
        region.imageSubresource = range;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = extent;
        tex->copyToBuffer(staging, { region });

        // 3. TransferSrc → ShaderReadOnly：恢复布局 + RHI 追踪，保证下一帧读回 oldLayout 正确。
        tex->transitionLayout(
            RHI::ImageLayout::ShaderReadOnly,
            RHI::PipelineStage::Transfer,
            RHI::PipelineStage::FragmentShader,
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::TransferRead),
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderRead),
            range);

        // 4. 映射 + 半精度 → float → sRGB 8bit（alpha 线性不转 sRGB）
        void* mapped = staging->map();
        if (!mapped) {
            LOG_ERROR("[FrameCapture] 映射 staging buffer 失败");
            return false;
        }
        const uint16_t* half = static_cast<const uint16_t*>(mapped);
        uint32_t w = extent.width, h = extent.height;
        size_t pixelCount = static_cast<size_t>(w) * h;
        std::vector<uint8_t> rgba8(pixelCount * 4);
        for (size_t i = 0; i < pixelCount; ++i) {
            const uint16_t* p = half + i * 4;
            float a = glm::unpackHalf1x16(p[3]);
            // SceneColor 是"直通 alpha blend 到透明黑"的结果：背景区域 RGB 实际已按 alpha 预乘
            //（绿色×alpha）。PNG 查看器按直通 alpha 再乘一遍 → 双重变暗 → 软边粒子黑边。
            // 这里先除回 alpha（un-premultiply）得到直通 alpha 的亮色；alpha≈0 时保留透明黑。
            float invA = (a > 0.004f) ? (1.0f / a) : 0.0f;
            rgba8[i * 4 + 0] = linearToSrgb8(glm::unpackHalf1x16(p[0]) * invA);
            rgba8[i * 4 + 1] = linearToSrgb8(glm::unpackHalf1x16(p[1]) * invA);
            rgba8[i * 4 + 2] = linearToSrgb8(glm::unpackHalf1x16(p[2]) * invA);
            rgba8[i * 4 + 3] = linearToUint8(a);
        }
        staging->unmap();

        // 5. 写 PNG
        char filename[64];
        std::snprintf(filename, sizeof(filename), "frame_%04u.png", m_captured);
        std::string path = m_cfg.outputDir + "/" + filename;
        int ok = stbi_write_png(path.c_str(), static_cast<int>(w), static_cast<int>(h),
                                4, rgba8.data(), static_cast<int>(w) * 4);
        if (!ok) {
            LOG_ERROR("[FrameCapture] stbi_write_png 失败: {}", path);
            return false;
        }

        m_captured++;
        if (m_cfg.frameCount > 0 && m_captured >= m_cfg.frameCount) {
            LOG_INFO("[FrameCapture] 已录满 {} 帧 → {}", m_cfg.frameCount, m_cfg.outputDir);
            destroyStaging();   // 释放读回资源
        }
        return true;
    }

} // namespace StarryEngine
