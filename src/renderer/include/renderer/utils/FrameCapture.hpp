#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <renderer/interface/RHIHandles.hpp>
#include <renderer/interface/IRHIResources.hpp>

namespace StarryEngine::RHI {
    class IRHI;
    class ResourceManager;
}

namespace StarryEngine {

    // 帧序列捕获：把离屏最终颜色纹理（RGBA16_Float 线性 HDR）异步读回 CPU，
    // 软件转 sRGB 8bit 后写 PNG（frame_%04d.png）。
    //
    // 全异步 + 缓存 + 不丢帧：
    //   · 读回 = RHI 异步读回（transition+copy+transition 一次命令缓冲 + fence，不阻塞），
    //     kSlots 个 staging 槽位作为环形缓存，主线程每帧只付一次提交。
    //   · 编码 = worker 池：wait 对应槽位 fence → map → 软件 sRGB 转换 → PNG 压缩 → 写盘，
    //     完全在后台线程，主线程渲染循环不受导出影响。
    //   · 无空闲槽位时主线程背压等待（绝不丢帧，内存有界 = kSlots 帧）。
    //
    // 布局约定：调用前纹理需处于 ShaderReadOnly（graph 每帧结束的最终布局）；
    // 异步读回内完成 TransferSrc 往返并把布局追踪转回 ShaderReadOnly。
    class FrameCapture {
    public:
        struct Config {
            std::string outputDir = "frames";
            uint32_t frameCount = 1;              // 0 = 持续录制（直到进程退出）
            uint32_t frameIndex = UINT32_MAX;     // 指定导出第 N 帧（渲染帧号，capture 调用计数），
                                                  // 设置时只导这一帧，frameCount 被忽略
        };

        explicit FrameCapture(std::shared_ptr<RHI::IRHI> rhi);
        ~FrameCapture();

        FrameCapture(const FrameCapture&) = delete;
        FrameCapture& operator=(const FrameCapture&) = delete;

        // 初始化输出目录 + worker 池；失败返回 false（保持未激活）
        bool initialize(const Config& cfg);

        // 捕获当前帧。录满/未激活/纹理不匹配时返回 false。
        // finalTexture 期望为 RGBA16_Float 的 Texture2D（引擎 SceneColor 约定）。
        bool capture(RHI::RHITexture* finalTexture);

        uint32_t capturedCount() const { return m_captured; }
        bool isActive() const { return m_active; }

    private:
        // 并发编码数 = min(kSlots, kWorkers)：worker 拿了 job 就占住槽位直到编码完，
        // 槽位少会让多余 worker 闲置（4槽+8worker 实测只有 13 FPS 并发）。
        static constexpr uint32_t kSlots = 8;       // 读回槽位（环形缓存，内存上界 = kSlots 帧）
        static constexpr uint32_t kWorkers = 8;     // 编码 worker 池大小（PNG 编码是吞吐瓶颈，按核数放宽）

        struct PendingJob {
            uint32_t slot = 0;
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t frameIndex = 0;                // 渲染帧号（PNG 文件名 + 日志）
        };

        bool ensureStaging(const RHI::Extent3D& extent);
        void destroyStaging();
        void startWorker();
        void workerMain();
        void process(const PendingJob& job);
        static void encodeAndWrite(const PendingJob& job, const uint16_t* half, const std::string& outputDir);

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;

        Config m_cfg;
        std::vector<RHI::BufferHandle> m_staging;   // kSlots 个 staging
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        uint64_t m_frameCounter = 0;                // capture() 调用计数 = 渲染帧号
        uint32_t m_captured = 0;                    // 已提交捕获帧数
        bool m_active = false;
        bool m_formatWarned = false;

        // 空闲槽位 + 背压（主线程无空闲槽时阻塞等待，绝不丢帧）
        std::mutex m_freeMutex;
        std::condition_variable m_freeCv;
        std::vector<uint32_t> m_freeSlots;

        // worker 池 + 待编码队列
        std::mutex m_workMutex;
        std::condition_variable m_workCv;
        std::deque<PendingJob> m_workQueue;
        std::vector<std::thread> m_workers;
        bool m_stop = false;
    };

} // namespace StarryEngine
