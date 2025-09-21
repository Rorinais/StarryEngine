#include"AxisAlignedBoundingBox.hpp"
namespace StarryEngine {
    AxisAlignedBoundingBox::AxisAlignedBoundingBox() : min(glm::vec3(std::numeric_limits<float>::max())),
        max(glm::vec3(std::numeric_limits<float>::lowest())) {
    }

    void AxisAlignedBoundingBox::expand(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    void AxisAlignedBoundingBox::expand(const BoundingBox& other) {
        if (const AxisAlignedBoundingBox* aabb = dynamic_cast<const AxisAlignedBoundingBox*>(&other)) {
            expand(aabb->min);
            expand(aabb->max);
        }
    }

    void AxisAlignedBoundingBox::transform(const glm::mat4& matrix) {
        std::vector<glm::vec3> corners = {
            glm::vec3(min.x, min.y, min.z),
            glm::vec3(min.x, min.y, max.z),
            glm::vec3(min.x, max.y, min.z),
            glm::vec3(min.x, max.y, max.z),
            glm::vec3(max.x, min.y, min.z),
            glm::vec3(max.x, min.y, max.z),
            glm::vec3(max.x, max.y, min.z),
            glm::vec3(max.x, max.y, max.z)
        };

        reset();
        for (auto& corner : corners) {
            glm::vec4 transformed = matrix * glm::vec4(corner, 1.0f);
            expand(glm::vec3(transformed));
        }
    }

    bool AxisAlignedBoundingBox::contains(const glm::vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
            point.y >= min.y && point.y <= max.y &&
            point.z >= min.z && point.z <= max.z;
    }

    bool AxisAlignedBoundingBox::isValid() const {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    void AxisAlignedBoundingBox::reset() {
        min = glm::vec3(std::numeric_limits<float>::max());
        max = glm::vec3(std::numeric_limits<float>::lowest());
    }
}