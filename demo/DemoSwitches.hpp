#pragma once
// ────────────────────────────────────────────────────────────────
// demo 的调试/测试开关集中处。
// 原来这些是 STARRY_* 环境变量，现已全部改为常量（环境变量机制移除）。
// 需要启用某个模式时，直接修改这里的值并重新编译。
// ────────────────────────────────────────────────────────────────
#include <cstdint>

namespace DemoSwitches {
    // ── 射线追踪 demo（原 STARRY_RT / STARRY_OFFLINE）──
    constexpr bool        kUseRayTracing = false;   // true = 走 RayTracingDemo
    constexpr bool        kOfflineMode   = false;   // 离线渲染模式
    constexpr bool        kOfflineDenoise = false;  // 离线降噪
    constexpr const char* kOfflinePath   = "";      // 离线输出路径（空 = 不输出）
    constexpr int         kSpp           = 0;       // 0 = 用默认(交互64/离线256)
    constexpr int         kOfflineFrames = 0;       // 0 = 用默认

    // ── 帧捕获（原 STARRY_FRAME_DUMP / COUNT / INDEX / EXIT）──
    constexpr bool        kFrameDump    = false;
    constexpr const char* kFrameDumpDir = "";
    constexpr uint32_t    kFrameCount   = 1;
    constexpr int         kFrameIndex   = -1;       // <0 = 不指定
    constexpr bool        kFrameExit    = false;

    // ── 其他（原 STARRY_NO_OUTLINE / EXTRA_CHARS / ASYNC_BONES / WIN_W,H / AUTO_QUIT）──
    constexpr bool        kNoOutline    = false;
    constexpr const char* kExtraChars   = "";
    constexpr bool        kAsyncBones   = false;
    constexpr uint32_t    kWinW         = 800;
    constexpr uint32_t    kWinH         = 600;
    constexpr uint64_t    kAutoQuitFrames = 0;      // 0 = 不自动退出
}
