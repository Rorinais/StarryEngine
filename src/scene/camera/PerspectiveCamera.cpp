#include "PerspectiveCamera.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine::Scene {

    void PerspectiveCamera::setPerspective(float fov, float aspect, float near_, float far_) {
        m_fov = fov;
        m_aspect = aspect;
        m_near = near_;
        m_far = far_;
        m_projDirty = true;
    }

    void PerspectiveCamera::lookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) {
        m_position = eye;
        m_target = center;
        m_up = up;
        m_viewDirty = true;
    }

    void PerspectiveCamera::setViewMatrix(const glm::mat4& view) {
        glm::mat4 invView = glm::inverse(view);
        m_position = glm::vec3(invView[3]);
        m_target = m_position - glm::vec3(invView[2]);
        m_up = glm::vec3(invView[1]);
        m_viewMatrix = view;
        m_viewDirty = false; 
    }

    void PerspectiveCamera::setPosition(const glm::vec3& pos) {
        m_position = pos;
        m_viewDirty = true; 
    }

    void PerspectiveCamera::updateViewMatrix() {
        m_viewMatrix = glm::lookAt(m_position, m_target, m_up);
        m_viewDirty = false;
    }

    void PerspectiveCamera::updateProjectionMatrix() {
        // Vulkan 透视投影：右手系，深度范围 0 到 1
        m_projMatrix = glm::perspectiveRH_ZO(m_fov, m_aspect, m_near, m_far);
        // Y 轴翻转（Vulkan 坐标原点在左上角）
        m_projMatrix[1][1] *= -1;
        m_projDirty = false;
    }

    void PerspectiveCamera::update() {
        if (m_viewDirty) updateViewMatrix();
        if (m_projDirty) updateProjectionMatrix();
    }

    // 单独更新投影的方法
    void PerspectiveCamera::updateProjection() {
        if (m_projDirty) updateProjectionMatrix();
    }
}