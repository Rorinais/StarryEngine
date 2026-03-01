#include "Application.hpp"
#include <stb_image.h>

namespace StarryEngine {
    FrameMonitor::FrameMonitor(Window::Ptr window, std::shared_ptr<FrameContext> frameContext, uint32_t flightFrame)
        : m_window(window)
        , m_frameContext(frameContext)
        , m_flightFrame(flightFrame)
        , m_startTime(std::chrono::high_resolution_clock::now())
        , m_lastFrameTime(m_startTime)
        , m_deltaTime(0.0f)
        , m_fps(0.0f)
        , m_frameCount(0)
        , m_lastFPSUpdate(0.0f)
        , m_fpsUpdateInterval(1.0f)
        , m_lastTitleUpdate(0.0) {
    }

    // 每帧调用一次，更新计时和统计
    void FrameMonitor::tick() {
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> delta = currentTime - m_lastFrameTime;
        m_deltaTime = delta.count();
        m_lastFrameTime = currentTime;

        m_frameCount++;
        float now = getTime();
        if (now - m_lastFPSUpdate >= m_fpsUpdateInterval) {
            m_fps = static_cast<float>(m_frameCount) / (now - m_lastFPSUpdate);
            m_frameCount = 0;
            m_lastFPSUpdate = now;
        }
    }

    // 更新窗口标题（通常每秒一次）
    void FrameMonitor::updateTitle() {
        double now = getTime();
        if (now - m_lastTitleUpdate >= 1.0) {
            const auto& stats = m_frameContext->getStatistics();
            uint32_t lastFrameIdx = (m_frameContext->getCurrentFrameIndex() + m_flightFrame - 1) % m_flightFrame;
            float lastGpuTime = m_frameContext->getFrameGPUTime(lastFrameIdx);

            std::stringstream title;
            title << "StarryEngine"
                << " | FPS: " << std::fixed << std::setprecision(1) << m_fps
                << " | GPU Time: " << std::setprecision(3) << lastGpuTime << " ms"
                << " | CPU(avg): " << stats.averageCPUTime << " ms"
                << " | Total Frames: " << stats.totalFrames;
            glfwSetWindowTitle(m_window->getHandle(), title.str().c_str());

            m_lastTitleUpdate = now;
        }
    }

    // 获取自启动以来的时间（秒）
    float FrameMonitor::getTime() const {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsed = now - m_startTime;
        return elapsed.count();
    }

    Application::Application() {
        // 1. 创建窗口（与原代码相同）
        Window::Config config;
        config.width = m_width;
        config.height = m_height;
        config.title = m_title;
        config.iconPath = m_icon_path;
        config.highDPI = false;
        config.resizable = true;
        m_window = Window::create(config);

        m_window->setKeyCallback([this](int key, int action) {
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
                glfwSetWindowShouldClose(m_window->getHandle(), GLFW_TRUE);
            });

        m_window->setResizeCallback([this](int width, int height) {
            mFramebufferResized = true;
            m_width = width;
            m_height = height;
            });

        // 2. 初始化 RHI（与原代码相同）
        RHI::RHIInitConfig rhiConfig;
        rhiConfig.windowHandle = m_window->getHandle();
        rhiConfig.windowWidth = m_width;
        rhiConfig.windowHeight = m_height;
        rhiConfig.appName = "StarryEngine Application";
        rhiConfig.appVersion = { 1, 0, 0 };
        rhiConfig.engineName = "StarryEngine";
        rhiConfig.engineVersion = { 1, 0, 0 };
        rhiConfig.enableDebug = true;
        rhiConfig.debugCallback = [](RHI::MessageSeverity severity,
            RHI::MessageSource source,
            const std::string& message) {
                // 调试输出与原代码相同（略）
                switch (severity) {
                case RHI::MessageSeverity::Verbose:
#ifdef _DEBUG
                    std::cout << "[VERBOSE] " << message << std::endl;
#endif
                    break;
                case RHI::MessageSeverity::Info:
                    std::cout << "[INFO] " << message << std::endl;
                    break;
                case RHI::MessageSeverity::Warning:
                    std::cout << "\033[33m[WARNING]\033[0m " << message << std::endl;
                    break;
                case RHI::MessageSeverity::Error:
                    std::cerr << "\033[31m[ERROR]\033[0m " << message << std::endl;
                    break;
                case RHI::MessageSeverity::Critical:
                    std::cerr << "\033[31;1m[CRITICAL]\033[0m " << message << std::endl;
                    break;
                default:
                    std::cout << "[UNKNOWN] " << message << std::endl;
                    break;
                }
            };
        rhiConfig.deviceFeatures.samplerAnisotropy = true;
        rhiConfig.deviceFeatures.textureCompression = true;
        rhiConfig.deviceFeatures.synchronization = true;
        rhiConfig.deviceFeatures.dynamicRendering = true;
        rhiConfig.presentMode = RHI::RHIInitConfig::PresentMode::FIFO;
        rhiConfig.swapChainImages = m_FlightFrame;
        rhiConfig.srgb = true;
        rhiConfig.frameBuffering = m_FlightFrame;
        rhiConfig.usePersistentCommandBuffers = true;
        rhiConfig.enableTimestamps = true;

        m_rhi = std::make_unique<VulkanRHI>();
        if (!m_rhi->initialize(rhiConfig)) {
            std::cerr << "Failed to initialize Vulkan RHI!" << std::endl;
            return;
        }

        m_resMgr = m_rhi->getResourceManager();

        // 创建全局描述符池（用于材质分配描述符集）
        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 20;
        poolDesc.poolSizes = {
            { RHI::DescriptorType::UniformBuffer, 10 },
            { RHI::DescriptorType::CombinedImageSampler, 10 },
            { RHI::DescriptorType::InputAttachment, 10 } 
        };
        poolDesc.debugName = "GlobalDescriptorPool";
        mDescriptorPoolHandle = m_rhi->getResourceManager()->createDescriptorPool(poolDesc);

        // 创建几何体和材质
        m_geometry = std::make_shared<RenderGraph::Geometry>(m_rhi->getResourceManager());
        m_material = std::make_shared<RenderGraph::Material>(m_rhi->getResourceManager());

        createShaderProgram();
        createBuffer();
        createUniformResources();
        loadTexture();

        // 创建管线布局（仅描述符集布局，不包含渲染目标）
        createPipelineLayout();

        m_gbufferRenderer = std::make_shared<RenderGraph::GBufferRenderer>(
            m_geometry, m_material, m_resMgr);

        createPostProcessResources();
        m_postRenderer = std::make_shared<RenderGraph::PostProcessRenderer>(
            m_postVS, m_postFS, m_postPipelineLayout, m_postDescriptorSet, m_resMgr);

        // 构建 RenderGraph
        buildRenderGraph();

        if (!m_renderGraph->compile()) {
            throw std::runtime_error("Failed to compile RenderGraph");
        }

        // 创建 Framebuffer
        createFramebuffers();

        std::cout << "Application initialized successfully!" << std::endl;
    }

    Application::~Application() {
        if (m_rhi) {
            m_rhi->waitIdle();
        }

        m_gbufferRenderer.reset();
        m_postRenderer.reset();
        m_material.reset();
        m_geometry.reset();
        m_renderGraph.reset(); 
        m_rhi.reset();
        m_window.reset();
        std::cout << "Application shutdown." << std::endl;
    }

    void Application::createShaderProgram() {
        // 顶点着色器
        std::string vsCode = R"(
            #version 450
            layout(location = 0) in vec3 inPosition;
            layout(location = 1) in vec3 inColor;
            layout(location = 2) in vec2 inTexCoord;
            layout(location = 0) out vec3 fragColor;
            layout(location = 1) out vec2 fragTexCoord;
            layout(binding = 0) uniform UniformBufferObject {
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
        m_material->setVertexShader(vsCode, "VertexShader");

        // 片元着色器
        std::string fsCode = R"(
            #version 450
            layout(location = 0) in vec3 fragColor;
            layout(location = 1) in vec2 fragTexCoord;
            layout(location = 0) out vec4 outColor;
            layout(binding = 1) uniform sampler2D texSampler;
            void main() {
                outColor = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);
            }
        )";
        m_material->setFragmentShader(fsCode, "FragmentShader");
    }

    void Application::createBuffer() {
        // 顶点数据（与原代码相同）
        std::vector<float> vertices = {
            // 背面 (z = -0.5)
            -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
             0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f,
             0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,
            -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f,
            // 正面 (z = 0.5)
            -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,  0.0f, 0.0f,
             0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  1.0f, 0.0f,
             0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 1.0f,  1.0f, 1.0f,
            -0.5f,  0.5f,  0.5f,  1.0f, 0.5f, 0.5f,  0.0f, 1.0f,
            // 左面 (x = -0.5)
            -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,  0.0f, 0.0f,
            -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
            -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f,
            -0.5f,  0.5f,  0.5f,  1.0f, 0.5f, 0.5f,  0.0f, 1.0f,
            // 右面 (x = 0.5)
             0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f,
             0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  1.0f, 0.0f,
             0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 1.0f,  1.0f, 1.0f,
             0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f,
             // 顶面 (y = 0.5)
             -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f,
              0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,
              0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 1.0f,  1.0f, 1.0f,
             -0.5f,  0.5f,  0.5f,  1.0f, 0.5f, 0.5f,  0.0f, 1.0f,
             // 底面 (y = -0.5)
             -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,  0.0f, 0.0f,
              0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  1.0f, 0.0f,
              0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f,
             -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f
        };
        std::vector<uint32_t> indices = {
            0,1,2, 2,3,0,      // 背面
            4,5,6, 6,7,4,      // 正面
            8,9,10, 10,11,8,   // 左面
            12,13,14, 14,15,12,// 右面
            16,17,18, 18,19,16,// 顶面
            20,21,22, 22,23,20 // 底面
        };

        RenderGraph::VertexLayout layout;
        layout.addBinding(0, 8 * sizeof(float), RHI::VertexInputRate::PerVertex)
            .addAttribute(0, 0, RHI::Format::RGB32_Float)   // 位置
            .addAttribute(1, 0, RHI::Format::RGB32_Float)   // 颜色
            .addAttribute(2, 0, RHI::Format::RG32_Float);   // 纹理坐标

        // 设置顶点缓冲区（binding 0）
        m_geometry->setVertexBuffer(0, vertices, layout, "posBuffer");
        m_geometry->setIndexBuffer(indices, "CubeIndexBuffer");
    }

    void Application::createUniformResources() {
        m_uniformBufferHandle = m_material->createAndAddUniformBuffer(sizeof(Uniforms), 0, "UniformBuffer");
        mUniformBuffer = m_resMgr->getBuffer(m_uniformBufferHandle);
    }


    void Application::createPipelineLayout() {
        RHI::PipelineLayoutDesc layoutDesc;
        layoutDesc.descriptorSetLayouts = { m_material->getDescriptorSetLayout() };
        layoutDesc.pushConstants = {};
        layoutDesc.debugName = "MainPipelineLayout";
        mPipelineLayoutHandle = m_rhi->createResource<RHI::PipelineLayoutHandle>(layoutDesc);

        m_material->allocateDescriptorSet(mDescriptorPoolHandle, mPipelineLayoutHandle, 0);
        m_material->updateDescriptorSet();
    }

    // ---------- 后处理资源 ----------
    void Application::createPostProcessResources() {
        // 全屏顶点着色器（无需输入）
        std::string fullscreenVS = R"(
            #version 450
            layout(location = 0) out vec2 outUV;
            void main() {
                const vec3 positions[3] = vec3[](
                    vec3(-1.0, -1.0, 0.0),
                    vec3( 3.0, -1.0, 0.0),
                    vec3(-1.0,  3.0, 0.0)
                );
                gl_Position = vec4(positions[gl_VertexIndex], 1.0);
                outUV = positions[gl_VertexIndex].xy * 0.5 + 0.5;
            }
        )";
        RHI::ShaderModuleDesc vsDesc;
        vsDesc.stage = RHI::ShaderStage::Vertex;
        vsDesc.sourcecode = fullscreenVS;
        vsDesc.debugName = "PostVS";
        m_postVS = m_resMgr->createShader(vsDesc);

        // 后处理片元着色器（使用输入附件）
        std::string postFS = R"(
            #version 450
            layout(location = 0) in vec2 inUV;
            layout(location = 0) out vec4 outColor;
            layout(input_attachment_index = 0, binding = 0) uniform subpassInput inputColor;

            void main() {
                // 1. 计算像素到屏幕中心的距离（UV 范围 0-1，中心为 0.5）
                vec2 center = vec2(0.5, 0.5);
                float dist = distance(inUV, center);

                // 2. 定义渐变半径范围（可调节）
                float innerRadius = 0.0;      // 内部完全反转
                float outerRadius = 0.5;      // 外部完全保留原色（距离最大可能约 0.707，取 0.5 时圆形较大）

                // 3. 根据距离计算混合因子（平滑过渡）
                float t = clamp((dist - innerRadius) / (outerRadius - innerRadius), 0.0, 1.0);
                // t = 0 时完全反转，t = 1 时完全保留原色

                // 4. 获取原始颜色
                vec3 originalColor = subpassLoad(inputColor).rgb;

                // 5. 计算反转颜色
                vec3 invertedColor = 1.0 - originalColor;

                // 6. 线性混合
                vec3 finalColor = mix(invertedColor, originalColor, t);

                outColor = vec4(finalColor, 1.0);
            }
        )";
        RHI::ShaderModuleDesc fsDesc;
        fsDesc.stage = RHI::ShaderStage::Fragment;
        fsDesc.sourcecode = postFS;
        fsDesc.debugName = "PostFS";
        m_postFS = m_resMgr->createShader(fsDesc);

        // 创建描述符集布局（仅输入附件）
        RHI::DescriptorSetLayoutBinding inputBinding;
        inputBinding.binding = 0;
        inputBinding.type = RHI::DescriptorType::InputAttachment;
        inputBinding.count = 1;
        inputBinding.stageFlags = RHI::ShaderStage::Fragment;
        RHI::DescriptorSetLayoutDesc inputLayoutDesc;
        inputLayoutDesc.bindings = { inputBinding };
        m_postInputLayout = m_resMgr->createDescriptorSetLayout(inputLayoutDesc);

        // 创建管线布局
        RHI::PipelineLayoutDesc pipelineLayoutDesc;
        pipelineLayoutDesc.descriptorSetLayouts = { m_postInputLayout };
        m_postPipelineLayout = m_resMgr->createPipelineLayout(pipelineLayoutDesc);

        // 分配描述符集（暂时不更新，等中间纹理物理句柄可用后更新）
        RHI::DescriptorSetDesc setDesc;
        setDesc.descriptorPool = mDescriptorPoolHandle;
        setDesc.pipelineLayout = m_postPipelineLayout;
        setDesc.setIndex = 0;
        m_postDescriptorSet = m_resMgr->createDescriptorSet(setDesc);
    }

    // 加载纹理
    void Application::loadTexture() {
        m_material->addTexture("C:\\Users\\41384\\Desktop\\Snipaste.png",
            RHI::Format::RGBA8_UNorm,
            "DiffuseTexture",
            1);
        m_material->createDescriptorSetLayout();
    }

    void Application::buildRenderGraph() {
        m_renderGraph = std::make_unique<RenderGraph::RenderGraph>(m_rhi);

        // 中间纹理（几何 Pass 输出）
        RHI::TextureDesc intermediateDesc;
        intermediateDesc.extent = { m_width, m_height, 1 };
        intermediateDesc.format = RHI::Format::RGBA8_UNorm;
        intermediateDesc.type = RHI::TextureType::Texture2D;
        intermediateDesc.allowRenderTarget = true;
        intermediateDesc.allowInputAttachment = true;
        m_intermediateTexId = m_renderGraph->createVirtualTexture(intermediateDesc, "Intermediate");

        // 深度纹理
        RHI::TextureDesc depthDesc;
        depthDesc.extent = { m_width, m_height, 1 };
        depthDesc.format = m_rhi->getDepthFormat();
        depthDesc.type = RHI::TextureType::Texture2D;
        depthDesc.allowDepthStencil = true;
        m_depthTexId = m_renderGraph->createVirtualTexture(depthDesc, "Depth");

        // 交换链图像
        RHI::TextureDesc swapchainDesc;
        swapchainDesc.extent = { m_width, m_height, 1 };
        swapchainDesc.format = RHI::Format::BGRA8_sRGB;
        swapchainDesc.type = RHI::TextureType::Texture2D;
        swapchainDesc.allowRenderTarget = true;
        m_swapchainTexId = m_renderGraph->importExternalTexture(
            RHI::TextureHandle::Null(),
            swapchainDesc,
            RHI::ImageLayout::Undefined,
            "Swapchain"
        );

        // ========== 几何 Pass ==========
        auto* geomPass = m_renderGraph->addPassNode("GeometryPass");
        geomPass->setRenderArea(m_width, m_height);

        auto& geomBuilder = geomPass->getBuilder();
        geomBuilder.addColorAttachment("Color",
            RHI::Format::RGBA8_UNorm,
            RHI::ImageLayout::ShaderReadOnly,
            RHI::AttachmentLoadOp::Clear,
            RHI::AttachmentStoreOp::Store);
        geomBuilder.addDepthAttachment("Depth",
            m_rhi->getDepthFormat(),
            RHI::AttachmentLoadOp::Clear,
            RHI::AttachmentStoreOp::DontCare);

        auto& geomSubpass = geomPass->addSubpass("GeomSubpass");
        geomSubpass.addColorAttachment("Color")
            .setDepthStencilAttachment("Depth")
            .setPipelineName("GeomPipeline")
            .setRenderer(m_gbufferRenderer.get());

        geomPass->setClearColor("Color", { 0.0f, 0.0f, 0.0f, 1.0f });
        geomPass->setClearDepthStencil("Depth", 1.0f, 0);

        RHI::GraphicsPipelineDesc geomPipelineDesc;
        geomPipelineDesc.vertexShader = m_material->getVertexShader();
        geomPipelineDesc.fragmentShader = m_material->getFragmentShader();
        geomPipelineDesc.vertexInput = m_geometry->getVertexInputState();
        geomPipelineDesc.topology = RHI::PrimitiveTopology::TriangleList;
        geomPipelineDesc.rasterizer.cullMode = RHI::CullMode::None;
        geomPipelineDesc.depthStencil.depthTestEnable = true;
        geomPipelineDesc.depthStencil.depthWriteEnable = true;
        geomPipelineDesc.multisample.rasterizationSamples = 1;
        geomPipelineDesc.colorBlend.attachments = { RHI::BlendAttachmentState{} };
        geomPipelineDesc.dynamicStates = { RHI::DynamicState::Viewport, RHI::DynamicState::Scissor };
        geomPipelineDesc.viewport.viewports = { {0.0f, 0.0f, (float)m_width, (float)m_height, 0.0f, 1.0f} };
        geomPipelineDesc.viewport.scissors = { {{0,0},{m_width,m_height}} };
        geomPipelineDesc.pipelineLayoutHandle = mPipelineLayoutHandle;
        geomSubpass.setPipelineDescription(geomPipelineDesc);

        // ========== 后处理 Pass ==========
        auto* postPass = m_renderGraph->addPassNode("PostPass");
        postPass->setRenderArea(m_width, m_height);

        auto& postBuilder = postPass->getBuilder();
        postBuilder.addColorAttachment("Final",
            RHI::Format::BGRA8_sRGB,
            RHI::ImageLayout::PresentSrc,
            RHI::AttachmentLoadOp::Clear,
            RHI::AttachmentStoreOp::Store);

        postBuilder.addInputAttachment("Color",
            RHI::Format::RGBA8_UNorm,
            RHI::ImageLayout::ShaderReadOnly, 
            RHI::ImageLayout::ShaderReadOnly,  
            RHI::AttachmentLoadOp::Load,
            RHI::AttachmentStoreOp::Store);

        auto& postSubpass = postPass->addSubpass("PostSubpass");
        postSubpass.addColorAttachment("Final")
            .addInputAttachment("Color")
            .setPipelineName("PostPipeline")
            .setRenderer(m_postRenderer.get());

        postPass->setClearColor("Final", { 0.0f, 0.0f, 0.0f, 1.0f });

        RHI::GraphicsPipelineDesc postPipelineDesc;
        postPipelineDesc.vertexShader = m_postVS;
        postPipelineDesc.fragmentShader = m_postFS;
        postPipelineDesc.topology = RHI::PrimitiveTopology::TriangleList;
        postPipelineDesc.rasterizer.cullMode = RHI::CullMode::None;
        postPipelineDesc.depthStencil.depthTestEnable = false;
        postPipelineDesc.colorBlend.attachments = { RHI::BlendAttachmentState{} };
        postPipelineDesc.dynamicStates = { RHI::DynamicState::Viewport, RHI::DynamicState::Scissor };
        postPipelineDesc.viewport.viewports = { {0.0f, 0.0f, (float)m_width, (float)m_height, 0.0f, 1.0f} };
        postPipelineDesc.viewport.scissors = { {{0,0},{m_width,m_height}} };
        postPipelineDesc.pipelineLayoutHandle = m_postPipelineLayout;
        postSubpass.setPipelineDescription(postPipelineDesc);
    }


    // ---------- Framebuffer 创建 ----------
    void Application::createFramebuffers() {
        // 获取物理资源
        RHI::TextureHandle depthPhys = m_renderGraph->getPhysicalTexture(m_depthTexId);
        RHI::TextureHandle interPhys = m_renderGraph->getPhysicalTexture(m_intermediateTexId);
        if (!depthPhys.isValid() || !interPhys.isValid())
            throw std::runtime_error("Missing physical textures");

        auto* depthTex = m_resMgr->getTexture(depthPhys);
        auto* interTex = m_resMgr->getTexture(interPhys);
        void* depthView = depthTex->getDefaultView();
        void* interView = interTex->getDefaultView();

        const auto& sortedPasses = m_renderGraph->getSortedPasses();
        if (sortedPasses.size() != 2)
            throw std::runtime_error("Expected two passes");

        // 几何 Pass framebuffer（固定，不依赖交换链）
        RHI::RenderPassHandle geomRenderPass = sortedPasses[0]->getRenderPassHandle();
        auto* geomRpObj = m_resMgr->getRenderPass(geomRenderPass);
        RHI::FramebufferDesc geomFbDesc;
        geomFbDesc.renderPass = geomRpObj->getNativeHandle();
        geomFbDesc.attachments = { interView, depthView };
        geomFbDesc.extent.width = m_width;
        geomFbDesc.extent.height = m_height;
        geomFbDesc.layers = 1;
        m_geomFramebuffers.push_back(m_resMgr->createFramebuffer(geomFbDesc));

        // 后处理 Pass framebuffer（每个交换链图像一个）
        RHI::RenderPassHandle postRenderPass = sortedPasses[1]->getRenderPassHandle();
        auto* postRpObj = m_resMgr->getRenderPass(postRenderPass);
        uint32_t swapchainCount = m_rhi->getSwapChainImageCount();
        m_postFramebuffers.resize(swapchainCount);

        for (uint32_t i = 0; i < swapchainCount; ++i) {
            void* swapchainView = m_rhi->getSwapChainImageView(i);
            // 注意附件顺序：索引0 = 颜色输出，索引1 = 输入附件
            std::vector<void*> attachments = { swapchainView, interView };
            RHI::FramebufferDesc postFbDesc;
            postFbDesc.renderPass = postRpObj->getNativeHandle();
            postFbDesc.attachments = attachments;
            postFbDesc.extent.width = m_width;
            postFbDesc.extent.height = m_height;
            postFbDesc.layers = 1;
            m_postFramebuffers[i] = m_resMgr->createFramebuffer(postFbDesc);
        }

        // 更新后处理描述符集：将中间纹理绑定到输入附件
        RHI::DescriptorImageInfo imageInfo;
        imageInfo.texture = interPhys;
        imageInfo.sampler = RHI::SamplerHandle::Null(); // 输入附件不需要采样器
        imageInfo.imageLayout = RHI::ImageLayout::ShaderReadOnly;

        auto* set = m_resMgr->getDescriptorSet(m_postDescriptorSet);
        if (set) {
            set->writeInputAttachment(0, 0, interTex, RHI::ImageLayout::ShaderReadOnly);
            set->update();
        }
    }

    void Application::run() {
        std::cout << "Starting application main loop..." << std::endl;
        auto frameContext = m_rhi->getFrameContext();
        FrameMonitor monitor(m_window, frameContext, m_FlightFrame);

        while (!glfwWindowShouldClose(m_window->getHandle())) {
            glfwPollEvents();
            monitor.tick();

            if (mFramebufferResized) {
                mFramebufferResized = false;
                if (m_width == 0 || m_height == 0) continue;
                m_rhi->waitIdle();

                for (auto fb : m_geomFramebuffers) m_resMgr->destroy(fb);
                m_geomFramebuffers.clear();
                for (auto fb : m_postFramebuffers) m_resMgr->destroy(fb);
                m_postFramebuffers.clear();

                m_renderGraph.reset();

                if (!m_rhi->recreateSwapChain(m_width, m_height)) {
                    std::cerr << "Failed to recreate swap chain!" << std::endl;
                    continue;
                }

                buildRenderGraph();

                if (!m_renderGraph->compile()) {
                    throw std::runtime_error("Failed to recompile RenderGraph");
                }

                createFramebuffers();
            }

            if (m_width == 0 || m_height == 0) continue;

            bool success = m_rhi->renderFrame([&](RHI::RHICommandEncoder* encoder, uint32_t imageIndex) {
                // 更新 Uniform
                float time = monitor.getTime();
                glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                glm::mat4 proj = glm::perspective(glm::radians(45.0f), static_cast<float>(m_width) / m_height, 0.1f, 10.0f);
                proj[1][1] *= -1;
                Uniforms ubo = { model, view, proj };
                mUniformBuffer->update(&ubo, sizeof(ubo), 0);

                // 设置两个 Pass 的 framebuffer
                std::vector<RHI::FramebufferHandle> passFbs = {
                    m_geomFramebuffers[0],               // 几何 Pass（固定）
                    m_postFramebuffers[imageIndex]        // 后处理 Pass（按图像索引）
                };
                m_renderGraph->setPassFramebuffers(passFbs);
                m_renderGraph->execute(imageIndex, encoder);
                });

            monitor.updateTitle();
        }

        std::cout << "Application main loop ended." << std::endl;
    }

} // namespace StarryEngine

int main() {
#ifdef __linux__
    char exePath[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", exePath, sizeof(exePath)-1);
    if (count != -1) {
        exePath[count] = '\0';
        char* lastSlash = strrchr(exePath, '/');
        if (lastSlash) {
            *lastSlash = '\0'; 
            std::string layerPath = std::string(exePath) + "/layers";
            setenv("VK_LAYER_PATH", layerPath.c_str(), 1);
            std::string libPath = std::string(exePath);
            std::string currentLdPath = getenv("LD_LIBRARY_PATH") ? getenv("LD_LIBRARY_PATH") : "";
            setenv("LD_LIBRARY_PATH", (libPath + ":" + currentLdPath).c_str(), 1);
        }
    }
#elif _WIN32
    _putenv_s("VK_LAYER_PATH", "layers");
#endif
    
    StarryEngine::Application app;
    app.run();
}