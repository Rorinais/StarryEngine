#include "AnimationClip.hpp"
#include <cmath>
#include <algorithm>

namespace StarryEngine::Assets {

    glm::mat4 AnimationClip::sample(float time) const {
        if (keyframes.empty()) return glm::mat4(1.0f);
        if (keyframes.size() == 1) return keyframes[0].toMatrix();

        // 时间归一化：循环取模 / 夹到末尾
        if (looping && duration > 0.0f) {
            time = std::fmod(time, duration);
            if (time < 0.0f) time += duration;
        } else {
            time = std::clamp(time, keyframes.front().time, keyframes.back().time);
        }

        // 找到包围 time 的两个相邻关键帧
        size_t i = 0;
        while (i + 1 < keyframes.size() && keyframes[i + 1].time <= time) ++i;
        size_t j = std::min(i + 1, keyframes.size() - 1);

        const auto& a = keyframes[i];
        const auto& b = keyframes[j];
        float span = b.time - a.time;
        float f = (span > 0.0f) ? (time - a.time) / span : 0.0f;
        f = std::clamp(f, 0.0f, 1.0f);

        // 位置/缩放线性插值，旋转球面插值（保证平滑）
        glm::vec3 pos = glm::mix(a.position, b.position, f);
        glm::quat rot = glm::slerp(a.rotation, b.rotation, f);
        glm::vec3 scl = glm::mix(a.scale, b.scale, f);

        glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
        m *= glm::mat4_cast(rot);
        return glm::scale(m, scl);
    }

} // namespace StarryEngine::Assets
