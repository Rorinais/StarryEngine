#include <assets/animation/AnimationClip.hpp>
#include <cmath>
#include <algorithm>

namespace StarryEngine::Assets {

    // 采样关键帧数组（线性插值）。keys 空 → 返回默认值
    static glm::vec3 sampleKeys(const std::vector<VectorKey>& keys, float time, const glm::vec3& def) {
        if (keys.empty()) return def;
        if (keys.size() == 1) return keys[0].value;
        size_t i = 0;
        while (i + 1 < keys.size() && keys[i + 1].time <= time) ++i;
        size_t j = std::min(i + 1, keys.size() - 1);
        float span = keys[j].time - keys[i].time;
        float f = (span > 0.0f) ? (time - keys[i].time) / span : 0.0f;
        return glm::mix(keys[i].value, keys[j].value, std::clamp(f, 0.0f, 1.0f));
    }

    static glm::quat sampleKeys(const std::vector<QuatKey>& keys, float time, const glm::quat& def) {
        if (keys.empty()) return def;
        if (keys.size() == 1) return keys[0].value;
        size_t i = 0;
        while (i + 1 < keys.size() && keys[i + 1].time <= time) ++i;
        size_t j = std::min(i + 1, keys.size() - 1);
        float span = keys[j].time - keys[i].time;
        float f = (span > 0.0f) ? (time - keys[i].time) / span : 0.0f;
        return glm::slerp(keys[i].value, keys[j].value, std::clamp(f, 0.0f, 1.0f));
    }

    glm::mat4 AnimationClip::sampleBoneTrack(const BoneTrack& track, float time) const {
        // 无关键帧 → 恒等（保持绑定姿势）
        if (track.positions.empty() && track.rotations.empty() && track.scales.empty())
            return glm::mat4(1.0f);

        glm::vec3 pos = sampleKeys(track.positions, time, glm::vec3(0.0f));
        glm::quat rot = sampleKeys(track.rotations, time, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        glm::vec3 scl = sampleKeys(track.scales, time, glm::vec3(1.0f));

        glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
        m *= glm::mat4_cast(rot);
        return glm::scale(m, scl);
    }

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
