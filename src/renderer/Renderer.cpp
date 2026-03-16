#include"Renderer.hpp"

namespace StarryEngine {
    Renderer::Renderer(std::shared_ptr<RHI::IRHI> rhi,
        RHI::DescriptorPoolHandle globalPool,
        std::shared_ptr<Scene::Scene> scene)
        : m_rhi(rhi), m_resMgr(rhi->getResourceManager()),m_globalPool(globalPool), m_scene(scene) {

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
    }
}