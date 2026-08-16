#pragma once
#include <assets/animation/AnimationClip.hpp>
#include <assets/animation/Skeleton.hpp>
#include <core/Clock.hpp>
#include <memory>

namespace StarryEngine::Scene {

    class Animator {
    public:
        void setClip(std::shared_ptr<Assets::AnimationClip> clip) { m_clip = clip; }
        std::shared_ptr<Assets::AnimationClip> getClip() const { return m_clip; }

        void setSpeed(float speed) { m_speed = speed; }
        float getSpeed() const { return m_speed; }

        void setLooping(bool loop) { m_looping = loop; }
        void setDuration(float seconds) { m_duration = seconds; }

        void seek(float time) { m_time = time; }
        float getTime() const { return m_time; }

        void play()  { m_playing = true; }
        void pause() { m_playing = false; }
        bool isPlaying() const { return m_playing; }

        void update(const Clock& clock);

        const glm::mat4& getTransform() const { return m_transform; }

        void updateSkeleton(Assets::Skeleton& skeleton, const Assets::AnimationClip& clip, float time);
        const std::vector<glm::mat4>& getBoneMatrices() const { return m_boneMatrices; }

    private:
        std::shared_ptr<Assets::AnimationClip> m_clip;
        float m_time = 0.0f;
        float m_speed = 1.0f;
        float m_duration = 0.0f;
        bool m_looping = true;
        bool m_playing = true;
        glm::mat4 m_transform = glm::mat4(1.0f);
        std::vector<glm::mat4> m_boneMatrices;
    };

} // namespace StarryEngine::Scene
