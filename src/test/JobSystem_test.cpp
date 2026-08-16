#include <core/JobSystem.hpp>

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

using namespace StarryEngine;

// 每个 job 写自己的槽位（互不相交输出 = 软光追 job 纪律），waitAll 后主线程核对。
// 不需要原子/锁：帧 barrier 保证全部写完才读。
static unsigned fib(unsigned n) {
    unsigned a = 0, b = 1;
    for (unsigned i = 0; i < n; ++i) { unsigned t = a + b; a = b; b = t; }
    return a;
}

static int runBatch(JobSystem& js, const char* mode, unsigned jobCount) {
    std::vector<unsigned> slots(jobCount, 0);   // job i 独写 slots[i]（互不相交）
    for (unsigned i = 0; i < jobCount; ++i) {
        unsigned n = 18 + (i % 12);             // 负载有差异 → 制造不均衡
        js.submit([&slots, i, n]() {
            unsigned r = fib(n);
            for (unsigned rep = 0; rep < 4; ++rep) r = fib(r % 20 + 10);  // 加大计算量
            slots[i] = r;
        });
    }
    js.waitAll();
    for (unsigned i = 0; i < jobCount; ++i) {
        if (slots[i] == 0) {                    // fib(n) 恒 >0 → 0 说明 job 丢失/未完成
            std::printf("  FAIL[%s] slot %u 未被写\n", mode, i);
            return 1;
        }
    }
    return 0;
}

// 多线程并发 submit 的线程安全验证（8 线程 × 2000 job）
static int runThreadedSubmit(JobSystem& js) {
    std::atomic<uint32_t> done{0};
    constexpr unsigned kThreads = 8;
    constexpr unsigned kPer = 2000;
    std::vector<std::thread> producers;
    for (unsigned t = 0; t < kThreads; ++t) {
        producers.emplace_back([&]() {
            for (unsigned i = 0; i < kPer; ++i)
                js.submit([&done]() { done.fetch_add(1, std::memory_order_relaxed); });
        });
    }
    for (auto& t : producers) t.join();
    js.waitAll();
    if (done.load() != kThreads * kPer) {
        std::printf("  FAIL[并发 submit] done=%u expected=%u\n", done.load(), kThreads * kPer);
        return 1;
    }
    return 0;
}

int main() {
    int rc = 0;

    // 用例 1：线程模式（软光追式：一批 job + 帧 barrier）
    {
        JobSystem js(JobSystem::Config{});
        std::printf("[JobSystem_test] 线程模式：workers=%u\n", js.workerCount());
        rc |= runBatch(js, "threaded", 4096);
        rc |= runThreadedSubmit(js);
    }

    // 用例 2：同步回退（软开关 enableThreads=false，数据流与单线程一致）
    {
        JobSystem js(JobSystem::Config{ .enableThreads = false });
        std::printf("[JobSystem_test] 同步回退：workers=%u\n", js.workerCount());
        rc |= runBatch(js, "sync", 4096);
    }

    // 用例 3：worker 内 fire-and-forget 嵌套 submit（追加同批次，晚些执行）
    {
        JobSystem js(JobSystem::Config{});
        std::atomic<uint32_t> done{0};
        js.submit([&]() {
            js.submit([&done]() { done.fetch_add(1, std::memory_order_relaxed); });
            done.fetch_add(1, std::memory_order_relaxed);
        });
        js.waitAll();
        if (done.load() != 2) {
            std::printf("  FAIL[嵌套 submit] done=%u expected=2\n", done.load());
            rc = 1;
        }
    }

    // 析构验证：三例的 JobSystem 均在作用域末销毁 → waitAll + join，无挂起即通过
    if (rc == 0) std::printf("[JobSystem_test] ALL PASS\n");
    else         std::printf("[JobSystem_test] FAILURES\n");
    return rc;
}
