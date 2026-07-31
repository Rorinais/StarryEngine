#include "OrthographicCamera.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine::Scene {

    void OrthographicCamera::setOrthographic(float left, float right, float bottom, float top, float near_, float far_) {
        m_left = left;
        m_right = right;
        m_bottom = bottom;
        m_top = top;
        m_near = near_;
        m_far = far_;
        m_projDirty = true;
    }

    void OrthographicCamera::lookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) {
        m_position = eye;
        m_target = center;
        m_up = up;
        m_viewDirty = true;
    }

    void OrthographicCamera::setViewMatrix(const glm::mat4& view) {
        glm::mat4 invView = glm::inverse(view);
        m_position = glm::vec3(invView[3]);
        m_target = m_position - glm::vec3(invView[2]); 
        m_up = glm::vec3(invView[1]);
        m_viewMatrix = view;
        m_viewDirty = false;
    }

    void OrthographicCamera::setPosition(const glm::vec3& pos) {
        m_position = pos;
        m_viewDirty = true;
    }

    void OrthographicCamera::updateViewMatrix() {
        m_viewMatrix = glm::lookAt(m_position, m_target, m_up);
        m_viewDirty = false;
    }

    void OrthographicCamera::updateProjectionMatrix() {
        // Vulkan 正交投影：右手系，深度范围 0 到 1
        m_projMatrix = glm::orthoRH_ZO(m_left, m_right, m_bottom, m_top, m_near, m_far);
        // Y 轴翻转（因为 Vulkan 原点在左上角）
        m_projMatrix[1][1] *= -1;
        m_projDirty = false;
    }

    void OrthographicCamera::updateProjection() {
        if (m_projDirty) updateProjectionMatrix();
    }

    void OrthographicCamera::update() {
        if (m_viewDirty) updateViewMatrix();
        if (m_projDirty) updateProjectionMatrix();
    }
}