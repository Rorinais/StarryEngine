#include "Application.hpp"

namespace StarryEngine {
    Application::Application() : m_lastFpsTime(0.0), m_depthFormat(RHI::Format::Undefined) {
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

        if (m_rhi) {
            m_depthFormat = m_rhi->getDefaultDepthFormat();
        }

        std::cout << "Application initialized successfully!" << std::endl;
    }

    void Application::run() {
        std::cout << "Starting application main loop..." << std::endl;

        createShaderProgram();
		createBuffer();
		createRenderPass();
		createPipelineLayout();
		createPipeline();
		createFramebuffers();

        auto frameContext = m_rhi->getFrameContext();

        double lastFrameTime = glfwGetTime();  // 上一帧结束的时间点
        uint64_t frameCounter = 0;
        
        while (!glfwWindowShouldClose(m_window->getHandle())) {
            glfwPollEvents();
            double frameStart = glfwGetTime();

            // 处理窗口大小变化
            if (mFramebufferResized) {
                mFramebufferResized = false;

                if (!m_rhi->recreateSwapChain(m_width, m_height)) {
                    std::cerr << "Failed to recreate swap chain!" << std::endl;
                }
                // 重新创建帧缓冲
                mFramebuffers = m_rhi->createFramebuffers(mRenderPassHandle,mDepthTextureHandle);
            }

            bool success = m_rhi->renderFrame([this](RHI::RHICommandEncoder* encoder, uint32_t imageIndex) {
                RHI::RenderPassBeginInfo rpBegin{};
                rpBegin.renderPass = m_rhi->getRenderPass(mRenderPassHandle)->getNativeHandle();
                rpBegin.framebuffer = m_rhi->getFramebuffer(mFramebuffers[imageIndex])->getNativeHandle();
                rpBegin.renderArea = { {0, 0}, {m_width, m_height} };
                rpBegin.clearValues = {
                    RHI::ClearValue{{RHI::Color::Black()}},
                    RHI::ClearValue{1.0f, 0}
                };
                encoder->beginRenderPass(rpBegin, RHI::SubpassContents::Inline);

                RHI::Viewport viewport{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
                encoder->setViewport(viewport);
                RHI::Rect2D scissor{ {0, 0}, {m_width, m_height} };
                encoder->setScissor(scissor);

                encoder->bindPipeline(m_rhi->getPipeline(mPipelineHandle));
                encoder->draw(3, 1, 0, 0);

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
        if (mVertexBuffer){
            delete mVertexBuffer;
            mVertexBuffer = nullptr;
        }

        // 清理窗口
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