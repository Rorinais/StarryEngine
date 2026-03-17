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

        auto geometry = std::make_shared<Assets::Geometry>(m_resMgr);
        std::vector<Assets::MaterialParams> params;
        if (!Assets::ModelLoader::loadFromFile(m_resMgr,"assets/models/Griseo.obj", *geometry, params)) {
            return;
        }
        geometry->uploadToGPU();
        Assets::VertexLayout vertexLayout = geometry->getVertexLayout();

        std::vector<std::shared_ptr<Assets::Material>> materials;
        for (auto& param : params) {
            auto material = std::make_shared<Assets::Material>(m_resMgr);
            if (param.name== "body"){
                material->loadShaders("assets/shaders/core/shader.vert", "assets/shaders/core/shader.frag");
            }
            else if (param.name == "brow") {
                material->loadShaders("assets/shaders/core/shader.vert", "assets/shaders/core/shader.frag");
            }
            else if (param.name == "eyes") {
                material->loadShaders("assets/shaders/core/shader.vert", "assets/shaders/core/shader.frag");
            }
            else if (param.name == "face") {
                material->loadShaders("assets/shaders/core/shader.vert", "assets/shaders/core/shader.frag");
            }
            else{
                material->loadShaders("assets/shaders/core/shader.vert", "assets/shaders/core/shader.frag");
            }

            material->createAndAddUniformBuffer(sizeof(Assets::Uniforms), 0, "UBO");

            if (!param.albedoTexture.empty()) {
                material->addTexture(param.albedoTexture, RHI::Format::RGBA8_UNorm, "Albedo", 1);
            }
            else {
                LOG_WARN("Material {} has no albedo texture", param.name);
            }

            materials.push_back(material);
        }

        auto renderPath = std::make_unique<DeferredRenderPath>(m_rhi, m_descriptorPool, m_width, m_height);
        if (!renderPath->initialize(vertexLayout)) {
            LOG_ERROR("Failed to initialize renderPath");
            return;
        }

        auto dsLayout = renderPath->getDescriptorSetLayout();
        if (!dsLayout.isValid()) {
            LOG_ERROR("renderPath descriptor set layout is invalid");
            return;
        }

        for (auto& material : materials) {
            material->setExternalDescriptorSetLayout(dsLayout);
            if (!material->allocateDescriptorSet(m_descriptorPool, 0)) {
                LOG_ERROR("Failed to allocate descriptor set for material");
                continue;
            }
            material->updateDescriptorSet();
        }

        auto obj = std::make_shared<Scene::RenderObject>();
        obj->geometry = geometry;
        obj->materials = materials;
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.5f));
        glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
        obj->transform = translation * rotation * scale;
        m_rotatingObject = obj;
        m_scene->addObject(obj);

        auto camera = std::make_shared<Scene::PerspectiveCamera>();
        camera->setPerspective(glm::radians(45.0f), (float)m_width / m_height, 0.1f, 100.0f);
        camera->lookAt(glm::vec3(0.0f, 2.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_scene->addCamera(camera);
        m_scene->setActiveCamera(camera);

        m_renderer = std::make_unique<Renderer>(m_rhi, m_descriptorPool, m_scene);
        m_renderer->setRenderPath(std::move(renderPath));
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

            float deltaTime = monitor.getDeltaTime();
            //m_rotationAngle += deltaTime * glm::radians(90.0f); 

            //if (m_rotatingObject) {
            //    glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, .0f, 0.0f)) *
            //        glm::rotate(glm::mat4(1.0f), glm::radians(0.f), glm::vec3(0.0f, 1.0f, 0.0f)) *
            //        glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
            //    m_rotatingObject->transform = transform;
            //}

            bool success = m_rhi->renderFrame([this, deltaTime](RHI::RHICommandEncoder* encoder, uint32_t imageIndex) {
                    m_renderer->renderFrame(encoder, imageIndex, deltaTime);
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