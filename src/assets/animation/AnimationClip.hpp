#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace StarryEngine::Assets {

    // 变换关键帧：某个时间点对象的 位置 / 旋转 / 缩放
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

    // 对象变换曲线：一串关键帧，按时间线性/球面插值
    // 属于动画资源（assets），后续可从 assimp 的 aiAnimation 加载
    struct AnimationClip {
        std::string name;
        float duration = 0.0f;      // 总时长（秒），0 = 无循环边界
        bool looping = true;        // 是否循环
        std::vector<TransformKeyframe> keyframes;

        // 采样：给定时间 → 插值出变换矩阵
        glm::mat4 sample(float time) const;
    };

} // namespace StarryEngine::Assets
