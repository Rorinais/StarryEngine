#include "Application.hpp"
#include <stb_image.h>

namespace StarryEngine {
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

        GetEventDispatcher().subscribe(EventType::KeyPressed,
            [this](IEvent& e) {
                auto& ev = static_cast<KeyEvent&>(e);
                if (ev.getKey() == GLFW_KEY_ESCAPE && ev.getAction() == GLFW_PRESS) {
                    glfwSetWindowShouldClose(m_window->getHandle(), GLFW_TRUE);
                }
            }
        );
        
        GetEventDispatcher().subscribe(EventType::MouseButtonPressed,
            [this](IEvent& e) {
                auto& ev = static_cast<MouseButtonEvent&>(e);
                int button = ev.getButton();
                int action = ev.getAction();
                int mods = ev.getMods();
            }
        );

        GetEventDispatcher().subscribe(EventType::WindowResize,
            [this](IEvent& e) {
                auto& ev = static_cast<WindowResizeEvent&>(e);
                m_width = ev.getWidth();
                m_height = ev.getHeight();
                mFramebufferResized = true;
                LOG_INFO("Window resized to {}x{}", m_width, m_height);
            }
        );

        VulkanRHIFactory factory;
        m_rhi = factory.createDefault(RHI::API::Vulkan, m_window, m_width, m_height, m_FlightFrame);
        if (!m_rhi) {
            std::cerr << "Failed to create RHI!" << std::endl;
            return;
        }

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

        // GbufferPass: 输出颜色到 intermediate，深度到 depth
        renderpasses.push_back({
            std::make_unique<RenderGraph::GbufferPass>(m_resMgr, mDescriptorPoolHandle),
            depthTexId,                        // inputTexture
            intermediateTexId,                 // outputTexture
            RHI::ImageLayout::Undefined,       // inputInitial (深度)
            RHI::ImageLayout::DepthStencilAttachment, // inputFinal (深度)
            RHI::ImageLayout::Undefined,       // outputInitial
            RHI::ImageLayout::ShaderReadOnly  // outputFinal (供后处理读取)

            });

        //// PostProcessPass: 输出到交换链，输入来自 intermediate
        //renderpasses.push_back({
        //    std::make_unique<RenderGraph::PostProcessPass>(m_resMgr, mDescriptorPoolHandle),
        //    intermediateTexId,                  // inputTexture
        //    swapchainTexId,                    // outputTexture
        //    RHI::ImageLayout::ShaderReadOnly,  // inputInitial
        //    RHI::ImageLayout::ShaderReadOnly,   // inputFinal
        //    RHI::ImageLayout::Undefined,       // outputInitial
        //    RHI::ImageLayout::PresentSrc      // outputFinal (呈现)
        //    });

        renderpasses.push_back({
            std::make_unique<RenderGraph::ImguiPass>(m_rhi, m_window, mDescriptorPoolHandle),
            intermediateTexId, swapchainTexId,
            RHI::ImageLayout::ShaderReadOnly, RHI::ImageLayout::ShaderReadOnly,
            RHI::ImageLayout::Undefined, RHI::ImageLayout::PresentSrc
            });

        for (auto& info : renderpasses) {
            info.renderpass->setViewport(m_width, m_height);
            info.renderpass->setup(
                *m_renderGraph,
                info.inputTexture,
                info.outputTexture,  
                info.inputInitial,
                info.inputFinal,
                info.outputInitial,
                info.outputFinal
            );
        }

        if (!m_renderGraph->compile()) {
            throw std::runtime_error("Failed to compile RenderGraph");
        }

        for (auto& renderpass : renderpasses) {
            renderpass.renderpass->updateInputAttachment(renderpass.inputTexture, renderpass.inputFinal);
            if (auto* imguiPass = dynamic_cast<RenderGraph::ImguiPass*>(renderpass.renderpass.get())) {
                imguiPass->postCompile();
            }
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
    StarryEngine::Logger::init();
    StarryEngine::Logger::setShowSourceLoc(false);

    StarryEngine::Application app;
    app.run();
    StarryEngine::Logger::shutdown();
}