#include"DeferredRenderPath.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine {
    bool DeferredRenderPath::initialize(RHI::DescriptorSetLayoutHandle globalSetLayout) {
        m_globalSetLayout = globalSetLayout;

        m_recorder = std::make_shared<RenderGraph::MeshDrawRecorder>(m_resMgr);

        if (!createRenderGraph()) {
            return false;
        }

        return true;
    }

    bool DeferredRenderPath::createRenderGraph() {
        // 创建 RenderGraph
        m_renderGraph = std::make_shared<RenderGraph::RenderGraph>(m_rhi);
        m_renderGraph->setSwapchainImageCount(m_rhi->getSwapChainImageCount());

        // 创建深度纹理
        auto depthDesc = m_renderGraph->createBaseTextureDesc(
            m_width, m_height, m_rhi->getDepthFormat(),
            true, false, false, RHI::TextureType::Texture2D);
        auto depth = m_renderGraph->createVirtualTexture(depthDesc, "Depth");

        // 导入交换链纹理
        std::vector<void*> swapchainViews;
        for (uint32_t i = 0; i < m_rhi->getSwapChainImageCount(); ++i)
            swapchainViews.push_back(m_rhi->getSwapChainImageView(i));
        auto swapchainDesc = m_renderGraph->createBaseTextureDesc(
            m_width, m_height, RHI::Format::BGRA8_sRGB,
            false, true, false, RHI::TextureType::Texture2D);
        m_swapchainTex = m_renderGraph->importExternalTexture(
            RHI::TextureHandle::Null(), swapchainViews,
            swapchainDesc, RHI::ImageLayout::Undefined, "Swapchain");

        // 创建主 Pass
        auto* mainPass = m_renderGraph->addPassNode("MainPass");
        mainPass->setRenderArea(m_width, m_height);
        mainPass->addColorOutput(m_swapchainTex)
            .setClearColor({ 0.05f, 0.05f, 0.05f, 1.0f })
            .setInitialLayout(RHI::ImageLayout::Undefined)
            .setFinalLayout(RHI::ImageLayout::PresentSrc);
        mainPass->addDepthOutput(depth)
            .setInitialLayout(RHI::ImageLayout::Undefined)
            .setFinalLayout(RHI::ImageLayout::DepthStencilAttachment)
            .setClearDepth(1.0f);

        mainPass->addSubpassProxy("GridSubpass")
            .addColorAttachment(m_swapchainTex)
            .addDepthStencilAttachment(depth)
            .setPipelineName("GridPipeline")
            .setRecorder(m_recorder.get()); 

        // 编译 RenderGraph
        if (!m_renderGraph->compile()) {
            LOG_ERROR("Failed to compile RenderGraph");
            return false;
        }

        // 保存 RenderPass 句柄
        m_renderPassHandle = mainPass->getRenderPassHandle();
        return true;
    }

    RHI::PipelineHandle DeferredRenderPath::getOrCreatePipeline(
        const Scene::GraphicsPipelineState& state,
        RHI::RenderPassHandle renderPass,
        uint32_t subpassIndex) {
        return Assets::PipelineCache::getOrCreateGraphicsPipeline(m_resMgr.get(), state, renderPass, subpassIndex);
    }

    void DeferredRenderPath::setDrawItems(const Scene::AnalysisSceneResult& secneData) {
        m_cachedDrawItems = secneData.drawItems;
        m_cachedPipelines = secneData.PSO;
    }

    void DeferredRenderPath::update(const glm::mat4& view, const glm::mat4& proj, float deltaTime) {
        // 普通物体子通道 (subpass 1)
        std::vector<RHI::PipelineHandle> geomPipelines;
        geomPipelines.reserve(m_cachedPipelines.size());
        for (auto& pso : m_cachedPipelines) {
            auto pipeline = getOrCreatePipeline(*pso, m_renderPassHandle, 0);
            geomPipelines.push_back(pipeline);
        }
        m_recorder->setPipelines(geomPipelines);
        m_recorder->setDrawItems(m_cachedDrawItems);
    }

    void DeferredRenderPath::render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) {
        m_renderGraph->execute(frameIndex, encoder);
    }

    void DeferredRenderPath::onResize(uint32_t width, uint32_t height) {
        m_width = width;
        m_height = height;

        initialize(m_globalSetLayout);
    }
}