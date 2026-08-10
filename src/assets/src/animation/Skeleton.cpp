#include <assets/animation/Skeleton.hpp>

namespace StarryEngine::Assets {

    void Skeleton::propagateTransforms() {
        // 骨骼已按层级顺序存储（父在子前），逐个传播
        for (auto& bone : bones) {
            if (bone.parentIndex >= 0 && bone.parentIndex < static_cast<int>(bones.size()))
                bone.globalTransform = bones[bone.parentIndex].globalTransform * bone.localTransform;
            else
                bone.globalTransform = bone.localTransform;
        }
    }

    std::vector<glm::mat4> Skeleton::computeSkinningMatrices() const {
        std::vector<glm::mat4> result;
        result.reserve(bones.size());
        for (const auto& bone : bones)
            result.push_back(bone.globalTransform * bone.inverseBindMatrix);
        return result;
    }

} // namespace StarryEngine::Assets
