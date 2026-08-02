#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace StarryEngine::Assets {

    // ── 变换动画（对象级，球体等）──
    struct TransformKeyframe {
        float time = 0.0f;
        glm::vec3 position = glm::vec3(0.0f);
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // 单位四元数
        glm::vec3 scale = glm::vec3(1.0f);

        glm::mat4 toMatrix() const {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
            m *= glm::mat4_cast(rotation);
            return glm::scale(m, scale);
        }
    };

    // ── 骨骼动画轨道 ──
    struct VectorKey { float time = 0.0f; glm::vec3 value = glm::vec3(0.0f); };
    struct QuatKey   { float time = 0.0f; glm::quat value = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); };

    // 单个骨骼的动画曲线（位置/旋转/缩放分离，对应 assimp aiNodeAnim）
    struct BoneTrack {
        int boneIndex = -1;                      // Skeleton 中的骨骼索引
        std::vector<VectorKey> positions;
        std::vector<QuatKey> rotations;
        std::vector<VectorKey> scales;
    };

    // AnimationClip：变换动画（keyframes）或骨骼动画（tracks）
    struct AnimationClip {
        std::string name;
        float duration = 0.0f;                   // 时长（ticks）
        float ticksPerSecond = 25.0f;            // ticks → 秒换算
        bool looping = true;

        // 变换动画（对象级，球体等）
        std::vector<TransformKeyframe> keyframes;
        glm::mat4 sample(float time) const;      // 变换动画采样

        // 骨骼动画（角色）
        std::vector<BoneTrack> tracks;
        glm::mat4 sampleBoneTrack(const BoneTrack& track, float time) const;  // 骨骼轨道采样

        bool isSkeletal() const { return !tracks.empty(); }
        float durationSeconds() const {
            float tps = (ticksPerSecond > 0.0f) ? ticksPerSecond : 25.0f;
            return duration / tps;
        }
    };

} // namespace StarryEngine::Assets
