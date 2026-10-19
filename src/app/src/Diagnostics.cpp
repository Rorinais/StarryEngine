#include <app/Diagnostics.hpp>
#include <app/RuntimeSettings.hpp>
#include <renderer/interface/IRHI.hpp>
#include <renderer/interface/vulkan/VulkanFrameContext.hpp>
#include <logging/Logger.hpp>

namespace StarryEngine::App {

void Diagnostics::reset() {
    m_perfLast = std::chrono::steady_clock::now();
    m_perfLastFrames = 0;
    m_frames = 0;
}

void Diagnostics::onFrame(StarryEngine::RHI::IRHI& rhi, const std::function<void()>& requestExit) {
    ++m_frames;

    // 自动退出（原 STARRY_AUTO_QUIT，现为 RuntimeSettings::autoQuitFrames）
    const uint64_t autoQuit = runtimeSettings().autoQuitFrames;
    if (autoQuit > 0 && m_frames >= autoQuit) {
        if (requestExit) requestExit();
        return;
    }

    // 性能日志（每 5 秒一行）
    const auto now = std::chrono::steady_clock::now();
    const double wall = std::chrono::duration<double>(now - m_perfLast).count();
    if (wall < 5.0) return;
    m_perfLast = now;

    auto fc = rhi.getFrameContext();
    if (!fc) return;
    const auto& s = fc->getStatistics();
    const uint64_t frames = s.totalFrames - m_perfLastFrames;
    m_perfLastFrames = s.totalFrames;
    LOG_INFO("[perf] 窗内帧={} 实际FPS={:.1f} | 平均帧={:.2f}ms 最大帧={:.2f}ms | CPU均={:.2f}ms GPU均={:.2f}ms",
        frames, (wall > 0.0) ? static_cast<double>(frames) / wall : 0.0,
        s.averageFrameTime, s.maxFrameTime, s.averageCPUTime, s.averageGPUTime);
}

} // namespace StarryEngine::App
