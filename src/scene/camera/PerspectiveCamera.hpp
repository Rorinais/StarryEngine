#pragma once
#include "ICamera.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace StarryEngine::Scene {
    class PerspectiveCamera : public ICamera {
    public:
        PerspectiveCamera() = default;

        void setPerspective(float fov, float aspect, float near_, float far_);

        void lookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up);

        void update() override;

    private:
        glm::vec3 m_eye = glm::vec3(0.0f);
        glm::vec3 m_center = glm::vec3(0.0f);
        glm::vec3 m_up = glm::vec3(0.0f, 1.0f, 0.0f);
        float m_fov = 60.0f;
        float m_aspect = 1.0f;
        float m_near = 0.1f;
        float m_far = 100.0f;
        bool m_dirty = true;
    };
}