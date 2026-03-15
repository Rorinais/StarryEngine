#include "Application.hpp"

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

        GetEventDispatcher().subscribe(EventType::KeyPressed,[this](IEvent& e) {
                auto& ev = static_cast<KeyEvent&>(e);
                if (ev.getKey() == GLFW_KEY_ESCAPE && ev.getAction() == GLFW_PRESS) {
                    glfwSetWindowShouldClose(m_window->getHandle(), GLFW_TRUE);
                }
        });
        
        GetEventDispatcher().subscribe(EventType::MouseButtonPressed,[this](IEvent& e) {
                auto& ev = static_cast<MouseButtonEvent&>(e);
                int button = ev.getButton();
                int action = ev.getAction();
                int mods = ev.getMods();
        });

        GetEventDispatcher().subscribe(EventType::WindowResize,[this](IEvent& e) {
                auto& ev = static_cast<WindowResizeEvent&>(e);
                m_width = ev.getWidth();
                m_height = ev.getHeight();
                m_framebufferResized = true;
                LOG_INFO("Window resized to {}x{}", m_width, m_height);
        });

        m_rhi = VulkanRHIFactory::createDefault(RHI::API::Vulkan, m_window, m_width, m_height, m_flightFrame);
        if (!m_rhi) LOG_ERROR("Failed to create RHI!");

        createDescriptorPool();
        createRenderer();
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
        m_descriptorPool = m_resMgr->createDescriptorPool(poolDesc);
    }

    void Application::createRenderer() {
        m_scene = std::make_shared<Scene::Scene>();

        // 1. 加载模型并获取顶点布局
        auto geometry = std::make_shared<Assets::Geometry>(m_resMgr);
        std::vector<Assets::MaterialParams> params;
        if (!Assets::ModelLoader::loadFromFile("assets/models/geo.obj", *geometry, params)) {
            LOG_ERROR("Failed to load model");
            return;
        }
        geometry->uploadToGPU();
        Assets::VertexLayout vertexLayout = geometry->getVertexLayout();

        // 2. 创建 DeferredPipeline，传入顶点布局
        auto pipeline = std::make_unique<DeferredPipeline>();
        if (!pipeline->initialize(m_rhi, m_descriptorPool, m_width, m_height, vertexLayout)) {
            LOG_ERROR("Failed to initialize pipeline");
            return;
        }

        // 3. 从管线获取描述符集布局
        auto dsLayout = pipeline->getDescriptorSetLayout();
        if (!dsLayout.isValid()) {
            LOG_ERROR("Pipeline descriptor set layout is invalid");
            return;
        }

        // 4. 为每个材质创建对象并初始化
        std::vector<std::shared_ptr<Assets::Material>> materials;
        for (size_t i = 0; i < params.size(); ++i) {
            auto material = std::make_shared<Assets::Material>(m_resMgr);
            material->loadShaders("assets/shaders/core/shader.vert", "assets/shaders/core/shader.frag");

            // 创建 uniform 缓冲区（binding 0），管线着色器将使用它
            auto buffer = material->createAndAddUniformBuffer(sizeof(Assets::Uniforms), 0, "UBO");
            if (!buffer.isValid()) {
                LOG_ERROR("Failed to create uniform buffer for material {}", i);
                continue;
            }

            // 设置外部布局并分配描述符集（set 0）
            material->setExternalDescriptorSetLayout(dsLayout);
            if (!material->allocateDescriptorSet(m_descriptorPool, 0)) {
                LOG_ERROR("Failed to allocate descriptor set for material {}", i);
                continue;
            }
            material->updateDescriptorSet();

            materials.push_back(material);
        }

        // 5. 创建 RenderObject 并添加到场景
        auto obj = std::make_shared<Scene::RenderObject>();
        obj->geometry = geometry;
        obj->materials = materials;
        // 缩放并平移物体以适应视锥（根据你的模型大小调整）
        obj->transform = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f)) *
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f));
        m_scene->addObject(obj);

        // 6. 创建相机
        auto camera = std::make_shared<Scene::PerspectiveCamera>();
        camera->setPerspective(45.0f, (float)m_width / m_height, 0.1f, 100.0f);
        camera->lookAt(glm::vec3(5.0f, 5.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_scene->addCamera(camera);
        m_scene->setActiveCamera(camera);

        // 7. 创建渲染器并设置已初始化的管线
        m_renderer = std::make_unique<Renderer>(m_rhi, m_descriptorPool, m_scene);
        m_renderer->setPipeline(std::move(pipeline));
    }

    void Application::run() {
        auto frameContext = m_rhi->getFrameContext();
        FrameMonitor monitor(m_window, frameContext, m_flightFrame);

        while (!glfwWindowShouldClose(m_window->getHandle())) {
            glfwPollEvents();
            monitor.tick();

            if (m_framebufferResized) {
                m_rhi->waitIdle();
                m_framebufferResized = false;
                if (m_width == 0 || m_height == 0) continue;

                if (!m_rhi->recreateSwapChain(m_width, m_height)) {
                    std::cerr << "Failed to recreate swap chain!" << std::endl;
                    continue;
                }
                m_renderer->onResize(m_width,m_height);

                continue;
            }
            if (m_width == 0 || m_height == 0) continue;

            m_deltaTime = monitor.getDeltaTime();

            bool success = m_rhi->renderFrame([this](RHI::RHICommandEncoder* encoder, uint32_t imageIndex) {
                    m_renderer->renderFrame(encoder, imageIndex, m_deltaTime);
             });
            monitor.updateTitle();
        }
    }

    Application::~Application() {
        if (m_rhi) m_rhi->waitIdle();
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