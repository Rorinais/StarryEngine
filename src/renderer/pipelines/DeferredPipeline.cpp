#include"DeferredPipeline.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine {

    bool DeferredPipeline::initialize(std::shared_ptr<RHI::IRHI> rhi,
        RHI::DescriptorPoolHandle globalPool,
        uint32_t width, uint32_t height, 
        const Assets::VertexLayout& vertexLayout) {
        m_rhi = rhi;
        m_resMgr = rhi->getResourceManager();
        m_globalPool = globalPool;
        m_width = width;
        m_height = height;
        m_vertexLayout = vertexLayout;

        // 创建 RenderGraph
        m_renderGraph = std::make_shared<RenderGraph::RenderGraph>(rhi);
        m_renderGraph->setSwapchainImageCount(rhi->getSwapChainImageCount());

        // 创建纹理（深度、颜色、交换链）
        auto depthDesc = m_renderGraph->createBaseTextureDesc(
            width, height, rhi->getDepthFormat(),
            true, false, false, RHI::TextureType::Texture2D);
        auto depth = m_renderGraph->createVirtualTexture(depthDesc, "Depth");

        std::vector<void*> swapchainViews;
        for (uint32_t i = 0; i < rhi->getSwapChainImageCount(); ++i)
            swapchainViews.push_back(rhi->getSwapChainImageView(i));
        auto swapchainDesc = m_renderGraph->createBaseTextureDesc(
            width, height, RHI::Format::BGRA8_sRGB,
            false, true, false, RHI::TextureType::Texture2D);
        m_swapchainTex = m_renderGraph->importExternalTexture(
            RHI::TextureHandle::Null(), swapchainViews,
            swapchainDesc, RHI::ImageLayout::Undefined, "Swapchain");

        // 创建录制器
        m_recorder = std::make_shared<RenderGraph::MeshDrawRecorder>(m_resMgr);

        // 创建着色器（内嵌或从文件加载）
        Assets::ShaderLoader loader(m_resMgr);
        std::string vsCode = R"(
        #version 450
        layout(location = 0) in vec3 inPosition;
        layout(location = 1) in vec3 inColor;
        layout(location = 2) in vec2 inTexCoord;
        layout(location = 0) out vec3 fragColor;
        layout(location = 1) out vec2 fragTexCoord;
        layout(set = 0, binding = 0) uniform UniformBufferObject {
            mat4 model;
            mat4 view;
            mat4 proj;
        } ubo;
        void main() {
            gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
            fragColor = inColor;
            fragTexCoord = inTexCoord;
        }
    )";
        std::string fsCode = R"(
        #version 450
        layout(location = 0) in vec3 fragColor;
        layout(location = 1) in vec2 fragTexCoord;
        layout(location = 0) out vec4 outColor;
        void main() {
            outColor = vec4(1.0, 0.0, 0.0, 1.0);
        }
    )";
        auto vertInfo = loader.loadFromSource(vsCode, RHI::ShaderStage::Vertex, "GBufferVS");
        auto fragInfo = loader.loadFromSource(fsCode, RHI::ShaderStage::Fragment, "GBufferFS");
        if (!vertInfo || !fragInfo) {
            LOG_ERROR("Failed to create shaders");
            return false;
        }
        m_recorder->setVertexShader(vsCode, "GBufferVS");
        m_recorder->setFragmentShader(fsCode, "GBufferFS");

        // 添加绑定信息（用于管线布局）
        m_recorder->addBinding(0, RHI::DescriptorType::UniformBuffer, 1,
            RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment);

        // 创建管线布局（内部创建描述符集布局）
        m_pipelineLayout = m_recorder->createPipelineLayout("GBufferPipelineLayout");
        if (!m_pipelineLayout.isValid()) {
            LOG_ERROR("Failed to create pipeline layout");
            return false;
        }
        // 保存描述符集布局，供材质分配使用
        m_descriptorSetLayout = m_recorder->getDescriptorSetLayout();

        // 将管线布局设置给录制器（用于后续绑定）
        m_recorder->setPipelineLayout(m_pipelineLayout);

        auto* mainPass = m_renderGraph->addPassNode("MainPass");
        mainPass->setRenderArea(width, height);
        mainPass->addColorOutput(m_swapchainTex)
            .setClearColor({ 0.05f, 0.05f, 0.05f, 1.0f })
            .setInitialLayout(RHI::ImageLayout::Undefined)
            .setFinalLayout(RHI::ImageLayout::PresentSrc);
        mainPass->addDepthOutput(depth)
            .setInitialLayout(RHI::ImageLayout::Undefined)
            .setFinalLayout(RHI::ImageLayout::DepthStencilAttachment)
            .setClearDepth(1.0f);

        auto geomSubpass = mainPass->addSubpassProxy("GeomSubpass")
            .addColorAttachment(m_swapchainTex)
            .addDepthStencilAttachment(depth)
            .setPipelineName("GeomPipeline")
            .setRecorder(m_recorder.get())
            .setPipelineDescription(
                m_renderGraph->createBasePipelineDesc(
                    m_recorder->getVertexShader(),
                    m_recorder->getFragmentShader(),
                    m_vertexLayout.build(),
                    m_recorder->getPipelineLayout(),
                    width, height, 1, 1.0f)
            );

        // 编译 RenderGraph
        if (!m_renderGraph->compile()) {
            LOG_ERROR("Failed to compile RenderGraph");
            return false;
        }
        return true;
    }

    void DeferredPipeline::update(const Scene::Scene& scene, float deltaTime) {
        auto camera = scene.getActiveCamera();
        if (!camera) return;
        camera->update();

        glm::mat4 view = camera->getViewMatrix();
        glm::mat4 proj = camera->getProjMatrix();
        proj[1][1] *= -1;

        std::vector<Scene::DrawItem> drawItems;
        auto objects = scene.getOpaqueObjects();

        for (auto& obj : objects) {
            if (!obj->geometry) continue;
            const auto& submeshes = obj->geometry->getSubmeshes();
            for (size_t i = 0; i < submeshes.size(); ++i) {
                const auto& submesh = submeshes[i];
                if (submesh.materialIndex >= obj->materials.size()) continue;
                auto material = obj->materials[submesh.materialIndex];
                if (!material) continue;

                // 创建绘制项
                drawItems.push_back({
                    obj->geometry,
                    material,
                    submesh.indexOffset,
                    submesh.indexCount,
                    obj->transform
                    });

                // 更新该材质的 uniform 缓冲区（假设 binding 0 是 uniform）
                Assets::Uniforms ubo = { obj->transform, view, proj };
                auto buffer = material->getUniformBuffer(0);
                if (buffer.isValid()) {
                    auto* bufObj = m_resMgr->getBuffer(buffer);
                    bufObj->update(&ubo, sizeof(ubo), 0);
                }
            }
        }

        // 按材质排序以减少状态切换
        std::sort(drawItems.begin(), drawItems.end(),
            [](const Scene::DrawItem& a, const Scene::DrawItem& b) {
                return a.material->getSortKey() < b.material->getSortKey();
            });

        m_recorder->setDrawItems(drawItems);
    }

    void DeferredPipeline::render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) {
        m_renderGraph->execute(frameIndex, encoder);
    }

    void DeferredPipeline::onResize(uint32_t width, uint32_t height) {
        m_width = width;
        m_height = height;
        // 简单重新初始化（更优的做法是只重建相关资源）
        initialize(m_rhi, m_globalPool, width, height,m_vertexLayout);
    }
}