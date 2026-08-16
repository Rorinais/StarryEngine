#include <core/JobSystem.hpp>

namespace StarryEngine {

thread_local uint32_t JobSystem::tls_workerIndex = JobSystem::kNonWorkerIndex;

JobSystem::JobSystem()
    : JobSystem(Config{}) {
}

JobSystem::JobSystem(const Config& cfg)
    : m_enableThreads(cfg.enableThreads) {
    if (!m_enableThreads) return;

    uint32_t n = cfg.workerCount;
    if (n == 0) {
        uint32_t hw = std::thread::hardware_concurrency();
        n = (hw > 1) ? (hw - 1) : 1;   // 留 1 条给主线程
    }

    m_workers.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        m_workers.emplace_back([this, i] { workerLoop(i); });   // 携带 worker 索引 → TLS
    }
}

JobSystem::~JobSystem() {
    waitAll();                          // 不遗留执行中的 job
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_stop = true;
    }
    m_cvWork.notify_all();              // 唤醒所有 worker 退出
    for (auto& t : m_workers) {
        if (t.joinable()) t.join();
    }
}

void JobSystem::submit(Job job) {
    if (!m_enableThreads) {             // 软开关回退：同步内联，数据流与单线程一致
        job();
        return;
    }
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_queue.push_back(std::move(job));
        ++m_pending;
    }
    m_cvWork.notify_one();
}

void JobSystem::waitAll() {
    if (!m_enableThreads) return;       // 同步模式下 job 已内联完成
    std::unique_lock<std::mutex> lk(m_mutex);
    m_cvIdle.wait(lk, [&] { return m_pending == 0; });
}

void JobSystem::workerLoop(uint32_t workerIndex) {
    tls_workerIndex = workerIndex;      // 整条线程生命周期内稳定：并行录制取 per-worker 池
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            // 谓词含队列非空：即使 notify 丢失（worker 都在忙），回到这里也能立即拿到活
            m_cvWork.wait(lk, [&] { return m_stop || !m_queue.empty(); });
            if (m_stop && m_queue.empty()) return;
            job = std::move(m_queue.front());
            m_queue.pop_front();
        }
        job();                          // 锁外执行：job 内可再 submit
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            --m_pending;
            if (m_pending == 0) m_cvIdle.notify_all();
        }
    }
}

} // namespace StarryEngine
