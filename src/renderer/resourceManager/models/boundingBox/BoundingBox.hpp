#pragma once
#include <glm/glm.hpp>
#include <vector>
namespace StarryEngine {
    class BoundingBox {
    public:
        virtual ~BoundingBox() = default;

        // 扩展包围盒以包含点
        virtual void expand(const glm::vec3& point) = 0;

        // 扩展包围盒以包含另一个包围盒
        virtual void expand(const BoundingBox& other) = 0;

        // 应用变换矩阵
        virtual void transform(const glm::mat4& matrix) = 0;

        // 判断点是否在包围盒内
        virtual bool contains(const glm::vec3& point) const = 0;

        // 判断是否有效（已初始化）
        virtual bool isValid() const = 0;

        // 重置包围盒
        virtual void reset() = 0;

        // 获取包围盒中心
        virtual glm::vec3 getCenter() const = 0;

        // 克隆方法（用于多态拷贝）
        virtual BoundingBox* clone() const = 0;
    };
}