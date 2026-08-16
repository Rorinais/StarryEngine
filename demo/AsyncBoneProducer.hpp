#pragma once
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <glm/glm.hpp>
#include <assets/animation/AnimationClip.hpp>
#include <assets/animation/Skeleton.hpp>
#include <scene/animation/Animator.hpp>

namespace StarryEngine {

// ═══════════════════════════════════════════════════════════════════════
// AsyncBoneProducer —— 异步数据线程的参考实现（producer/consumer 双缓冲）
//
// 模式：
//   数据线程(producer)：推进动画时间 → updateSkeleton 算骨骼矩阵 → 写 CPU 槽位 → 标记就绪
//   渲染线程(consumer)：step(delta) 消费就绪槽位 → 由调用方 memcpy 上传到 GPU SSBO
//
// 三条纪律（这就是"异步多线程渲染"骨架的核心，重写时照着走）：
//   1. 数据线程只做纯 CPU 数学，绝不碰 RHI/Vulkan。
//      Vulkan 描述符/缓冲写入有外同步要求，且每帧上传只有一个正确位置（渲染线程）；
//      producer 摘出去的只有"算出数据"，上传路径原样不动。
//   2. 双缓冲在 CPU 侧：producer 写槽 A 时 consumer 读槽 B，互不踩踏；
//      ready 标志在 m_mutex 下做 happens-before（producer 的写 → ready=true → consumer 读到）。
//   3. 软开关：enableThread=false 时 step() 同步内联执行，数据流与旧实现完全一致 ——
//      先关着验证渲染逻辑，再开线程验证线程本身（两变量解耦）。
//
// 槽位乒乓：producer 每轮写 m_produceIdx、consumer 读 m_consumeIdx，两者恒互补（0↔1），
// 同一个槽位永远不会被并发读写。producer 计算阶段在锁外跑（只写自己独占的状态），
// 关键区极小（仅 kick 入队 / ready 翻转）。
// ═══════════════════════════════════════════════════════════════════════
class AsyncBoneProducer {
public:
    struct Config {
        bool enableThread = false;   // ★ 软开关：默认关（同步内联）
    };

    // skeleton/clip 按值拷入（独立副本）：updateSkeleton 会原地改骨架，不能共享调用方数据。
    // 线程自给自足，只访问 producer 自有成员；即使 teardown 时线程仍在跑也绝不碰 demo 数据。
    AsyncBoneProducer(const Assets::Skeleton& skeleton, const Assets::AnimationClip& clip, const Config& cfg)
        : m_skeleton(skeleton)
        , m_clip(clip)
        , m_cfg(cfg)
        , m_time(0.0f) {
        // 引导：构造时就地算好第 0 帧（与旧 addGriseoModel 首次 updateSkeleton(0) 一致）
        m_slots[0].matrices = computeMatrices(0.0f);
        m_slots[0].ready = true;
        if (m_cfg.enableThread) {
            m_thread = std::thread(&AsyncBoneProducer::producerLoop, this);
        }
    }

    ~AsyncBoneProducer() {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_stop = true;
        }
        m_cvRequest.notify_all();
        if (m_thread.joinable()) m_thread.join();
    }

    AsyncBoneProducer(const AsyncBoneProducer&) = delete;
    AsyncBoneProducer& operator=(const AsyncBoneProducer&) = delete;

    // 第 0 帧矩阵（构造时已算好）：供主线程预热骨骼 SSBO 用（建 buffer + 写描述符必须主线程）。
    const std::vector<glm::mat4>& getFrame0Matrices() const { return m_slots[0].matrices; }

    // ── 渲染线程每帧调用 ──
    // 1) kick：请求 producer 推进一帧（写另一个槽位，与本次渲染并行）
    // 2) 等本帧槽位就绪（producer 上一轮 kick 已算好）
    // 返回：就绪槽位的骨骼矩阵（只读，有效期到下次 step()；调用方须立即 memcpy）
    const std::vector<glm::mat4>& step(float deltaTime) {
        if (!m_cfg.enableThread) {
            // 同步模式：就地计算，数据流与旧实现完全一致
            m_slots[0].matrices = computeMatrices(deltaTime);
            return m_slots[0].matrices;
        }

        // 异步模式：kick
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_pendingDelta = deltaTime;
            m_hasRequest = true;
        }
        m_cvRequest.notify_one();

        const int idx = m_consumeIdx;
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cvReady.wait(lk, [&] { return m_stop || m_slots[idx].ready; });
            m_slots[idx].ready = false;
        }
        m_consumeIdx = 1 - m_consumeIdx;
        return m_slots[idx].matrices;
    }

private:
    struct Slot {
        std::vector<glm::mat4> matrices;
        bool ready = false;      // producer 写完置 true；consumer 读后清 false（均锁保护）
    };

    std::vector<glm::mat4> computeMatrices(float delta) {
        // 与旧 onUpdate 相同的 ticks 换算 / looping / duration 语义
        float tps = (m_clip.ticksPerSecond > 0.0f) ? m_clip.ticksPerSecond : 25.0f;
        // 非循环动画播到 duration 就停住（保持最后一帧，不再推进/绕回）
        if (m_clip.looping || m_time < m_clip.duration) m_time += delta * tps;
        m_animator.updateSkeleton(m_skeleton, m_clip, m_time);
        return m_animator.getBoneMatrices();   // 拷贝进槽位（getBoneMatrices 返回内部引用）
    }

    void producerLoop() {
        for (;;) {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cvRequest.wait(lk, [&] { return m_hasRequest || m_stop; });
            if (m_stop) return;
            m_hasRequest = false;
            const float delta = m_pendingDelta;
            const int writeIdx = m_produceIdx;       // 乒乓：与消费槽位互补
            m_produceIdx = 1 - m_produceIdx;
            lk.unlock();                             // 计算放锁外：只此线程访问 m_skeleton/m_animator

            m_slots[writeIdx].matrices = computeMatrices(delta);

            {
                std::lock_guard<std::mutex> g(m_mutex);
                m_slots[writeIdx].ready = true;
            }
            m_cvReady.notify_one();
        }
    }

    Assets::Skeleton m_skeleton;      // 独立副本（updateSkeleton 会改骨架，不能共享）
    Assets::AnimationClip m_clip;     // 独立副本
    Scene::Animator m_animator;       // producer 独占
    Config m_cfg;
    float m_time = 0.0f;

    Slot m_slots[2];                  // CPU 双缓冲槽位
    int m_consumeIdx = 0;             // consumer 本轮读
    int m_produceIdx = 1;             // producer 本轮写（构造后与消费互补）

    std::mutex m_mutex;
    std::condition_variable m_cvRequest;   // 有 kick 请求
    std::condition_variable m_cvReady;     // 槽位就绪
    bool m_hasRequest = false;
    float m_pendingDelta = 0.0f;
    bool m_stop = false;
    std::thread m_thread;
};

} // namespace StarryEngine
