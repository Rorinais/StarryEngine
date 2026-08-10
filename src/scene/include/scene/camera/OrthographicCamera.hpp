#pragma once
#include <scene/camera/ICamera.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace StarryEngine::Scene {
    class OrthographicCamera : public ICamera {
    public:
        OrthographicCamera() = default;

        void setOrthographic(float left, float right, float bottom, float top, float near_, float far_);
        void lookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) override;

        void setViewMatrix(const glm::mat4& view) override;
        void setPosition(const glm::vec3& pos) override;
        glm::vec3 getPosition() const override { return m_position; }

        void markProjectionDirty() { m_projDirty = true; }
        void updateProjection();
        void update() override;

        float getLeft() const { return m_left; }
        float getRight() const { return m_right; }
        float getBottom() const { return m_bottom; }
        float getTop() const { return m_top; }
        float getNear() const { return m_near; }
        float getFar() const { return m_far; }

    private:
        float m_left = -1.0f;
        float m_right = 1.0f;
        float m_bottom = -1.0f;
        float m_top = 1.0f;
        float m_near = 0.1f;
        float m_far = 100.0f;

        glm::vec3 m_up = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 m_target = glm::vec3(0.0f, 0.0f, -1.0f);

        bool m_viewDirty = true;
        bool m_projDirty = true;

        void updateViewMatrix();
        void updateProjectionMatrix();
    };
}