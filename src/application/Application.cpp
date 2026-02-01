#include "Application.hpp"

namespace StarryEngine {
    Application::Application() {
        // 1. 创建窗口
        Window::Config config;
        config.width = m_width;
        config.height = m_height;
        config.title = m_title;
        config.iconPath = m_icon_path;
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

        // **重要：先禁用调试，等设备创建成功后再启用**
        rhiConfig.enableDebug = true;

        // **设置调试回调**
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
        rhiConfig.srgb = true;

        // 帧上下文配置
        rhiConfig.frameBuffering = 2; // 双缓冲
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

        while (!glfwWindowShouldClose(m_window->getHandle())) {
            glfwPollEvents();

            // 处理窗口大小变化
            if (mFramebufferResized) {
                mFramebufferResized = false;

                // 等待设备空闲
                // TODO: 实现交换链重建
                std::cout << "Window resized to " << m_width << "x" << m_height
                    << ", swapchain recreation needed." << std::endl;
            }

            // TODO: 渲染逻辑
            // 1. 开始帧
            // 2. 记录命令
            // 3. 结束帧
            // 4. 提交渲染

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
        if (m_rhi) {
            m_rhi->clear();
        }

        // 清理窗口
        m_window.reset();

        std::cout << "Application shutdown." << std::endl;
    }

} // namespace StarryEngine

int main() {
#ifdef _WIN32
    _putenv_s("VK_LAYER_PATH", "layers");
#endif 
    StarryEngine::Application app;
    app.run();
}