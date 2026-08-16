#pragma once
#include <cmath>

namespace StarryEngine {

    class Clock {
    public:
        void advance(float deltaTime) {
            m_deltaTime = deltaTime;
            if (m_paused) return;

            m_time += deltaTime * m_speed;

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

        float getTime() const { return m_time; }

        float getDeltaTime() const { return m_paused ? 0.0f : m_deltaTime; }

        void setSpeed(float speed) { m_speed = speed; }
        float getSpeed() const { return m_speed; }

        void setLoop(bool loop) { m_loop = loop; }
        bool isLooping() const { return m_loop; }

        void setDuration(float seconds) { m_duration = seconds; }
        float getDuration() const { return m_duration; }

        void pause()   { m_paused = true; }
        void resume()  { m_paused = false; }
        bool isPaused() const { return m_paused; }

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
