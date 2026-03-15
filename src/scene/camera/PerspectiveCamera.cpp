#include "PerspectiveCamera.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine::Scene {

    void PerspectiveCamera::setPerspective(float fov, float aspect, float near_, float far_) {
        m_fov = fov;
        m_aspect = aspect;
        m_near = near_;
        m_far = far_;
        m_dirty = true;
    }

    void PerspectiveCamera::lookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) {
        m_eye = eye;
        m_center = center;
        m_up = up;
        m_dirty = true;
    }

    void PerspectiveCamera::update() {
        if (m_dirty) {
            glm::mat4 view = glm::lookAt(m_eye, m_center, m_up);
            glm::mat4 proj = glm::perspective(m_fov, m_aspect, m_near, m_far);
            // Vulkan 需要翻转 Y
            proj[1][1] *= -1;
            ICamera::update(view, proj); // 调用基类存储矩阵
            m_dirty = false;
        }
    }

}