#include <scene/camera/CameraController.hpp>
#include <scene/camera/PerspectiveCamera.hpp>
#include <memory>
#include <algorithm>
#include <GLFW/glfw3.h> 
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>


namespace StarryEngine {

    CameraController::CameraController(std::shared_ptr<Scene::ICamera> camera,
        float moveSpeed,
        float mouseSensitivity)
        : m_camera(camera)
        , m_position(camera->getPosition())
        , m_moveSpeed(moveSpeed)
        , m_mouseSensitivity(mouseSensitivity) {
    }

    void CameraController::update(float deltaTime) {
        if (!m_enabled || !m_camera) return;

        glm::vec3 moveDir(0.0f);
        if (m_keyStates[GLFW_KEY_W]) moveDir += getForward();
        if (m_keyStates[GLFW_KEY_S]) moveDir -= getForward();
        if (m_keyStates[GLFW_KEY_A]) moveDir -= getRight();
        if (m_keyStates[GLFW_KEY_D]) moveDir += getRight();
        if (m_keyStates[GLFW_KEY_Q]) moveDir -= glm::vec3(0, 1, 0); 
        if (m_keyStates[GLFW_KEY_E]) moveDir += glm::vec3(0, 1, 0); 

        if (glm::length(moveDir) > 0.001f) {
            moveDir = glm::normalize(moveDir);
            m_position += moveDir * m_moveSpeed * deltaTime;
        }

        updateViewMatrix();
    }

    void CameraController::onKeyPressed(int key, int action) {
        if (!m_enabled) return;
        if (action == GLFW_PRESS) {
            m_keyStates[key] = true;
        }
        else if (action == GLFW_RELEASE) {
            m_keyStates[key] = false;
        }
    }

    void CameraController::onMouseMoved(double x, double y) {
        if (!m_enabled) return;

        if (m_firstMouse) {
            m_lastX = x;
            m_lastY = y;
            m_firstMouse = false;
            return;
        }

        double xOffset = x - m_lastX;
        double yOffset = m_lastY - y;
        m_lastX = x;
        m_lastY = y;

        xOffset *= m_mouseSensitivity;
        yOffset *= m_mouseSensitivity;

        m_yaw += static_cast<float>(xOffset);
        m_pitch += static_cast<float>(yOffset);

        // 限制俯仰角
        if (m_pitch > 89.0f) m_pitch = 89.0f;
        if (m_pitch < -89.0f) m_pitch = -89.0f;

        updateViewMatrix();
    }

    void CameraController::onMouseScrolled(double xOffset, double yOffset) {
        if (!m_enabled) return;
        m_moveSpeed += static_cast<float>(yOffset) * m_scrollSensitivity;
        if (m_moveSpeed < 0.1f) m_moveSpeed = 0.1f;
    }

    void CameraController::setFov(float delta) {
        if (!m_camera) return;
        auto perspCam = std::dynamic_pointer_cast<Scene::PerspectiveCamera>(m_camera);
        if (!perspCam) return;

        float currentFovDeg = glm::degrees(perspCam->getFov());
        float newFovDeg = glm::clamp(currentFovDeg + delta, 30.0f, 120.0f);

        perspCam->setPerspective(glm::radians(newFovDeg),perspCam->getAspect(),perspCam->getNear(),perspCam->getFar());
        perspCam->updateProjection();
    }

    glm::vec3 CameraController::getForward() const {
        glm::vec3 front;
        front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        front.y = sin(glm::radians(m_pitch));
        front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        return glm::normalize(front);
    }

    glm::vec3 CameraController::getRight() const {
        glm::vec3 forward = getForward();
        glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        return glm::normalize(glm::cross(forward, worldUp));
    }

    void CameraController::updateViewMatrix() {
        glm::vec3 forward = getForward();
        glm::vec3 right = getRight();
        glm::vec3 up = glm::cross(right, forward);

        glm::mat4 view = glm::lookAt(m_position, m_position + forward, up);
        m_camera->setViewMatrix(view);
    }

    void CameraController::setCamera(std::shared_ptr<Scene::ICamera> newCamera,
        const glm::mat4& currentView,
        const glm::vec3& currentPos) {
        if (!newCamera) return;
        m_camera = newCamera;
        m_position = currentPos; 

        glm::vec3 forward = -glm::normalize(glm::vec3(currentView[2]));
        glm::vec3 up = glm::vec3(currentView[1]);

        m_yaw = glm::degrees(atan2(forward.z, forward.x));
        m_pitch = glm::degrees(asin(forward.y));
        
        m_camera->lookAt(m_position, m_position + forward, up);

        updateViewMatrix();
    }

} // namespace StarryEngine