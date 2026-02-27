#include "Application.hpp"
#include <stb_image.h>

namespace StarryEngine {
    Application::Application() : m_lastFpsTime(0.0) {
        // 1. 创建窗口
        Window::Config config;
        config.width = m_width;
        config.height = m_height;
        config.title = m_title;
        config.iconPath = m_icon_path;       
        config.highDPI = false;    
		config.resizable = true;
        m_window = Window::create(config);

        // 2. 设置回调
        m_window->setKeyCallback([this](int key, int action) {
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(m_window->getHandle(), GLFW_TRUE);
        });

        m_window->setResizeCallback([this](int width, int height) {
            mFramebufferResized = true;
            m_width = width;
            m_height = height;
        });

        // 3. 配置RHI
        RHI::RHIInitConfig rhiConfig;
        rhiConfig.windowHandle = m_window->getHandle();
        rhiConfig.windowWidth = m_width;
        rhiConfig.windowHeight = m_height;

        // 应用信息
        rhiConfig.appName = "StarryEngine Application";
        rhiConfig.appVersion = { 1, 0, 0 };
        rhiConfig.engineName = "StarryEngine";
        rhiConfig.engineVersion = { 1, 0, 0 };
        rhiConfig.enableDebug = true;

        rhiConfig.debugCallback = [](RHI::MessageSeverity severity,
            RHI::MessageSource source,
            const std::string& message) {
                // 根据严重程度选择输出方式
                switch (severity) {
                case RHI::MessageSeverity::Verbose:
                    // 详细信息，通常只在调试时开启
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

        // 设备特性
        rhiConfig.deviceFeatures.samplerAnisotropy = true;
        rhiConfig.deviceFeatures.textureCompression = true;
        rhiConfig.deviceFeatures.synchronization = true;
        rhiConfig.deviceFeatures.dynamicRendering = true;

        // 交换链配置
        rhiConfig.presentMode = RHI::RHIInitConfig::PresentMode::FIFO; 
		rhiConfig.swapChainImages = m_FlightFrame; 
        rhiConfig.srgb = true;

        // 帧上下文配置
        rhiConfig.frameBuffering = m_FlightFrame; 
        rhiConfig.usePersistentCommandBuffers = true;
        rhiConfig.enableTimestamps = true;

        m_rhi = std::make_unique<VulkanRHI>();
        if (!m_rhi->initialize(rhiConfig)) {
            std::cerr << "Failed to initialize Vulkan RHI!" << std::endl;
            return;
        }

        std::cout << "Application initialized successfully!" << std::endl;
    }

    void Application::loadTexture(const char* filename) {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(filename, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (!pixels) {
            std::cerr << "Failed to load texture image: " << filename << std::endl;
            return;
        }

        // 创建纹理描述
        RHI::TextureDesc texDesc;
        texDesc.extent = { (uint32_t)texWidth, (uint32_t)texHeight, 1 };
        texDesc.format = RHI::Format::RGBA8_UNorm;  // 注意：如果使用 sRGB，需选择对应格式
        texDesc.type = RHI::TextureType::Texture2D;
        texDesc.mipLevels = 1;
        texDesc.arrayLayers = 1;
        texDesc.sampleCount = 1;
        texDesc.allowRenderTarget = false;
        texDesc.allowDepthStencil = false;
        texDesc.allowUnorderedAccess = false;
        texDesc.debugName = "MyTexture";

        mTextureHandle = m_rhi->createResource<RHI::TextureHandle,RHI::TextureDesc>(texDesc);
        mTexture = m_rhi->getResource<RHI::TextureHandle>(mTextureHandle);

        // 上传像素数据
        mTexture->update(pixels, texWidth * texHeight * 4, { RHI::ImageAspect::Color, 0, 1, 0, 1 });
        mTexture->transitionLayout(
            RHI::ImageLayout::ShaderReadOnly,          // 目标布局
            RHI::PipelineStage::Transfer,              // 源阶段（传输完成）
            RHI::PipelineStage::FragmentShader,        // 目标阶段（片元着色器采样）
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::TransferWrite), // 源访问
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderRead),    // 目标访问
            { RHI::ImageAspect::Color, 0, 1, 0, 1 }
        );
        stbi_image_free(pixels);
    }

    void Application::run() {
        std::cout << "Starting application main loop..." << std::endl;

        createShaderProgram();
        createBuffer();
        createRenderPass();

        createUniformResources();      // 创建 UBO、布局、池
        loadTexture("C:\\Users\\41384\\Desktop\\Snipaste.png");
        createSampler();
        createPipelineLayout();        // 此时布局句柄已包含两个 binding
        allocateAndUpdateDescriptorSet();

        createPipeline();

        auto frameContext = m_rhi->getFrameContext();

        double lastFrameTime = glfwGetTime();  // 上一帧结束的时间点
        uint64_t frameCounter = 0;
        
        while (!glfwWindowShouldClose(m_window->getHandle())) {
            glfwPollEvents();
            double frameStart = glfwGetTime();

            if (mFramebufferResized) {
                mFramebufferResized = false;

                if (m_width == 0 || m_height == 0) {
                    continue;
                }

                if (!m_rhi->recreateSwapChain(m_width, m_height)) {
                    std::cerr << "Failed to recreate swap chain!" << std::endl;
                }
            }

            if (m_width == 0 || m_height == 0) {
                continue;
            }

            bool success = m_rhi->renderFrame([this](RHI::RHICommandEncoder* encoder, uint32_t imageIndex) {
                // ========== 1. 更新 Uniform 缓冲区数据（每帧计算并上传 MVP 矩阵）==========
                static auto startTime = std::chrono::high_resolution_clock::now();
                auto currentTime = std::chrono::high_resolution_clock::now();
                float time = std::chrono::duration<float>(currentTime - startTime).count();

                glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                glm::mat4 proj = glm::perspective(glm::radians(45.0f), static_cast<float>(m_width) / m_height, 0.1f, 10.0f);
                //proj[1][1] *= -1;

                Uniforms ubo = { model, view, proj };
                mUniformBuffer->update(&ubo, sizeof(ubo), 0);

                // ========== 2. 开始渲染通道 ==========
                const auto& framebuffers = m_rhi->getFramebuffers();
                if (imageIndex >= framebuffers.size()) return;
                auto* fb = m_rhi->getResource<RHI::FramebufferHandle>(framebuffers[imageIndex]);
                if (!fb) return;

                RHI::RenderPassBeginInfo rpBegin{};
                rpBegin.renderPass = m_rhi->getResource<RHI::RenderPassHandle>(mRenderPassHandle)->getNativeHandle();
                rpBegin.framebuffer = fb->getNativeHandle();
                rpBegin.renderArea = { {0, 0}, {m_width, m_height} };
                rpBegin.clearValues = {
                    RHI::ClearValue{{RHI::Color::Black()}},
                    RHI::ClearValue{1.0f, 0}
                };
                encoder->beginRenderPass(rpBegin, RHI::SubpassContents::Inline);

                RHI::Viewport viewport{
                    0.0f,
                    static_cast<float>(m_height),   // y 设为窗口高度
                    static_cast<float>(m_width),    // width
                    -static_cast<float>(m_height),  // height 为负值实现 Y 轴翻转
                    0.0f, 1.0f
                };
                encoder->setViewport(viewport);
                RHI::Rect2D scissor{ {0, 0}, {m_width, m_height} };
                encoder->setScissor(scissor);

                // 绑定图形管线
                encoder->bindPipeline(m_rhi->getResource<RHI::PipelineHandle>(mPipelineHandle));

                // ========== 3. 绑定描述符集（Uniform 缓冲区）==========
                encoder->bindDescriptorSets(
                    RHI::PipelineBindPoint::Graphics,
                    m_rhi->getResource<RHI::PipelineLayoutHandle>(mPipelineLayoutHandle),  // 需要确保 mPipelineLayoutHandle 有效
                    0,                                                 // firstSet
                    { mDescriptorSetHandle },                          // 描述符集句柄列表
                    {}                                                  // 动态偏移量（无）
                );

                // ========== 4. 绑定顶点和索引缓冲区 ==========
                if (mVertexBuffer) {
                    std::vector<RHI::RHIBuffer*> buffers = { mVertexBuffer };
                    std::vector<uint64_t> offsets = { 0 };
                    encoder->bindVertexBuffers(0, buffers, offsets);
                }
                if (mIndexBuffer) {
                    encoder->bindIndexBuffer(mIndexBuffer, 0, RHI::IndexType::UInt32);
                }

                // ========== 5. 绘制立方体（36个索引） ==========
                encoder->drawIndexed(36, 1, 0, 0, 0);

                encoder->endRenderPass();
            });

            double frameEnd = glfwGetTime();   
            double frameDuration = frameEnd - frameStart;  
            double realFPS = 1.0 / frameDuration;

            // 更新标题
            double now = glfwGetTime();
            if (now - m_lastFpsTime >= 1.0) {
                const auto& stats = frameContext->getStatistics();
                uint32_t lastFrameIdx = (frameContext->getCurrentFrameIndex() + m_FlightFrame - 1) % m_FlightFrame;
                float lastGpuTime = frameContext->getFrameGPUTime(lastFrameIdx);

                std::stringstream title;
                title << m_title
                    << " | Real FPS: " << std::fixed << std::setprecision(1) << realFPS
                    << " | GPU Time: " << std::setprecision(3) << lastGpuTime << " ms"
                    << " | CPU(avg): " << stats.averageCPUTime << " ms"
                    << " | Total Frames: " << stats.totalFrames;
                glfwSetWindowTitle(m_window->getHandle(), title.str().c_str());

                m_lastFpsTime = now;
            }
        }

        std::cout << "Application main loop ended." << std::endl;
    }

    Application::~Application() {
		m_rhi.reset();
        m_window.reset();
        std::cout << "Application shutdown." << std::endl;
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