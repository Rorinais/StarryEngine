#include"Renderer.hpp"

namespace StarryEngine {
    Renderer::Renderer(std::shared_ptr<RHI::IRHI> rhi,
        RHI::DescriptorPoolHandle globalPool,
        std::shared_ptr<Scene::Scene> scene)
        : m_rhi(rhi), m_resMgr(rhi->getResourceManager()),m_globalPool(globalPool), m_scene(scene) {
        if (m_scene->getAllObjects().empty()) {
            LOG_ERROR("Scene has no objects, cannot create default pipeline");
            return;
        }
        auto firstObj = m_scene->getAllObjects()[0];
        auto vertexLayout = firstObj->geometry->getVertexLayout();
        //m_pipeline = std::move(CreateDefaultPipeline(rhi, globalPool, vertexLayout));

    }

    Renderer::~Renderer() {
        destroy();
    }

    void Renderer::destroy() {
        m_rhi->waitIdle();
        if (m_pipeline){
            m_pipeline.reset();
        }
    }

    void Renderer::renderFrame(RHI::RHICommandEncoder* encoder, uint32_t frameIndex, float deltaTime) {
        m_pipeline->update(*m_scene, deltaTime);
        m_pipeline->render(encoder, frameIndex);
    }

    void Renderer::onResize(uint32_t width, uint32_t height) {
        m_rhi->waitIdle();
        m_pipeline->onResize(width, height);
    }

    void Renderer::setPipeline(std::unique_ptr<IPipeline> newPipeline) {
        if (!newPipeline) return;
        destroy();
        m_pipeline = std::move(newPipeline);
        //if (newPipeline->initialize(m_rhi, m_globalPool, m_rhi->getWidth(), m_rhi->getHeight())) {
        //    m_pipeline = std::move(newPipeline);
        //    LOG_INFO("Pipeline switched successfully");
        //}
        //else {
        //    LOG_ERROR("Failed to initialize new pipeline");
        //    m_pipeline = std::move(CreateDefaultPipeline(m_rhi, m_globalPool));
        //}
    }

    std::unique_ptr<IPipeline> Renderer::CreateDefaultPipeline(
        std::shared_ptr<RHI::IRHI> rhi,
        RHI::DescriptorPoolHandle globalPool, const Assets::VertexLayout& vertexLayout) {
        auto pipeline = std::make_unique<DeferredPipeline>();

        if (pipeline->initialize(rhi, globalPool, rhi->getWidth(), rhi->getHeight(), vertexLayout)) {
            return pipeline;
        }
        LOG_ERROR("Failed to create default pipeline");
        return nullptr;
    }
}