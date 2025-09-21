#pragma once
#include "BoundingBox.hpp"
#include <limits>
namespace StarryEngine {
    class AxisAlignedBoundingBox : public BoundingBox {
    public:
        AxisAlignedBoundingBox();

        void expand(const glm::vec3& point) override;

        void expand(const BoundingBox& other) override;

        void transform(const glm::mat4& matrix) override;

        bool contains(const glm::vec3& point) const override;

        bool isValid() const override;

        void reset() override;

        AxisAlignedBoundingBox* clone() const override { return new AxisAlignedBoundingBox(*this); }
        glm::vec3 getCenter() const override { return (min + max) * 0.5f; }
        glm::vec3 getMin() const { return min; }
        glm::vec3 getMax() const { return max; }

    private:
        glm::vec3 min;
        glm::vec3 max;
    };
}
