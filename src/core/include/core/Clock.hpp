#pragma once
#include <cmath>

namespace StarryEngine {

    // 全局游戏时钟 —— 所有动画/时间相关功能的唯一时间源。
    // 每帧 advance(deltaTime) 累计时间，支持播放控制（循环/变速/暂停/跳转）。
    //
    // 用法：
    //   Clock clock;
    //   ... 每帧 ...
    //   clock.advance(deltaTime);
    //   animator->update(clock.getTime());
    class Clock {
    public:
        // 每帧推进。deltaTime 为当前帧增量（秒）。
        void advance(float deltaTime) {
            m_deltaTime = deltaTime;
            if (m_paused) return;

            m_time += deltaTime * m_speed;

            // 有限时长：到达终点后循环或停在终点
            if (m_duration > 0.0f) {
                if (m_loop) {
                    m_time = std::fmod(m_time, m_duration);
                    if (m_time < 0.0f) m_time += m_duration;
                }
                else if (m_time >= m_duration) {
                    m_time = m_duration;
                }
            }
        }

        // 当前累计时间（秒）
        float getTime() const { return m_time; }

        // 当前帧增量（暂停时为 0）
        float getDeltaTime() const { return m_paused ? 0.0f : m_deltaTime; }

        // 播放速度（1.0 = 正常，2.0 = 两倍速，0.5 = 半速）
        void setSpeed(float speed) { m_speed = speed; }
        float getSpeed() const { return m_speed; }

        // 循环：true 时时间到 duration 后回到 0 重新播
        void setLoop(bool loop) { m_loop = loop; }
        bool isLooping() const { return m_loop; }

        // 时长（秒）。0 = 无限（默认）。配合 setLoop 使用。
        void setDuration(float seconds) { m_duration = seconds; }
        float getDuration() const { return m_duration; }

        void pause()   { m_paused = true; }
        void resume()  { m_paused = false; }
        bool isPaused() const { return m_paused; }

        // 跳转到指定时间
        void seek(float time) { m_time = time; }

    private:
        float m_time = 0.0f;        // 累计时间
        float m_deltaTime = 0.0f;   // 当前帧增量
        float m_speed = 1.0f;       // 播放速度
        float m_duration = 0.0f;    // 时长（0 = 无限）
        bool  m_loop = false;       // 是否循环
        bool  m_paused = false;     // 是否暂停
    };

} // namespace StarryEngine
