#include "Application.hpp"

namespace StarryEngine {
    Application::Application() {
        // 1. 创建窗口
        Window::Config config;
        config.width = m_width;
        config.height = m_height;
        config.title = m_title;
        config.iconPath = m_icon_path;       
        config.highDPI = false;                    
        m_window = Window::create(config);

        // 2. 设置回调
        m_window->setKeyCallback([this](int key, int action) {
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
                glfwSetWindowShouldClose(m_window->getHandle(), GLFW_TRUE);
            }
            });

        m_window->setResizeCallback([this](int width, int height) {
            mFramebufferResized = true;
            m_width = width;
            m_height = height;

            // TODO: 处理交换链重建
            });

        // 3. 配置RHI
        RHI::RHIInitConfig rhiConfig;

        // 设置窗口句柄
        rhiConfig.windowHandle = m_window->getHandle();

        // 设置窗口尺寸
        rhiConfig.windowWidth = m_width;
        rhiConfig.windowHeight = m_height;

        // 应用信息
        rhiConfig.appName = "StarryEngine Application";
        rhiConfig.appVersion = { 1, 0, 0 };
        rhiConfig.engineName = "StarryEngine";
        rhiConfig.engineVersion = { 1, 0, 0 };
        rhiConfig.enableDebug = true;

        rhiConfig.debugCallback = [](StarryEngine::RHI::MessageSeverity severity,
            StarryEngine::RHI::MessageSource source,
            const std::string& message) {
                // 根据严重程度选择输出方式
                switch (severity) {
                case StarryEngine::RHI::MessageSeverity::Verbose:
                    // 详细信息，通常只在调试时开启
#ifdef _DEBUG
                    std::cout << "[VERBOSE] " << message << std::endl;
#endif
                    break;
                case StarryEngine::RHI::MessageSeverity::Info:
                    std::cout << "[INFO] " << message << std::endl;
                    break;
                case StarryEngine::RHI::MessageSeverity::Warning:
                    std::cout << "\033[33m[WARNING]\033[0m " << message << std::endl;
                    break;
                case StarryEngine::RHI::MessageSeverity::Error:
                    std::cerr << "\033[31m[ERROR]\033[0m " << message << std::endl;
                    break;
                case StarryEngine::RHI::MessageSeverity::Critical:
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
        rhiConfig.presentMode = RHI::RHIInitConfig::PresentMode::FIFO; // 垂直同步
		rhiConfig.swapChainImages = m_frameCount; // 双缓冲
        rhiConfig.srgb = true;

        // 帧上下文配置
        rhiConfig.frameBuffering = m_frameCount; // 双缓冲
        rhiConfig.usePersistentCommandBuffers = true;

        // 4. 初始化Vulkan RHI
        m_rhi = std::make_unique<VulkanRHI>();
        if (!m_rhi->initialize(rhiConfig)) {
            std::cerr << "Failed to initialize Vulkan RHI!" << std::endl;
            return;
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

        //for (auto handle: shaderHandles) {
        //    m_rhi->release(handle);
        //}

        auto swapChain = m_rhi->getSwapChain();
        auto frameContext = m_rhi->getFrameContext();
        auto acquireFunc = [&](VkSemaphore imageAvailableSemaphore, VkFence inFlightFence, uint32_t& imageIndex)->VkResult {
            VkResult result = swapChain->acquireNextImage(imageAvailableSemaphore, inFlightFence, UINT16_MAX);
            if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
                imageIndex = swapChain->getCurrentImageIndex();  
            }
            return result;
            };
        auto presentFunc = [&](VkQueue presentQueue, uint32_t imageIndex, VkSemaphore renderFinishedSemaphore)->VkResult {
            return swapChain->present(presentQueue, imageIndex, renderFinishedSemaphore);
            };

        frameContext->setRecreateCallback([&](uint32_t width, uint32_t height)-> bool {
            return swapChain->recreate(width, height);
            });
        
        while (!glfwWindowShouldClose(m_window->getHandle())) {
            glfwPollEvents();

            // 处理窗口大小变化
            if (mFramebufferResized) {
                mFramebufferResized = false;
                swapChain->recreate(m_width, m_height);
                // 重新创建帧缓冲
                mFramebuffers = m_rhi->createFramebuffers(mRenderPassHandle);
            }

            FrameContext::FrameInfo frameInfo = frameContext->beginFrame(acquireFunc);

            if (frameInfo.needsRecreate) {
                if(!frameContext->isRecreationNeeded()) {
					m_rhi->waitIdle();
                    swapChain->recreate(m_width, m_height);
                    continue;
				}
            }
			auto encoder = m_rhi->getCommandEncoder(frameInfo.commandBuffer);

            RHI::RenderPassBeginInfo rpBegin{};
            rpBegin.renderPass = m_rhi->getRenderPass(mRenderPassHandle)->getNativeHandle();
            rpBegin.framebuffer = m_rhi->getFramebuffer(mFramebuffers[frameInfo.imageIndex])->getNativeHandle();  // 确保 mFramebuffers 已创建
            rpBegin.renderArea = { {0, 0}, {m_width, m_height} };
            rpBegin.clearValues = {
                RHI::ClearValue{{RHI::Color::Black()}},
                RHI::ClearValue{1.0f, 0}                 
            };
            encoder->beginRenderPass(rpBegin, RHI::SubpassContents::Inline);

            RHI::Viewport viewport{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
            encoder->setViewport(viewport);

            // 设置动态裁剪矩形
            RHI::Rect2D scissor{ {0, 0}, {m_width, m_height} };
            encoder->setScissor(scissor);

            encoder->bindPipeline(m_rhi->getPipeline(mPipelineHandle));
            encoder->draw(3, 1, 0, 0);

			encoder->endRenderPass();

			frameContext->endFrame(frameInfo);

			VkResult PresentResult = frameContext->submitFrame(frameInfo, m_rhi->getGraphicsQueue(), presentFunc);
            if (PresentResult == VK_ERROR_OUT_OF_DATE_KHR || PresentResult == VK_SUBOPTIMAL_KHR) {
				mFramebufferResized = true;
            }


            // 简单帧率限制
            static auto lastTime = glfwGetTime();
            auto currentTime = glfwGetTime();
            if (currentTime - lastTime < 1.0 / 60.0) {
                continue;
            }
            lastTime = currentTime;
        }

        std::cout << "Application main loop ended." << std::endl;
    }

    Application::~Application() {
        if (mVertexBuffer){
            delete mVertexBuffer;
            mVertexBuffer = nullptr;
        }
        if (m_rhi) {
            m_rhi->clear();
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