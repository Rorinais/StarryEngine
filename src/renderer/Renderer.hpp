#pragma once
#include "../assets/Assets.hpp"
#include "../event/Events.hpp"
#include "../logging/Logger.hpp"
#include "../scene/Scene.hpp"
#include "graph/RenderGraph.hpp"
#include "backend/RHIFactory.hpp"
#include "renderPaths/DeferredRenderPath.hpp"

namespace StarryEngine {
	class Renderer {
	public:
		Renderer(std::shared_ptr<RHI::IRHI> rhi,RHI::DescriptorPoolHandle globalPool,std::shared_ptr<Scene::Scene> scene);

		~Renderer();

		void destroy();

		void renderFrame(RHI::RHICommandEncoder* encoder, uint32_t frameIndex, float deltaTime);

		void onResize(uint32_t width, uint32_t height);

		void setRenderPath(std::unique_ptr<IRenderPath> newRenderPath);

		//解析场景，将数据拆分为管线描述和实时更新的数据
		void analysisSecne();

		//全局的描述符布局，MVP矩阵，
		void createGlobalSetLayout();

	private:
		std::shared_ptr<RHI::IRHI> m_rhi;
		std::shared_ptr<RHI::ResourceManager> m_resMgr;
		RHI::DescriptorPoolHandle m_globalPool;
		std::shared_ptr<Scene::Scene> m_scene;
		std::unique_ptr<IRenderPath> m_renderPath;

		RHI::DescriptorSetLayoutHandle m_globalSetLayout;
	};

}