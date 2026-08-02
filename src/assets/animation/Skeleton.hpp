#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace StarryEngine::Assets {

    // 骨骼关节：层级中的一个节点
    struct Bone {
        std::string name;
        glm::mat4 inverseBindMatrix = glm::mat4(1.0f);  // aiBone::mOffsetMatrix（绑定姿势逆）
        int parentIndex = -1;                            // 父骨骼索引（-1 = 根）

        // 每帧计算（动画采样后填充）
        glm::mat4 localTransform  = glm::mat4(1.0f);    // 当前帧局部变换
        glm::mat4 globalTransform = glm::mat4(1.0f);    // 层级传播后的全局变换
    };

    // 骨骼系统：层级 + 逆绑定矩阵 + 每帧最终蒙皮矩阵
    struct Skeleton {
        std::vector<Bone> bones;
        std::unordered_map<std::string, uint32_t> nameToIndex;

        uint32_t getBoneIndex(const std::string& name) const {
            auto it = nameToIndex.find(name);
            return (it != nameToIndex.end()) ? it->second : UINT32_MAX;
        }
        uint32_t getBoneCount() const { return static_cast<uint32_t>(bones.size()); }

        // 每帧采样后：从根到叶传播局部变换 → globalTransform
        void propagateTransforms();

        // 蒙皮矩阵 = globalTransform * inverseBindMatrix（上传 GPU）
        std::vector<glm::mat4> computeSkinningMatrices() const;
    };

} // namespace StarryEngine::Assets
