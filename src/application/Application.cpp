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
        config.fullScreen = false;
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
        buildRenderGraph();

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

    void Application::buildRenderGraph() {
        m_renderGraph = std::make_unique<RenderGraph::RenderGraph>(m_rhi);
        m_renderGraph->setSwapchainImageCount(m_rhi->getSwapChainImageCount());

        auto intermediateDesc = m_renderGraph->createBaseTextureDesc(m_width, m_height, RHI::Format::RGBA8_UNorm,false);
        auto intermediateTexId = m_renderGraph->createVirtualTexture(intermediateDesc, "Intermediate");

        auto depthDesc = m_renderGraph->createBaseTextureDesc(m_width, m_height, m_rhi->getDepthFormat(), true, false, false);
        auto depthTexId = m_renderGraph->createVirtualTexture(depthDesc, "Depth");

        // 导入交换链纹理
        std::vector<void*> swapchainViews;
        for (uint32_t i = 0; i < m_rhi->getSwapChainImageCount(); ++i) {
            swapchainViews.push_back(m_rhi->getSwapChainImageView(i));
        }
        auto swapchainDesc = m_renderGraph->createBaseTextureDesc(m_width, m_height, RHI::Format::BGRA8_sRGB, false, true, false);
        auto swapchainTexId = m_renderGraph->importExternalTexture(
            RHI::TextureHandle::Null(), 
            swapchainViews,
            swapchainDesc, 
            RHI::ImageLayout::Undefined, 
            "Swapchain");

        renderpasses.push_back({
            std::make_unique<RenderGraph::GbufferPass>(m_resMgr, mDescriptorPoolHandle),
            intermediateTexId,depthTexId,
            });

        renderpasses.push_back({
            std::make_unique<RenderGraph::PostProcessPass>(m_resMgr, mDescriptorPoolHandle),
            intermediateTexId,swapchainTexId,RHI::ImageLayout::ShaderReadOnly
            });

        for (auto& renderpass: renderpasses){
            renderpass.renderpass->setViewport(m_width, m_height);
            renderpass.renderpass->setup(*m_renderGraph.get(), renderpass.inputTexture, renderpass.outputTexture);
        }

        if (!m_renderGraph->compile()) {
            throw std::runtime_error("Failed to compile RenderGraph");
        }

        for (auto& renderpass : renderpasses) {
            renderpass.renderpass->updateInputAttachment(renderpass.inputTexture, renderpass.finalLayout);
        }

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

                for (auto& renderpass : renderpasses) {
                    renderpass.renderpass.reset();
                }
                renderpasses.clear();
                m_renderGraph.reset();

                if (!m_rhi->recreateSwapChain(m_width, m_height)) {
                    std::cerr << "Failed to recreate swap chain!" << std::endl;
                    continue;
                }
                buildRenderGraph();

                continue;
            }
            if (m_width == 0 || m_height == 0) continue;

            bool success = m_rhi->renderFrame([&](RHI::RHICommandEncoder* encoder, uint32_t imageIndex) {
                float time = monitor.getTime();
                glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                glm::mat4 proj = glm::perspective(glm::radians(60.0f), static_cast<float>(m_width) / m_height, 0.1f, 100.0f);
                proj[1][1] *= -1;

                for (auto& renderpass : renderpasses) {
                    renderpass.renderpass->update({ model, view, proj });
                }
                m_renderGraph->execute(imageIndex, encoder);
            });
            monitor.updateTitle();
        }
    }

    Application::~Application() {
        if (m_rhi) m_rhi->waitIdle();
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