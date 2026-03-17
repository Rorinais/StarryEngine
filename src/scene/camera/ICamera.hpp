#pragma once
#include<glm/mat4x4.hpp>
#include"../NonRenderObject.hpp"

namespace StarryEngine::Scene {
	class ICamera {
	public:
		ICamera() : m_viewMatrix(glm::mat4(1.0f)), m_projMatrix(glm::mat4(1.0f)) {}
		virtual void update(glm::mat4 viewMat,glm::mat4 projMat){
			m_viewMatrix = viewMat;
			m_projMatrix = projMat;
		}

		virtual void update() = 0;

		glm::mat4 getViewMatrix()const { return m_viewMatrix; }
		glm::mat4 getProjMatrix()const { return m_projMatrix; }

	private:
		glm::mat4 m_viewMatrix;
		glm::mat4 m_projMatrix;
	};


}