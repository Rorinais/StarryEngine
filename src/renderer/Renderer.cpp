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
        if (m_renderPath){
            m_renderPath.reset();
        }
    }

    void Renderer::renderFrame(RHI::RHICommandEncoder* encoder, uint32_t frameIndex, float deltaTime) {
        m_renderPath->update(*m_scene, deltaTime);
        m_renderPath->render(encoder, frameIndex);
    }

    void Renderer::onResize(uint32_t width, uint32_t height) {
        m_rhi->waitIdle();
        m_renderPath->onResize(width, height);
    }

    void Renderer::setRenderPath(std::unique_ptr<IRenderPath> newRenderPath) {
        if (!newRenderPath) return;
        destroy();
        m_renderPath = std::move(newRenderPath);
    }

    void Renderer::createGlobalSetLayout() {
        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            {0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment},
            //{1, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment} // 天空盒贴图，暂时不设置
        };
        m_globalSetLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);
        if (!m_globalSetLayout.isValid()) {
            LOG_ERROR("Failed to create global descriptor set layout");
        }
    }
}