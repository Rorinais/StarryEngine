#pragma once
#include "ICamera.hpp"
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>

namespace StarryEngine {

    class CameraController {
    public:
        CameraController(std::shared_ptr<Scene::ICamera> camera,
            float moveSpeed = 5.0f,
            float mouseSensitivity = 0.1f);

        void update(float deltaTime);
        void onKeyPressed(int key, int action);
        void onMouseMoved(double x, double y);
        void onMouseScrolled(double xOffset, double yOffset);
        void setEnabled(bool enabled) { m_enabled = enabled; }
        void setCamera(std::shared_ptr<Scene::ICamera> newCamera,
            const glm::mat4& currentView,
            const glm::vec3& currentPos);

    private:
        std::shared_ptr<Scene::ICamera> m_camera;

        glm::vec3 m_position;
        float m_yaw = -90.0f;   // 初始指向 -Z
        float m_pitch = 0.0f;

        float m_moveSpeed;
        float m_mouseSensitivity;
        float m_scrollSensitivity = 1.0f;

        bool m_enabled = true;
        bool m_firstMouse = true;
        double m_lastX, m_lastY;

        std::unordered_map<int, bool> m_keyStates;

        glm::vec3 getForward() const;
        glm::vec3 getRight() const;
        void updateViewMatrix();
    };

} // namespace StarryEngine