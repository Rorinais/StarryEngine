#pragma once
// ────────────────────────────────────────────────────────────────
// L3 应用层：运行时开关的唯一存放处。
// 原先这些散落在 STARRY_* 环境变量、DemoSwitches 常量、引擎成员三处，现已收拢到这里。
// 全局单例（runtimeSettings()）便于调试面板/热键在运行时直接修改，无需重编译。
// ────────────────────────────────────────────────────────────────
#include <cstdint>
#include <string>

namespace StarryEngine::App {

struct RuntimeSettings {
    // ── 渲染策略 ──
    bool          incrementalResize = true;   ///< 尺寸变化时只重建尺寸相关资源（false = 整图重建）
    int           resizeThrottleMs  = 600;    ///< 交换链重建节流间隔
    bool          forceRenderPath   = false;  ///< 强制旧版 render pass 路径
    bool          parallelRecord    = false;  ///< 帧内并行录制
    bool          parallelThreaded  = true;   ///< 并行时用线程池（false = inline）
    uint32_t      jobWorkers        = 0;      ///< 线程数：0 = 用 JobSystem 默认
    bool          frameInFlight     = false;  ///< 帧间解耦(FID)

    // ── 内容 / 演示 ──
    bool          useRayTracing    = false;   ///< true = 走 RayTracingDemo
    bool          offlineMode      = false;
    bool          offlineDenoise   = false;
    std::string   offlinePath;
    int           spp              = 0;       ///< 0 = 用默认(交互64/离线256)
    int           offlineFrames    = 0;
    bool          noOutline        = false;
    std::string   extraChars;
    bool          asyncBones       = false;

    // ── 诊断 ──
    bool          frameDump        = false;
    std::string   frameDumpDir;
    uint32_t      frameCount       = 1;
    int           frameIndex       = -1;      ///< <0 = 不指定
    bool          frameExit        = false;
    uint64_t      autoQuitFrames   = 0;       ///< 0 = 不自动退出
};

/// 全局运行时开关（唯一实例；测试可传入自己的实例替换默认值）
inline RuntimeSettings& runtimeSettings() {
    static RuntimeSettings s;
    return s;
}

} // namespace StarryEngine::App
