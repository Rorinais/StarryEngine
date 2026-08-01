#include "Animator.hpp"

namespace StarryEngine::Scene {

    void Animator::update(const Clock& clock) {
        if (!m_playing || !m_clip) return;

        // 推进时间：使用时钟的帧增量 × 速度（各对象独立进度）
        m_time += clock.getDeltaTime() * m_speed;

        // 循环边界
        float dur = (m_duration > 0.0f) ? m_duration : m_clip->duration;
        if (dur > 0.0f) {
            if (m_looping) {
                m_time = std::fmod(m_time, dur);
                if (m_time < 0.0f) m_time += dur;
            } else if (m_time >= dur) {
                m_time = dur;
            }
        }

        m_transform = m_clip->sample(m_time);
    }

} // namespace StarryEngine::Scene
