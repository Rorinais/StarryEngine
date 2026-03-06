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

    float FrameMonitor::getTime() const {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsed = now - m_startTime;
        return elapsed.count();
    }

    Application::Application() {
        Window::Config config;
        config.width = m_width;
        config.height = m_height;
        config.title = m_title;
        config.iconPath = m_icon_path;
        config.highDPI = false;
        config.resizable = true;
        m_window = Window::create(config);

        m_window->setKeyCallback([this](int key, int action) {
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(m_window->getHandle(), GLFW_TRUE);
        });

        m_window->setResizeCallback([this](int width, int height) {
            mFramebufferResized = true;
            m_width = width;
            m_height = height;
        });

        RHI::RHIInitConfig rhiConfig;
        rhiConfig.windowHandle = m_window->getHandle();
        rhiConfig.windowWidth = m_width;
        rhiConfig.windowHeight = m_height;
        rhiConfig.appName = "StarryEngine Application";
        rhiConfig.appVersion = { 1, 0, 0 };
        rhiConfig.engineName = "StarryEngine";
        rhiConfig.engineVersion = { 1, 0, 0 };
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

        rhiConfig.enableDebug = true;
        rhiConfig.debugCallback = [](RHI::MessageSeverity severity, RHI::MessageSource source, const std::string& message) {
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

        m_rhi = std::make_unique<VulkanRHI>();
        if (!m_rhi->initialize(rhiConfig)) {
            std::cerr << "Failed to initialize Vulkan RHI!" << std::endl;
            return;
        }
        m_rhi->printAllDeivceInfo();

        createDescriptorPool();
        createGbuffer();
        createPostBuffer();
        createGrid();
        createImGuiShaders();
        buildRenderGraph();
        createImGui();

        m_rhi->printResourceStatistics();
    }

    void Application::createDescriptorPool() {
        m_resMgr = m_rhi->getResourceManager();

        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 20;
        poolDesc.poolSizes = {
            { RHI::DescriptorType::UniformBuffer, 10 },
            { RHI::DescriptorType::CombinedImageSampler, 10 },
            { RHI::DescriptorType::InputAttachment, 10 }
        };
        poolDesc.freeDescriptorSet = true;
        poolDesc.debugName = "GlobalDescriptorPool";
        mDescriptorPoolHandle = m_rhi->getResourceManager()->createDescriptorPool(poolDesc);
    }

    void Application::createGbuffer() {
        m_gbufferRecorder = std::make_shared<RenderGraph::GBufferRecorder>(m_resMgr);

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
        m_gbufferRecorder->setVertexShader(vsCode, "VertexShader");

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
        m_gbufferRecorder->setFragmentShader(fsCode, "FragmentShader");

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


        m_gbufferRecorder->setVertexBuffer(0, vertices, layout, "posBuffer");
        m_gbufferRecorder->setIndexBuffer(indices, "CubeIndexBuffer");


        m_uniformBufferHandle = m_gbufferRecorder->createAndAddUniformBuffer(sizeof(Uniforms), 0, "UniformBuffer");

        m_gbufferRecorder->addTexture("C:\\Users\\41384\\Desktop\\Snipaste.png",
            RHI::Format::RGBA8_UNorm,
            "DiffuseTexture",
            1);

        m_gbufferRecorder->createDescriptorSetLayout();
        m_gbufferRecorder->createPipelineLayout("pipelineLayout");
        m_gbufferRecorder->allocateDescriptorSet(mDescriptorPoolHandle);
        m_gbufferRecorder->updateDescriptorSet();

    }

    void Application::createGrid() {
        m_gridRecorder = std::make_shared<RenderGraph::GridRecorder>(m_resMgr);

            // 顶点着色器（不变）
            std::string vsCode = R"(
            #version 450
            layout(location = 0) in vec3 inPosition;
            layout(location = 1) in vec3 inColor;
            layout(location = 0) out vec3 fragColor;
            layout(binding = 0) uniform UniformBufferObject {
                mat4 model;
                mat4 view;
                mat4 proj;
            } ubo;
            void main() {
                gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
                fragColor = inColor;
            }
        )";
        m_gridRecorder->setVertexShader(vsCode, "GridVS");

        // 片段着色器（不变）
        std::string fsCode = R"(
            #version 450
            layout(location = 0) in vec3 fragColor;
            layout(location = 0) out vec4 outColor;
            void main() {
                outColor = vec4(fragColor, 0.5);
            }
        )";
        m_gridRecorder->setFragmentShader(fsCode, "GridFS");

        // 生成网格数据
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        const float size = 50.0f;
        const int divisions = 50;
        const float step = size / divisions;
        const float half = size * 0.5f;

        // 定义颜色常量（RGB 标识坐标轴）
        const glm::vec3 colorXAxis(1.0f, 0.0f, 0.0f);   // 红色：X 轴
        const glm::vec3 colorYAxis(0.0f, 1.0f, 0.0f);   // 绿色：Y 轴
        const glm::vec3 colorZAxis(0.0f, 0.0f, 1.0f);   // 蓝色：Z 轴
        const glm::vec3 colorLine(0.4f, 0.4f, 0.4f);     // 灰色：普通网格线

        // 添加顶点的辅助函数（接受 x, y, z 和颜色）
        auto addVertex = [&](float x, float y, float z, const glm::vec3& col) {
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(col.r);
            vertices.push_back(col.g);
            vertices.push_back(col.b);
            };

        // --- 生成平行于 X 轴的线条 (y = 0) ---
        for (int i = 0; i <= divisions; ++i) {
            float z = -half + i * step;
            // 判断是否为 X 轴（即 z ≈ 0）
            bool isXAxis = (std::abs(z) < 0.001f);
            addVertex(-half, 0.0f, z, isXAxis ? colorXAxis : colorLine);
            addVertex(half, 0.0f, z, isXAxis ? colorXAxis : colorLine);
        }

        // --- 生成平行于 Z 轴的线条 (y = 0) ---
        for (int i = 0; i <= divisions; ++i) {
            float x = -half + i * step;
            // 判断是否为 Z 轴（即 x ≈ 0）
            bool isZAxis = (std::abs(x) < 0.001f);
            addVertex(x, 0.0f, -half, isZAxis ? colorZAxis : colorLine);
            addVertex(x, 0.0f, half, isZAxis ? colorZAxis : colorLine);
        }

        // --- 生成 Y 轴线（通过原点，从 y = -half 到 y = half）---
        addVertex(0.0f, -half, 0.0f, colorYAxis);   // 起点
        addVertex(0.0f, half, 0.0f, colorYAxis);   // 终点

        // 生成索引：每两个连续顶点构成一条线段
        uint32_t vertexCount = static_cast<uint32_t>(vertices.size() / 6);
        for (uint32_t i = 0; i < vertexCount; i += 2) {
            indices.push_back(i);
            indices.push_back(i + 1);
        }

        for (size_t i = 0; i < vertices.size(); i += 6) {
            float x = vertices[i];
            float y = vertices[i + 1];
            float z = vertices[i + 2];
            float r = vertices[i + 3];
            float g = vertices[i + 4];
            float b = vertices[i + 5];
            // 可以打印感兴趣的点
        }

        // 设置顶点布局（不变）
        RenderGraph::VertexLayout layout;
        layout.addBinding(0, 6 * sizeof(float), RHI::VertexInputRate::PerVertex)
            .addAttribute(0, 0, RHI::Format::RGB32_Float)   // 位置
            .addAttribute(1, 0, RHI::Format::RGB32_Float);  // 颜色

        m_gridRecorder->setVertexBuffer(0, vertices, layout, "GridVertexBuffer");
        m_gridRecorder->setIndexBuffer(indices, "GridIndexBuffer");

        // 创建 UniformBuffer 等后续操作（不变）
        m_gridUniformBufferHandle = m_gridRecorder->createAndAddUniformBuffer(sizeof(Uniforms), 0, "GridUniformBuffer");
        m_gridRecorder->createDescriptorSetLayout();
        m_gridRecorder->createPipelineLayout("GridPipelineLayout");
        m_gridRecorder->allocateDescriptorSet(mDescriptorPoolHandle);
        m_gridRecorder->updateDescriptorSet();
    }

    void Application::createPostBuffer() {
        m_postRecorder = std::make_shared<RenderGraph::PostProcessRecorder>(m_resMgr);
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
        m_postRecorder->setVertexShader(fullscreenVS, "VertexShader");

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
        m_postRecorder->setFragmentShader(postFS, "PostFS");

        m_postRecorder->addInputAttachmentBinding(0, RHI::ShaderStage::Fragment);
        m_postRecorder->createDescriptorSetLayout();
        m_postRecorder->createPipelineLayout("PostPipelineLayout");
        m_postRecorder->allocateDescriptorSet(mDescriptorPoolHandle, 0);
    }

    void Application::createImGui() {
        auto rpHandle = m_imguiPassNode->getRenderPassHandle();
        auto* rpObj = m_resMgr->getRenderPass(rpHandle);
        VkRenderPass imguiRenderPass = static_cast<VkRenderPass>(rpObj->getNativeHandle());

        // 获取描述符池原生句柄
        auto* pool = m_resMgr->getDescriptorPool(mDescriptorPoolHandle);
        VkDescriptorPool descPool = static_cast<VkDescriptorPool>(pool->getNativeHandle());

        // 初始化 ImGuiRecorder
        m_imguiRecorder->init(m_rhi.get(), descPool, m_window->getHandle(), imguiRenderPass);

    }

    void Application::createImGuiShaders() {
        m_imguiRecorder = std::make_shared<RenderGraph::ImGuiRecorder>(m_resMgr);
        std::string vsCode = R"(
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
        vsDesc.sourcecode = vsCode;
        vsDesc.stage = RHI::ShaderStage::Vertex;
        vsDesc.debugName = "ImGuiVS";
        m_imguiVertexShader = m_resMgr->createShader(vsDesc, "ImGuiVS");

        // 片段着色器代码（输出固定颜色）
        std::string fsCode = R"(
                #version 450
                layout(location = 0) in vec2 inUV;
                layout(location = 0) out vec4 outColor;
                void main() {
                    outColor = vec4(1.0, 0.0, 1.0, 1.0); // 品红，便于调试
                }
            )";
        RHI::ShaderModuleDesc fsDesc;
        fsDesc.sourcecode = fsCode;
        fsDesc.stage = RHI::ShaderStage::Fragment;
        fsDesc.debugName = "ImGuiFS";
        m_imguiFragmentShader = m_resMgr->createShader(fsDesc, "ImGuiFS");

        // 创建空的 PipelineLayout（不需要任何描述符）
        RHI::PipelineLayoutDesc layoutDesc;
        layoutDesc.descriptorSetLayouts = {}; // 空
        layoutDesc.pushConstants = {};
        layoutDesc.debugName = "ImGuiPipelineLayout";
        m_imguiPipelineLayout = m_resMgr->createPipelineLayout(layoutDesc, "ImGuiPipelineLayout");
    }

    void Application::buildRenderGraph() {
        m_renderGraph = std::make_unique<RenderGraph::RenderGraph>(m_rhi);
        m_renderGraph->setSwapchainImageCount(m_rhi->getSwapChainImageCount());

        // 创建中间纹理
        RHI::TextureDesc intermediateDesc;
        intermediateDesc.extent = { m_width, m_height, 1 };
        intermediateDesc.format = RHI::Format::RGBA8_UNorm;
        intermediateDesc.type = RHI::TextureType::Texture2D;
        intermediateDesc.allowRenderTarget = true;
        intermediateDesc.allowInputAttachment = true;
        auto intermediateTexId = m_renderGraph->createVirtualTexture(intermediateDesc, "Intermediate");

        // 创建深度纹理
        RHI::TextureDesc depthDesc;
        depthDesc.extent = { m_width, m_height, 1 };
        depthDesc.format = m_rhi->getDepthFormat();
        depthDesc.type = RHI::TextureType::Texture2D;
        depthDesc.allowDepthStencil = true;
        auto depthTexId = m_renderGraph->createVirtualTexture(depthDesc, "Depth");

        // 导入交换链纹理
        std::vector<void*> swapchainViews;
        for (uint32_t i = 0; i < m_rhi->getSwapChainImageCount(); ++i) {
            swapchainViews.push_back(m_rhi->getSwapChainImageView(i));
        }
        RHI::TextureDesc swapchainDesc;
        swapchainDesc.extent = { m_width, m_height, 1 };
        swapchainDesc.format = RHI::Format::BGRA8_sRGB;
        swapchainDesc.type = RHI::TextureType::Texture2D;
        swapchainDesc.allowRenderTarget = true;
        auto swapchainTexId = m_renderGraph->importExternalTexture(
            RHI::TextureHandle::Null(),
            swapchainViews,
            swapchainDesc,
            RHI::ImageLayout::Undefined,
            "Swapchain"
        );

        // ========== 主 Pass ==========
        auto* mainPass = m_renderGraph->addPassNode("MainPass");

        // 声明附件（直接使用纹理 ID）
        mainPass->addColorOutput(intermediateTexId)
            .setClearColor({ 0.05f, 0.05f, 0.05f, 1.0f })
            .setFinalLayout(RHI::ImageLayout::ShaderReadOnly);  // 供后处理读取

        mainPass->addDepthOutput(depthTexId)
            .setClearDepth(1.0f);

        // 基础管线描述（与原来相同）
        RHI::GraphicsPipelineDesc basePipelineDesc;
        basePipelineDesc.topology = RHI::PrimitiveTopology::TriangleList;
        basePipelineDesc.rasterizer.cullMode = RHI::CullMode::None;
        basePipelineDesc.multisample.rasterizationSamples = 1;
        basePipelineDesc.colorBlend.attachments = { RHI::BlendAttachmentState{} };
        basePipelineDesc.dynamicStates = { RHI::DynamicState::Viewport, RHI::DynamicState::Scissor };
        basePipelineDesc.viewport.viewports = { {0, 0, (float)m_width, (float)m_height, 0, 1} };
        basePipelineDesc.viewport.scissors = { {{0, 0}, {m_width, m_height}} };

        // 子通道 0：网格
        auto gridSubpass = mainPass->addSubpassProxy("GridSubpass");
        gridSubpass.addColorAttachment(intermediateTexId)
            .addDepthStencilAttachment(depthTexId)
            .setPipelineName("GridPipeline")
            .setRecorder(m_gridRecorder.get());

        RHI::GraphicsPipelineDesc gridPipelineDesc = basePipelineDesc;
        gridPipelineDesc.topology = RHI::PrimitiveTopology::LineList;
        gridPipelineDesc.vertexShader = m_gridRecorder->getVertexShader();
        gridPipelineDesc.fragmentShader = m_gridRecorder->getFragmentShader();
        gridPipelineDesc.vertexInput = m_gridRecorder->getVertexInputState();
        gridPipelineDesc.pipelineLayoutHandle = m_gridRecorder->getPipelineLayout();
        gridPipelineDesc.depthStencil.depthTestEnable = true;
        gridPipelineDesc.depthStencil.depthWriteEnable = true;
        gridPipelineDesc.depthStencil.depthCompareOp = RHI::CompareOp::Less;
        gridPipelineDesc.rasterizer.lineWidth = 1.0f;
        gridPipelineDesc.colorBlend.attachments[0].blendEnable = false;
        gridSubpass.setPipelineDescription(gridPipelineDesc);

        // 子通道 1：几何体
        auto geomSubpass = mainPass->addSubpassProxy("GeomSubpass");
        geomSubpass.addColorAttachment(intermediateTexId)
            .addDepthStencilAttachment(depthTexId)
            .setPipelineName("GeomPipeline")
            .setRecorder(m_gbufferRecorder.get());

        RHI::GraphicsPipelineDesc geomPipelineDesc = basePipelineDesc;
        geomPipelineDesc.topology = RHI::PrimitiveTopology::TriangleList;
        geomPipelineDesc.vertexShader = m_gbufferRecorder->getVertexShader();
        geomPipelineDesc.fragmentShader = m_gbufferRecorder->getFragmentShader();
        geomPipelineDesc.vertexInput = m_gbufferRecorder->getVertexInputState();
        geomPipelineDesc.pipelineLayoutHandle = m_gbufferRecorder->getPipelineLayout();
        geomPipelineDesc.depthStencil.depthTestEnable = true;
        geomPipelineDesc.depthStencil.depthWriteEnable = true;
        geomPipelineDesc.depthStencil.depthCompareOp = RHI::CompareOp::Less;
        geomPipelineDesc.colorBlend.attachments[0].blendEnable = false;
        geomSubpass.setPipelineDescription(geomPipelineDesc);

        mainPass->setRenderArea(m_width, m_height);

        // ========== 后处理 Pass ==========
        auto* postPass = m_renderGraph->addPassNode("PostPass");

        postPass->addColorOutput(swapchainTexId)
            .setClearColor({ 0.0f, 0.0f, 0.0f, 1.0f })
            .setFinalLayout(RHI::ImageLayout::PresentSrc);

        postPass->addInput(intermediateTexId)
            .setInitialLayout(RHI::ImageLayout::ShaderReadOnly);

        auto postSubpass = postPass->addSubpassProxy("PostSubpass");
        postSubpass.addColorAttachment(swapchainTexId)
            .addInputAttachment(intermediateTexId)
            .setPipelineName("PostPipeline")
            .setRecorder(m_postRecorder.get());

        RHI::GraphicsPipelineDesc postPipelineDesc = basePipelineDesc;
        postPipelineDesc.vertexShader = m_postRecorder->getVertexShader();
        postPipelineDesc.fragmentShader = m_postRecorder->getFragmentShader();
        postPipelineDesc.vertexInput = {};
        postPipelineDesc.pipelineLayoutHandle = m_postRecorder->getPipelineLayout();
        postPipelineDesc.depthStencil.depthTestEnable = false;
        postPipelineDesc.colorBlend.attachments[0].blendEnable = false;
        postSubpass.setPipelineDescription(postPipelineDesc);

        postPass->setRenderArea(m_width, m_height);


        // ========== ImGui Pass ==========
        m_imguiPassNode = m_renderGraph->addPassNode("ImGuiPass");
        m_imguiPassNode->addColorOutput(swapchainTexId)
            .setLoadOp(RHI::AttachmentLoadOp::Load)   // 保留之前的画面
            .setStoreOp(RHI::AttachmentStoreOp::Store)
            .setInitialLayout(RHI::ImageLayout::PresentSrc)  // 关键：设置初始布局
            .setFinalLayout(RHI::ImageLayout::PresentSrc);
        m_imguiPassNode->setRenderArea(m_width, m_height);

        auto guiSubpass = m_imguiPassNode->addSubpassProxy("ImGuiRendering");
        guiSubpass.addColorAttachment(swapchainTexId)
            .setPipelineName("ImGuiPipeline")
            .setRecorder(m_imguiRecorder.get());

        RHI::GraphicsPipelineDesc guiPipelineDesc = basePipelineDesc; // 复用基础描述
        guiPipelineDesc.vertexShader = m_imguiVertexShader;
        guiPipelineDesc.fragmentShader = m_imguiFragmentShader;
        guiPipelineDesc.vertexInput = {}; // 无顶点输入
        guiPipelineDesc.pipelineLayoutHandle = m_imguiPipelineLayout;
        guiPipelineDesc.depthStencil.depthTestEnable = false;
        guiPipelineDesc.colorBlend.attachments[0].blendEnable = false;
        guiSubpass.setPipelineDescription(guiPipelineDesc);

        // 手动依赖（可选，自动分析已足够）
        //m_renderGraph->addDependency(mainPass, postPass);

        if (!m_renderGraph->compile()) {
            throw std::runtime_error("Failed to compile RenderGraph");
        }

        // 更新后处理渲染器的输入附件描述符
        RHI::TextureHandle interPhys = m_renderGraph->getPhysicalTextureHandle(intermediateTexId);
        if (!interPhys.isValid()) {
            throw std::runtime_error("Intermediate texture physical handle invalid");
        }
        m_postRecorder->updateInputAttachment(0, interPhys, RHI::ImageLayout::ShaderReadOnly);
    }

    void Application::run() {
        auto frameContext = m_rhi->getFrameContext();
        FrameMonitor monitor(m_window, frameContext, m_FlightFrame);

        while (!glfwWindowShouldClose(m_window->getHandle())) {
            glfwPollEvents();
            monitor.tick();

            if (mFramebufferResized) {
                m_rhi->waitIdle();
                mFramebufferResized = false;
                if (m_width == 0 || m_height == 0) continue;

                m_renderGraph.reset();

                if (!m_rhi->recreateSwapChain(m_width, m_height)) {
                    std::cerr << "Failed to recreate swap chain!" << std::endl;
                    continue;
                }

                buildRenderGraph();
                createImGui();

                continue;
            }
            if (m_width == 0 || m_height == 0) continue;

            bool success = m_rhi->renderFrame([&](RHI::RHICommandEncoder* encoder, uint32_t imageIndex) {
                float time = monitor.getTime();
                glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                glm::mat4 proj = glm::perspective(glm::radians(60.0f), static_cast<float>(m_width) / m_height, 0.1f, 100.0f);
                proj[1][1] *= -1;

                Uniforms ubo = { model, view, proj };
                auto* UniformBuffer = m_resMgr->getBuffer(m_uniformBufferHandle);
                UniformBuffer->update(&ubo, sizeof(ubo), 0);

                Uniforms gridUbo = { glm::mat4(1.0f), view, proj };
                auto* gridUniformBuffer = m_resMgr->getBuffer(m_gridUniformBufferHandle);
                gridUniformBuffer->update(&gridUbo, sizeof(gridUbo), 0);

                m_imguiRecorder->newFrame();

                ImGui::ShowDemoWindow();
                {
                    ImGui::Begin("Statistics");
                    ImGui::Text("FPS: %.1f", monitor.getFPS());
                    ImGui::Text("Frame Time: %.3f ms", monitor.getDeltaTime() * 1000.0f);
                    ImGui::End();
                }

                m_renderGraph->execute(imageIndex, encoder);
            });
            monitor.updateTitle();
        }
    }

    Application::~Application() {
        if (m_rhi) m_rhi->waitIdle();

        m_gbufferRecorder.reset();
        m_postRecorder.reset();
        m_renderGraph.reset();
        m_rhi.reset();
        m_window.reset();
    }
} 

int main() {
#ifdef __linux__
    char exePath[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
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