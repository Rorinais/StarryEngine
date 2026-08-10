#pragma once
#include<glm/mat4x4.hpp>
#include <scene/NonRenderObject.hpp>

namespace StarryEngine::Scene {
	class ICamera {
	public:
		ICamera() : m_viewMatrix(glm::mat4(1.0f)), m_projMatrix(glm::mat4(1.0f)) {}
		virtual void update(glm::mat4 viewMat,glm::mat4 projMat){
			m_viewMatrix = viewMat;
			m_projMatrix = projMat;
		}

		virtual void update() = 0;
		virtual void setViewMatrix(const glm::mat4& view) = 0;
		virtual glm::vec3 getPosition() const = 0;
		virtual void setPosition(const glm::vec3& pos) = 0;
		virtual void lookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) = 0;

		glm::mat4 getViewMatrix()const { return m_viewMatrix; }
		glm::mat4 getProjMatrix()const { return m_projMatrix; }

	protected:
		glm::mat4 m_viewMatrix;
		glm::mat4 m_projMatrix;
		glm::vec3 m_position;
		glm::vec3 m_target;
	};
}