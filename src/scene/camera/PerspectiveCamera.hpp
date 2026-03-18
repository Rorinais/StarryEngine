#pragma once
#include "ICamera.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace StarryEngine::Scene {
    class PerspectiveCamera : public ICamera {
    public:
        PerspectiveCamera() = default;

        void setPerspective(float fov, float aspect, float near_, float far_);
        void lookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up)override;

        void setViewMatrix(const glm::mat4& view) override;
        void setPosition(const glm::vec3& pos) override;

        glm::vec3 getPosition() const override { return m_position; }
        float getFov() const { return m_fov; }
        float getNear() const { return m_near; }
        float getFar() const { return m_far; }

        void markProjectionDirty() { m_projDirty = true; }

        void updateProjection();

        void update() override;

    private:
        glm::vec3 m_up = glm::vec3(0.0f, 1.0f, 0.0f);
        float m_fov = 60.0f;
        float m_aspect = 1.0f;
        float m_near = 0.1f;
        float m_far = 100.0f;
        bool m_dirty = true;

        bool m_viewDirty = true;
        bool m_projDirty = true;

        void updateViewMatrix();
        void updateProjectionMatrix();
    };
}