#include"DeferredRenderPath.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine {
    bool DeferredRenderPath::initialize(RHI::DescriptorSetLayoutHandle globalSetLayout) {
        m_globalSetLayout = globalSetLayout;

        m_recorder = std::make_shared<RenderGraph::MeshDrawRecorder>(m_resMgr);
        m_gridRecorder = std::make_shared<RenderGraph::MeshDrawRecorder>(m_resMgr);

        // 创建材质私有布局（set 1）
        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            {0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment}, // 材质参数 UBO
            {1, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment}
        };
        m_descriptorSetLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);
        if (!m_descriptorSetLayout.isValid()) {
            LOG_ERROR("Failed to create descriptor set layout");
            return false;
        }

        // 创建普通物体的管线布局：set 0 全局 + set 1 材质布局
        RHI::PipelineLayoutDesc pipelineLayoutDesc;
        pipelineLayoutDesc.descriptorSetLayouts = { m_globalSetLayout, m_descriptorSetLayout };

        RHI::PushConstantRange pcRange;
        pcRange.stage = RHI::ShaderStage::Vertex;   // 只在顶点着色器使用
        pcRange.offset = 0;
        pcRange.size = sizeof(glm::mat4);                 // 模型矩阵大小
        pipelineLayoutDesc.pushConstants = { pcRange };

        m_pipelineLayout = m_resMgr->createPipelineLayout(pipelineLayoutDesc);
        if (!m_pipelineLayout.isValid()) {
            LOG_ERROR("Failed to create pipeline layout");
            return false;
        }

        pipelineLayoutDesc.descriptorSetLayouts = { m_globalSetLayout };
        m_pipelineLayoutGrid = m_resMgr->createPipelineLayout(pipelineLayoutDesc);
        if (!m_pipelineLayoutGrid.isValid()) {
            LOG_ERROR("Failed to create pipeline layout for grid");
            return false;
        }

        if (!createGridResources()) {
            LOG_ERROR("Failed to create grid resources");
            return false;
        }

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
        return true;
    }

    RHI::PipelineHandle DeferredRenderPath::getOrCreatePipeline(const Scene::GraphicsPipelineState& state,
        RHI::PipelineLayoutHandle layout) {
        size_t hash = std::hash<Scene::GraphicsPipelineState>{}(state);
        hash ^= std::hash<RHI::PipelineLayoutHandle>{}(layout);
        auto it = m_pipelineCache.find(hash);
        if (it != m_pipelineCache.end()) return it->second;

        RHI::GraphicsPipelineDesc desc;
        desc.vertexShader = state.vertexShader;
        desc.fragmentShader = state.fragmentShader;
        desc.vertexInput = state.vertexInput;
        desc.pipelineLayoutHandle = layout;
        desc.renderPass = state.renderPass;
        desc.subpass = state.subpassIndex;
        desc.rasterizer.cullMode = state.cullMode;
        desc.rasterizer.frontFace = state.frontFace;
        desc.rasterizer.lineWidth = state.lineWidth;
        desc.depthStencil.depthTestEnable = state.depthTestEnable;
        desc.depthStencil.depthWriteEnable = state.depthWriteEnable;
        desc.depthStencil.depthCompareOp = state.depthCompareOp;
        desc.topology = state.topology;
        desc.viewport.viewports = state.viewports;
        desc.viewport.scissors = state.scissors;
        desc.colorBlend.attachments = state.attachments;
        desc.dynamicStates = state.dynamicStates;

        auto handle = m_resMgr->createGraphicsPipeline(desc);
        if (handle.isValid()) {
            m_pipelineCache[hash] = handle;
        }
        return handle;
    }

    void DeferredRenderPath::setDrawItems(const Scene::AnalysisSceneResult& secneData) {
        m_cachedDrawItems = secneData.drawItems;
        m_cachedPipelines = secneData.PSO;
    }

    void DeferredRenderPath::update(const glm::mat4& view, const glm::mat4& proj, float deltaTime) {
        // 普通物体使用 m_pipelineLayout
        std::vector<RHI::PipelineHandle> geomPipelines;
        geomPipelines.reserve(m_cachedPipelines.size());
        for (auto& pso : m_cachedPipelines) {
            pso->renderPass = m_renderPassHandle;
            pso->subpassIndex = 1;
            geomPipelines.push_back(getOrCreatePipeline(*pso, m_pipelineLayout));  // 传入普通布局
        }
        m_recorder->setPipelines(geomPipelines);
        m_recorder->setDrawItems(m_cachedDrawItems);

        // 网格子通道（索引 0）使用 m_pipelineLayoutGrid
        if (m_gridGeometry && m_gridMaterial) {
            Scene::GraphicsPipelineState gridPso = m_gridMaterial->generatePipelineState(m_gridGeometry->getVertexInputState());
            gridPso.renderPass = m_renderPassHandle;
            gridPso.subpassIndex = 0;
            gridPso.topology = RHI::PrimitiveTopology::LineList;
            RHI::PipelineHandle gridPipeline = getOrCreatePipeline(gridPso, m_pipelineLayoutGrid);  // 传入网格布局

            std::vector<RHI::PipelineHandle> gridPipelines = { gridPipeline };
            m_gridRecorder->setPipelines(gridPipelines);

            // 生成网格绘制项
            std::vector<std::shared_ptr<Scene::DrawItem>> gridItems;
            const auto& submeshes = m_gridGeometry->getSubmeshes();
            if (!submeshes.empty()) {
                auto item = std::make_shared<Scene::DrawItem>();
                item->transform = glm::mat4(1.0f);
                item->vertexBuffer = m_gridGeometry->getVertexBuffer();
                item->indexBuffer = m_gridGeometry->getIndexBuffer();
                // 网格的描述符集只包含全局 set 0（空 set 1 由布局处理，但实际不绑定任何集）
                item->descriptorSet = { m_globalDescriptorSet };  
                item->indexOffset = submeshes[0].indexOffset;
                item->indexCount = submeshes[0].indexCount;
                item->pipelineIndex = 0;
                gridItems.push_back(item);
            }
            m_gridRecorder->setDrawItems(gridItems);
        }
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