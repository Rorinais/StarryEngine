#pragma once
// ────────────────────────────────────────────────────────────────
// L3 应用层：诊断 / 运行时统计。
// 把原先写在 main() 里的"性能日志 + 帧计数 + 自动退出"收拢成一个类。
// ────────────────────────────────────────────────────────────────
#include <chrono>
#include <cstdint>
#include <functional>

namespace StarryEngine::RHI { class IRHI; }

namespace StarryEngine::App {

class Diagnostics {
public:
    /// 每帧调用：累计帧数、按 autoQuitFrames 请求退出、限频打印性能。
    void onFrame(StarryEngine::RHI::IRHI& rhi, const std::function<void()>& requestExit);

    void reset();

private:
    std::chrono::steady_clock::time_point m_perfLast;
    uint64_t m_perfLastFrames = 0;
    uint64_t m_frames = 0;
};

} // namespace StarryEngine::App
