#pragma once
#include "Shape.hpp"
#include <array>
#include <memory>

namespace StarryEngine {

    class Cube : public Shape {
    public:
        // 静态创建方法
        static std::shared_ptr<Cube> create(float length = 1.0f, float width = 1.0f, float height = 1.0f) {
            return std::make_shared<Cube>(length, width, height);
        }

        Cube(float length = 1.0f, float width = 1.0f, float height = 1.0f)
            : mLength(length), mWidth(width), mHeight(height) {
        }

        Geometry::Ptr generateGeometry() const override;
        void getBoundingBox(glm::vec3& min, glm::vec3& max) const override;
        Type getType() const override { return Type::Cube; }
        float getLength() const { return mLength; }
        float getWidth() const { return mWidth; }
        float getHeight() const { return mHeight; }

        void setOrigin(glm::vec3 point) { mOrigin = point; }
        glm::vec3 getOrigin() const { return mOrigin; }

        // 添加获取顶点和索引数量的方法
        uint32_t getVertexCount() const {
            return 24; // 6个面 * 4个顶点
        }

        uint32_t getIndexCount() const {
            return 36; // 6个面 * 6个索引（2个三角形）
        }

    private:
        float mLength = 0.0f;
        float mWidth = 0.0f;
        float mHeight = 0.0f;
        glm::vec3 mOrigin{ 0.0f };
    };

} // namespace StarryEngine