#pragma once
#include <cstdint>
#include <functional>
#include <memory>

namespace StarryEngine {
    class JobSystem;

    namespace RHI {
        class RHICommandEncoder;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 并行命令录制上下文（ADR-6 第 2 步：每 pass → 独立 secondary CB job）
    //
    // jobs 非空 → RenderGraph::execute 走并行路径：每个启用的 (pass×subpass) 录进
    // 一条独立 secondary command buffer（一个 job，天然互不相交），帧 barrier 后
    // 主线程发布局转换 barrier + beginRenderPass(Secondary)+executeCommands+end。
    // jobs==nullptr → 原串行路径，行为与单线程完全一致（软开关第 0 级）。
    //
    // workerCount=0 或 JobSystem::enableThreads=false 时 job 在主线程同步内联执行
    // （软开关第 1 级：验 secondary CB/继承/executeCommands 单线程），数据流等价。
    // ═══════════════════════════════════════════════════════════════════════
    struct ParallelRecordingContext {
        JobSystem* jobs = nullptr;                                  // nullptr → 串行
        uint32_t workerCount = 1;                                   // 0 = 主线程内联
        // 从 per-worker 命令池分配并包好一条 secondary 编码器（尚未 beginSecondary）。
        // workerIndex ∈ [0, workerCount) 轮转；调用方接着 beginSecondary + 录制。
        std::function<std::unique_ptr<RHI::RHICommandEncoder>(uint32_t workerIndex)> allocateSecondary;
    };
} // namespace StarryEngine
