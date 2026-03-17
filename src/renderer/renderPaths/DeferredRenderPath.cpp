#include"DeferredRenderPath.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine {
    bool DeferredRenderPath::initialize(const Assets::VertexLayout& vertexLayout) {
        m_vertexLayout = vertexLayout;

        m_recorder = std::make_shared<RenderGraph::MeshDrawRecorder>(m_resMgr);
        m_gridRecorder = std::make_shared<RenderGraph::MeshDrawRecorder>(m_resMgr);

        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            {0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment},
            {1, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment}
        };
        m_descriptorSetLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);
        if (!m_descriptorSetLayout.isValid()) {
            LOG_ERROR("Failed to create descriptor set layout");
            return false;
        }

        RHI::PipelineLayoutDesc pipelineLayoutDesc;
        pipelineLayoutDesc.descriptorSetLayouts = { m_descriptorSetLayout };
        m_pipelineLayout = m_resMgr->createPipelineLayout(pipelineLayoutDesc);
        if (!m_pipelineLayout.isValid()) {
            LOG_ERROR("Failed to create pipeline layout");
            return false;
        }

        m_recorder->setPipelineLayout(m_pipelineLayout);
        m_gridRecorder->setPipelineLayout(m_pipelineLayout);

        if (!createGridResources()) {
            LOG_ERROR("Failed to create grid resources");
            return false;
        }

        if (!createRenderGraph()) {
            return false;
        }

        m_recorder->setPipelineGetter([this](RHI::ShaderHandle vert, RHI::ShaderHandle frag, uint32_t subpassIndex) {
            return this->getOrCreatePipeline(vert, frag,subpassIndex);
            });

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

        auto gridSubpass = mainPass->addSubpassProxy("GridSubpass")
            .addColorAttachment(m_swapchainTex)                // 使用同一个颜色附件
            .addDepthStencilAttachment(depth)                  // 使用同一个深度附件
            .setPipelineName("GridPipeline")
            .setRecorder(m_gridRecorder.get())                 // 绑定网格录制器
            .setPipelineDescription(
                m_renderGraph->createBasePipelineDesc(
                    m_gridMaterial->getVertexShader(),         // 顶点着色器
                    m_gridMaterial->getFragmentShader(),       // 片段着色器
                    m_gridGeometry->getVertexInputState(),     // 顶点输入描述
                    m_pipelineLayout,       // 管线布局
                    m_width, m_height,                         // 视口宽高（与渲染区域一致）
                    1, 1.0f,                                   // 子像素位数、深度范围最大值（通常固定）
                    RHI::PrimitiveTopology::LineList)          // 线列表拓扑
            );


        // 添加子通道，使用 setNoPipeline()，由录制器动态绑定管线
        auto geomSubpass = mainPass->addSubpassProxy("GeomSubpass")
            .addColorAttachment(m_swapchainTex)
            .addDepthStencilAttachment(depth)
            .setPipelineName("GeomPipeline")
            .setRecorder(m_recorder.get());

        // 编译 RenderGraph
        if (!m_renderGraph->compile()) {
            LOG_ERROR("Failed to compile RenderGraph");
            return false;
        }

        // 保存 RenderPass 句柄，供后续创建管线使用
        m_renderPassHandle = mainPass->getRenderPassHandle(); 

        return true;
    }

    bool DeferredRenderPath::createGridResources() {
        // ---------- 1. 生成网格顶点和索引数据（与之前相同）----------
        struct GridVertex {
            glm::vec3 position;
            glm::vec3 color;
        };
        std::vector<GridVertex> vertices;
        std::vector<uint32_t> indices;

        const float size = 50.0f;
        const int divisions = 50;
        const float step = size / divisions;
        const float half = size * 0.5f;
        const glm::vec3 colorXAxis(1.0f, 0.0f, 0.0f);
        const glm::vec3 colorYAxis(0.0f, 1.0f, 0.0f);
        const glm::vec3 colorZAxis(0.0f, 0.0f, 1.0f);
        const glm::vec3 colorLine(0.4f, 0.4f, 0.4f);

        // X 方向线条
        for (int i = 0; i <= divisions; ++i) {
            float z = -half + i * step;
            bool isXAxis = (std::abs(z) < 0.001f);
            glm::vec3 col = isXAxis ? colorXAxis : colorLine;
            vertices.push_back({ {-half, 0.0f, z}, col });
            vertices.push_back({ { half, 0.0f, z}, col });
        }

        // Z 方向线条
        for (int i = 0; i <= divisions; ++i) {
            float x = -half + i * step;
            bool isZAxis = (std::abs(x) < 0.001f);
            glm::vec3 col = isZAxis ? colorZAxis : colorLine;
            vertices.push_back({ { x, 0.0f, -half}, col });
            vertices.push_back({ { x, 0.0f,  half}, col });
        }

        // Y 轴线
        vertices.push_back({ {0.0f, -half, 0.0f}, colorYAxis });
        vertices.push_back({ {0.0f,  half, 0.0f}, colorYAxis });

        // 生成索引：每两个连续顶点构成一条线段
        for (uint32_t i = 0; i < vertices.size(); i += 2) {
            indices.push_back(i);
            indices.push_back(i + 1);
        }

        // 将顶点转换为 float 数组（用于 setVertices）
        std::vector<float> vertexData;
        vertexData.reserve(vertices.size() * 6);
        for (const auto& v : vertices) {
            vertexData.push_back(v.position.x);
            vertexData.push_back(v.position.y);
            vertexData.push_back(v.position.z);
            vertexData.push_back(v.color.r);
            vertexData.push_back(v.color.g);
            vertexData.push_back(v.color.b);
        }

        // ---------- 2. 创建网格 Geometry ----------
        m_gridGeometry = std::make_shared<Assets::Geometry>(m_resMgr);
        m_gridGeometry->setVertices(vertexData);
        m_gridGeometry->setIndices(indices);

        // 设置顶点布局（位置 + 颜色）
        Assets::VertexLayout gridLayout;
        gridLayout.addAttribute(0, 0, RHI::Format::RGB32_Float, 0);                // 位置
        gridLayout.addAttribute(1, 0, RHI::Format::RGB32_Float, 3 * sizeof(float)); // 颜色
        gridLayout.addBinding(0, 6 * sizeof(float), RHI::VertexInputRate::PerVertex); // stride = 6个float
        m_gridGeometry->setVertexLayout(gridLayout);

        // 设置单个子网格
        Assets::Submesh submesh;
        submesh.indexOffset = 0;
        submesh.indexCount = static_cast<uint32_t>(indices.size());
        submesh.materialIndex = 0;
        m_gridGeometry->setSubmeshes({ submesh });

        // 上传到 GPU（新接口）
        if (!m_gridGeometry->uploadToGPU()) {
            LOG_ERROR("Failed to upload grid geometry to GPU");
            return false;
        }

        // ---------- 3. 创建网格材质（与之前相同）----------
        m_gridMaterial = std::make_shared<Assets::Material>(m_resMgr);
        m_gridMaterial->loadShaders("assets/shaders/core/gridShader.vert", "assets/shaders/core/gridShader.frag");
        m_gridMaterial->setExternalDescriptorSetLayout(m_descriptorSetLayout);
        m_gridMaterial->createAndAddUniformBuffer(sizeof(Assets::Uniforms), 0, "GridUBO");
        if (!m_gridMaterial->allocateDescriptorSet(m_globalPool, 0)) {
            LOG_ERROR("Failed to allocate descriptor set for grid material");
            return false;
        }
        m_gridMaterial->updateDescriptorSet();

        return true;
    }

    RHI::PipelineHandle DeferredRenderPath::getOrCreatePipeline(RHI::ShaderHandle vertShader, RHI::ShaderHandle fragShader, uint32_t subpassIndex) {
        // 计算哈希（简单组合）
        size_t hash = 0;
        hash ^= std::hash<RHI::ShaderHandle>{}(vertShader) << 1;
        hash ^= std::hash<RHI::ShaderHandle>{}(fragShader);
        auto it = m_pipelineCache.find(hash);
        if (it != m_pipelineCache.end()) return it->second;

        // 创建新管线
        RHI::GraphicsPipelineDesc desc;
        desc.vertexShader = vertShader;
        desc.fragmentShader = fragShader;
        desc.vertexInput = m_vertexLayout.build();
        desc.pipelineLayoutHandle = m_pipelineLayout;
        desc.renderPass = m_renderPassHandle;
        desc.subpass = subpassIndex;
        // 设置默认渲染状态
        desc.rasterizer.cullMode = RHI::CullMode::None;
        desc.depthStencil.depthTestEnable = true;
        desc.depthStencil.depthWriteEnable = true;
        desc.depthStencil.depthCompareOp = RHI::CompareOp::Less;
        // 视口和裁剪设为动态，但需要提供默认值（会被动态状态覆盖）
        desc.viewport.viewports = { RHI::Viewport() };
        desc.viewport.scissors = { RHI::Rect2D() };
        desc.colorBlend.attachments = { RHI::BlendAttachmentState{} };
        desc.dynamicStates = { RHI::DynamicState::Viewport, RHI::DynamicState::Scissor };

        auto handle = m_resMgr->createGraphicsPipeline(desc);
        if (handle.isValid()) {
            m_pipelineCache[hash] = handle;
        }
        return handle;
    }

    void DeferredRenderPath::update(const Scene::Scene& scene, float deltaTime) {
        auto camera = scene.getActiveCamera();
        if (!camera) return;
        camera->update();

        glm::mat4 view = camera->getViewMatrix();
        glm::mat4 proj = camera->getProjMatrix();

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
                    obj->transform,
                    obj->geometry,
                    material,
                    submesh.indexOffset,
                    submesh.indexCount,
                    0
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

        std::vector<Scene::DrawItem> gridDrawItems;
        const auto& submeshes = m_gridGeometry->getSubmeshes();
        if (!submeshes.empty()) {
            const auto& submesh = submeshes[0];  
            Scene::DrawItem item;
            item.geometry = m_gridGeometry;
            item.material = m_gridMaterial;
            item.indexOffset = submesh.indexOffset;
            item.indexCount = submesh.indexCount; 
            item.transform = glm::mat4(1.0f);
            gridDrawItems.push_back(item);
        }

        if (m_gridMaterial) {
            Assets::Uniforms ubo = { glm::mat4(1.0f), view, proj };
            auto buffer = m_gridMaterial->getUniformBuffer(0);
            if (buffer.isValid()) {
                auto* bufObj = m_resMgr->getBuffer(buffer);
                bufObj->update(&ubo, sizeof(ubo), 0);
            }
        }

        m_gridRecorder->setDrawItems(gridDrawItems);
    }

    void DeferredRenderPath::render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) {
        m_renderGraph->execute(frameIndex, encoder);
    }

    void DeferredRenderPath::onResize(uint32_t width, uint32_t height) {
        m_width = width;
        m_height = height;
    }
}