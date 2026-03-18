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

        m_rhi = VulkanRHIFactory::createDefault(RHI::API::Vulkan, m_window, m_width, m_height, m_flightFrame);
        if (!m_rhi) LOG_ERROR("Failed to create RHI!");

        createDescriptorPool();
        createRenderer();
        initEventDispatcher();
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
                material->loadShaders("assets/shaders/core/shader.vert", "assets/shaders/core/face.frag");
            }
            else{
                material->loadShaders("assets/shaders/core/shader.vert", "assets/shaders/core/hair.frag");
            }
            material->enableDepthTest();
            material->enableDepthWrite();
            material->createAndAddUniformBuffer(sizeof(Assets::MaterialUniforms), 0, "MaterialUBO");

            if (!param.albedoTexture.empty()) {
                RHI::TextureHandle texHandle = material->addTexture(param.albedoTexture, RHI::Format::RGBA8_UNorm, "Albedo", 1);
                if (!texHandle.isValid()) {
                    LOG_ERROR("Failed to load texture: {}", param.albedoTexture);
                }
            }
            else {
                LOG_WARN("Material {} has no albedo texture", param.name);
            }

            materials.push_back(material);
        }

        m_renderer = std::make_unique<Renderer>(m_rhi, m_descriptorPool, m_scene);
        m_renderer->createGlobalSetLayout();
        m_renderer->createGlobalUniformBuffer();

        auto renderPath = std::make_unique<DeferredRenderPath>(m_rhi, m_descriptorPool, m_width, m_height);
        if (!renderPath->initialize(m_renderer->getGlobalSetLayout())) {
            LOG_ERROR("Failed to initialize renderPath");
            return;
        }

        renderPath->setGlobalDescriptorSet(m_renderer->getGlobalDescriptorSet());

        for (auto& material : materials) {
            material->setExternalDescriptorSetLayout(renderPath->getDescriptorSetLayout());
            if (!material->allocateDescriptorSet(m_descriptorPool, 1)) { 
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

        auto perspectiveCamera = std::make_shared<Scene::PerspectiveCamera>();
        perspectiveCamera->setPerspective(glm::radians(45.0f), (float)m_width / m_height, 0.1f, 100.0f);
        perspectiveCamera->lookAt(glm::vec3(0.0f, 2.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_scene->addCamera(perspectiveCamera);

        auto orthographicCamera = std::make_shared<Scene::OrthographicCamera>();
        orthographicCamera->setOrthographic(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
        orthographicCamera->lookAt(glm::vec3(0.0f, 2.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_scene->addCamera(orthographicCamera);

        m_scene->setActiveCamera(perspectiveCamera);

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
            if (m_cameraController) {
                m_cameraController->update(deltaTime);
            }

            bool success = m_rhi->renderFrame([this, deltaTime](RHI::RHICommandEncoder* encoder, uint32_t imageIndex) {
                    m_renderer->renderFrame(encoder, imageIndex, deltaTime);
             });
            monitor.updateTitle();
        }
    }

    void Application::initEventDispatcher() {
        GetEventDispatcher().subscribe(EventType::KeyPressed, [this](IEvent& e) {
            auto& ev = static_cast<KeyEvent&>(e);
            if (ev.getKey() == GLFW_KEY_ESCAPE && ev.getAction() == GLFW_PRESS) {
                glfwSetWindowShouldClose(m_window->getHandle(), GLFW_TRUE);
            }
            });

        GetEventDispatcher().subscribe(EventType::WindowResize, [this](IEvent& e) {
            auto& ev = static_cast<WindowResizeEvent&>(e);
            m_width = ev.getWidth();
            m_height = ev.getHeight();
            m_framebufferResized = true;

            if (m_scene && m_scene->getActiveCamera()) {
                auto cam = std::dynamic_pointer_cast<Scene::PerspectiveCamera>(m_scene->getActiveCamera());
                if (cam) {
                    cam->setPerspective(cam->getFov(), (float)m_width / m_height, cam->getNear(), cam->getFar());
                    cam->updateProjection(); 
                }
            }
            });

        // 订阅相机切换事件
        GetEventDispatcher().subscribe(EventType::CameraSwitch, [this](IEvent& e) {
            auto& ev = static_cast<CameraSwitchEvent&>(e);
            uint32_t index = ev.getCameraIndex();
            if (index < m_scene->getCameras().size()) {
                auto oldCamera = m_scene->getActiveCamera();
                auto newCamera = m_scene->getCamera(index);
                if (oldCamera && newCamera) {
                    glm::mat4 oldView = oldCamera->getViewMatrix();
                    glm::vec3 oldPos = oldCamera->getPosition();
                    m_scene->setActiveCamera(newCamera);
                    if (m_cameraController) {
                        m_cameraController->setCamera(newCamera, oldView, oldPos);
                    }
                }
            }
            });

        // 数字键切换相机
        GetEventDispatcher().subscribe(EventType::KeyPressed, [this](IEvent& e) {
            auto& ev = static_cast<KeyEvent&>(e);
            if (ev.getAction() == GLFW_PRESS) {
                int key = ev.getKey();
                if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
                    uint32_t index = key - GLFW_KEY_1;
                    GetEventDispatcher().dispatch<CameraSwitchEvent>(index);
                }
            }
            });

        if (m_scene->getActiveCamera()) {
            m_cameraController = std::make_unique<CameraController>(m_scene->getActiveCamera());
            m_cameraController->setEnabled(false); 

            // 鼠标按钮事件：左键激活控制，右键退出控制
            GetEventDispatcher().subscribe(EventType::MouseButtonPressed, [this](IEvent& e) {
                auto& ev = static_cast<MouseButtonEvent&>(e);
                if (ev.getButton() == GLFW_MOUSE_BUTTON_LEFT && ev.getAction() == GLFW_PRESS && !m_controlActive) {
                    // 进入控制模式
                    m_controlActive = true;
                    glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    m_cameraController->setEnabled(true);
                }
                else if (ev.getButton() == GLFW_MOUSE_BUTTON_RIGHT && ev.getAction() == GLFW_PRESS && m_controlActive) {
                    // 退出控制模式
                    m_controlActive = false;
                    glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    m_cameraController->setEnabled(false);
                }
                });

            // 键盘事件
            GetEventDispatcher().subscribe(EventType::KeyPressed, [this](IEvent& e) {
                if (!m_controlActive) return;
                auto& ev = static_cast<KeyEvent&>(e);
                m_cameraController->onKeyPressed(ev.getKey(), ev.getAction());
                });

            // 鼠标移动事件
            GetEventDispatcher().subscribe(EventType::MouseMoved, [this](IEvent& e) {
                if (!m_controlActive) return;
                auto& ev = static_cast<MouseMoveEvent&>(e);
                m_cameraController->onMouseMoved(ev.getX(), ev.getY());
                });

            // 鼠标滚轮事件
            GetEventDispatcher().subscribe(EventType::MouseScrolled, [this](IEvent& e) {
                if (!m_controlActive) return;
                auto& ev = static_cast<MouseScrollEvent&>(e);
                m_cameraController->onMouseScrolled(ev.getXOffset(), ev.getYOffset());
                });
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