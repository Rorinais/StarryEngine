#pragma once
#include "../assets/Assets.hpp"
#include "../event/Events.hpp"
#include "../logging/Logger.hpp"
#include "../scene/Scene.hpp"
#include "graph/RenderGraph.hpp"
#include "backend/RHIFactory.hpp"
#include "pipelines/DeferredPipeline.hpp"

namespace StarryEngine {
	class Renderer {
	public:
		Renderer(std::shared_ptr<RHI::IRHI> rhi,RHI::DescriptorPoolHandle globalPool,std::shared_ptr<Scene::Scene> scene);

		~Renderer();

		void destroy();

		void renderFrame(RHI::RHICommandEncoder* encoder, uint32_t frameIndex, float deltaTime);

		void onResize(uint32_t width, uint32_t height);

		void setPipeline(std::unique_ptr<IPipeline> newPipeline);

		std::unique_ptr<IPipeline> CreateDefaultPipeline(
			std::shared_ptr<RHI::IRHI> rhi,
			RHI::DescriptorPoolHandle globalPool, const Assets::VertexLayout& vertexLayout);
	private:
		std::shared_ptr<RHI::IRHI> m_rhi;
		std::shared_ptr<RHI::ResourceManager> m_resMgr;
		RHI::DescriptorPoolHandle m_globalPool;
		std::shared_ptr<Scene::Scene> m_scene;
		std::unique_ptr<IPipeline> m_pipeline;
	};

}