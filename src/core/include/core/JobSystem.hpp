#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace StarryEngine {

// ═══════════════════════════════════════════════════════════════════════
// JobSystem —— 软光线追踪式多线程的核心地基（线程池 + 帧 barrier）
//
// 这是重写 docs/rewrite-design.md ADR-6 第 2 步（job/tile 并行）的地基工具：
// 引擎此前没有 job system，一切并行工作（数据 job / 渲染收尾 job / 并行录制）
// 都建在它上面。
//
// 模型（软光追式：帧内一批 job + 帧 barrier）：
//   主线程每帧 submit 一批**互不相交**的 job（对照软 RT 切 tile：骨骼×网格、
//   材质×实例、实例缓冲×对象、录制×pass）
//   → worker 线程抢着执行
//   → waitAll()（帧 barrier）等全部完成 → 主线程继续（submit 后主线程仍可干别的）
//
// 三条纪律：
//   1. job 必须互不相交（只写自己的输出、只读共享常量）；帧 barrier 只保证
//      "全部完成"，不做细粒度同步（需要的依赖请拆成两批 + 中间 barrier）。
//   2. 任意线程可 submit；但 **waitAll 只能在 worker 之外调用**——worker 内
//      等自身所属的批次会死锁（pending 永不归零）。job 内可以再 submit
//      （追加到同批次，晚些执行），不能 waitAll。
//   3. 软开关：enableThreads=false 时 submit 同步内联、waitAll 空转——数据流
//      与单线程完全一致。先关着验证逻辑，再开线程验证线程本身（两变量解耦）。
//
// 线程安全：submit/waitAll 可在任意线程并发调用；析构先 waitAll 再停 worker，
// 保证不留任何执行中的 job。
// ═══════════════════════════════════════════════════════════════════════
class JobSystem {
public:
    using Job = std::function<void()>;

    struct Config {
        uint32_t workerCount = 0;    // 0 = 硬件并发 - 1（留 1 条给主线程）
        bool enableThreads = true;   // ★ 软开关：false = submit 同步内联
    };

    JobSystem();                     // 默认配置：线程开，worker = 硬件并发 - 1
    explicit JobSystem(const Config& cfg);
    ~JobSystem();                    // 等全部 job 完成 → 停 worker → join

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    // 提交一个 job。enableThreads=false 时立即同步执行（软开关回退）。
    void submit(Job job);

    // 帧 barrier：等此前 submit 的全部 job 完成。任意非-worker 线程可调用。
    void waitAll();

    // 当前执行线程的 worker 索引：worker 线程 = 0..workerCount-1，主线程/其他 = kNonWorkerIndex。
    // 并行命令录制靠它取"本执行线程"的 per-worker 命令池（每条线程只碰自己的池）。
    static uint32_t currentWorkerIndex() noexcept { return tls_workerIndex; }

    static constexpr uint32_t kNonWorkerIndex = static_cast<uint32_t>(-1);   // 非 worker 线程哨兵

    uint32_t workerCount() const noexcept { return static_cast<uint32_t>(m_workers.size()); }
    bool threadingEnabled() const noexcept { return m_enableThreads; }

private:
    void workerLoop(uint32_t workerIndex);

    std::vector<std::thread> m_workers;
    std::deque<Job> m_queue;         // 待执行队列
    std::mutex m_mutex;
    std::condition_variable m_cvWork;   // 队列非空 / 停止
    std::condition_variable m_cvIdle;   // m_pending == 0（帧 barrier）
    uint32_t m_pending = 0;          // 已提交未完成数（队列中 + 执行中）
    bool m_enableThreads = true;
    bool m_stop = false;

    static thread_local uint32_t tls_workerIndex;   // 默认 kNonWorkerIndex（主线程）
};

} // namespace StarryEngine
