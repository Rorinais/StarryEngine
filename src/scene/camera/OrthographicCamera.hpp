#pragma once
#include "ICamera.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace StarryEngine::Scene {
    class OrthographicCamera : public ICamera {
    public:
        OrthographicCamera() = default;

        // 设置正交投影参数（左、右、下、上、近、远）
        void setOrthographic(float left, float right, float bottom, float top, float near_, float far_);

        // 设置视角中心（类似 lookAt，但正交相机通常由位置和方向决定）
        void lookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) override;

        // 实现 ICamera 接口
        void setViewMatrix(const glm::mat4& view) override;
        void setPosition(const glm::vec3& pos) override;
        glm::vec3 getPosition() const override { return m_position; }

        // 标记投影脏（当参数改变时调用）
        void markProjectionDirty() { m_projDirty = true; }

        // 更新投影矩阵（仅当脏时）
        void updateProjection();

        // 更新所有脏矩阵
        void update() override;

        // 获取投影参数（可选，用于调试）
        float getLeft() const { return m_left; }
        float getRight() const { return m_right; }
        float getBottom() const { return m_bottom; }
        float getTop() const { return m_top; }
        float getNear() const { return m_near; }
        float getFar() const { return m_far; }

    private:
        // 正交投影参数
        float m_left = -1.0f;
        float m_right = 1.0f;
        float m_bottom = -1.0f;
        float m_top = 1.0f;
        float m_near = 0.1f;
        float m_far = 100.0f;

        // 方向向量（用于 lookAt 计算）
        glm::vec3 m_up = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 m_target = glm::vec3(0.0f, 0.0f, -1.0f);

        // 脏标志
        bool m_viewDirty = true;
        bool m_projDirty = true;

        void updateViewMatrix();
        void updateProjectionMatrix();
    };
}