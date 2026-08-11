#pragma once

#include <memory>
#include <string>

#include <renderer/interface/RHI_HANDLES_SYSTEM.hpp>
#include <renderer/interface/RHI_STRUCTS_RESOURCE.hpp>

namespace StarryEngine {

    namespace RHI { class ResourceManager; }

    // 帧序列捕获：把离屏最终颜色纹理（RGBA16_Float 线性 HDR）读回 CPU，
    // 软件转 sRGB 8bit 后写 PNG（frame_%04d.png）。
    //
    // 依赖 RHI_VK_Texture::copyToBuffer 内部同步（submit + vkQueueWaitIdle），
    // 在 Application 的 post-render 回调里调用，即读到本帧刚渲染完的内容
    // （读回命令与本帧渲染同队列、顺序靠后，天然等待）。
    //
    // 布局约定：调用前纹理需处于 ShaderReadOnly（graph 每帧结束的最终布局）；
    // 本类读回后会把布局/追踪转回 ShaderReadOnly，保证下一帧读回不因追踪错位报错。
    class FrameCapture {
    public:
        struct Config {
            std::string outputDir = "frames";
            uint32_t frameCount = 1;   // 0 = 持续录制（直到进程退出）
        };

        explicit FrameCapture(std::shared_ptr<RHI::ResourceManager> resMgr);
        ~FrameCapture();

        FrameCapture(const FrameCapture&) = delete;
        FrameCapture& operator=(const FrameCapture&) = delete;

        // 初始化输出目录；失败返回 false（保持未激活）
        bool initialize(const Config& cfg);

        // 捕获当前帧内容。录满/未激活/纹理不匹配时返回 false。
        // finalTexture 期望为 RGBA16_Float 的 Texture2D（引擎 SceneColor 约定）。
        bool capture(RHI::RHITexture* finalTexture);

        uint32_t capturedCount() const { return m_captured; }
        bool isActive() const { return m_active; }

    private:
        bool ensureStaging(const RHI::Extent3D& extent);
        void destroyStaging();

        std::shared_ptr<RHI::ResourceManager> m_resMgr;

        Config m_cfg;
        RHI::BufferHandle m_staging;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        uint32_t m_captured = 0;
        bool m_active = false;
        bool m_formatWarned = false;
    };

} // namespace StarryEngine
