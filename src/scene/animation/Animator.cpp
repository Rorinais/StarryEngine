#include "Animator.hpp"
#include <cmath>

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

    void Animator::updateSkeleton(Assets::Skeleton& skeleton, const Assets::AnimationClip& clip, float time) {
        if (skeleton.bones.empty()) return;

        // 时间循环取模（ticks）
        if (clip.duration > 0.0f) {
            time = std::fmod(time, clip.duration);
            if (time < 0.0f) time += clip.duration;
        }

        // 重置为绑定姿势（没有动画轨道的骨骼保持绑定姿势）
        for (auto& bone : skeleton.bones)
            bone.localTransform = bone.bindLocalTransform;

        // 采样每个轨道 → 覆盖对应骨骼的局部变换
        for (const auto& track : clip.tracks) {
            if (track.boneIndex < 0 || track.boneIndex >= static_cast<int>(skeleton.bones.size()))
                continue;
            skeleton.bones[track.boneIndex].localTransform = clip.sampleBoneTrack(track, time);
        }

        // 层级传播 → 全局变换 → 蒙皮矩阵
        skeleton.propagateTransforms();
        m_boneMatrices = skeleton.computeSkinningMatrices();
    }

} // namespace StarryEngine::Scene
