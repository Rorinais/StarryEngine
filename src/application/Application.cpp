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

    void Application::run() {
        auto frameContext = m_rhi->getFrameContext();
        FrameMonitor monitor(m_window, frameContext, m_flightFrame);

        // 初始化 shader 文件时间戳
        for (const auto& entry : std::filesystem::recursive_directory_iterator("assets/shaders")) {
            if (entry.is_regular_file())
                m_shaderTimestamps[entry.path().string()] = std::filesystem::last_write_time(entry);
        }
        m_lastFileCheck = std::chrono::steady_clock::now();
        m_nextAllowedReload = m_lastFileCheck;

        while (!glfwWindowShouldClose(m_window->getHandle())) {
            glfwPollEvents();
            monitor.tick();

            // ----- 自动 Shader 热重载（文件监控 + 防抖）-----
            auto now = std::chrono::steady_clock::now();
            if (now - m_lastFileCheck > std::chrono::milliseconds(500)) {
                m_lastFileCheck = now;
                bool anyChange = false;
                for (const auto& entry : std::filesystem::recursive_directory_iterator("assets/shaders")) {
                    if (!entry.is_regular_file()) continue;
                    std::string path = entry.path().string();
                    auto currentTime = std::filesystem::last_write_time(entry);
                    auto it = m_shaderTimestamps.find(path);
                    if (it == m_shaderTimestamps.end() || currentTime > it->second) {
                        anyChange = true;
                        m_shaderTimestamps[path] = currentTime;
                    }
                }

                if (anyChange && now >= m_nextAllowedReload && m_renderer) {
                    m_rhi->waitIdle();                     // 确保渲染空闲
                    m_renderer->reloadAllShaders();        // 重载所有使用的 shader
                    m_nextAllowedReload = now + kReloadCooldown; // 冷却
                }
            }

            // 窗口尺寸变化处理
            if (m_framebufferResized) {
                m_rhi->waitIdle();
                m_framebufferResized = false;
                if (m_width == 0 || m_height == 0) continue;

                if (!m_rhi->recreateSwapChain(m_width, m_height)) {
                    std::cerr << "Failed to recreate swap chain!" << std::endl;
                    continue;
                }
                m_renderer->onResize(m_width, m_height);
                continue;
            }
            if (m_width == 0 || m_height == 0) continue;

            // 每帧逻辑
            float deltaTime = monitor.getDeltaTime();
            if (m_cameraController) {
                m_cameraController->update(deltaTime);
            }

            bool success = m_rhi->renderFrame([this, deltaTime](RHI::RHICommandEncoder* encoder, uint32_t imageIndex) {
                m_renderer->renderFrame(encoder, imageIndex, deltaTime);
                });
            monitor.updateTitle();

            // 执行延迟销毁（资源释放队列）
            m_resMgr->tickFrame();
        }
    }

    void Application::initEventDispatcher() {
        // Esc 退出
        GetEventDispatcher().subscribe(EventType::KeyPressed, [this](IEvent& e) {
            auto& ev = static_cast<KeyEvent&>(e);
            if (ev.getKey() == GLFW_KEY_ESCAPE && ev.getAction() == GLFW_PRESS) {
                glfwSetWindowShouldClose(m_window->getHandle(), GLFW_TRUE);
            }
            });

        // 窗口尺寸变化
        GetEventDispatcher().subscribe(EventType::WindowResize, [this](IEvent& e) {
            auto& ev = static_cast<WindowResizeEvent&>(e);
            int newWidth = ev.getWidth();
            int newHeight = ev.getHeight();

            if (newWidth == 0 || newHeight == 0) {
                m_width = newWidth;
                m_height = newHeight;
                m_framebufferResized = true;
                return;
            }

            m_width = newWidth;
            m_height = newHeight;
            m_framebufferResized = true;

            if (m_scene && m_scene->getActiveCamera()) {
                auto cam = std::dynamic_pointer_cast<Scene::PerspectiveCamera>(m_scene->getActiveCamera());
                if (cam) {
                    float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
                    cam->setPerspective(cam->getFov(), aspect, cam->getNear(), cam->getFar());
                    cam->updateProjection();
                }
            }
            });

        // 相机切换事件
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

            // 鼠标控制
            GetEventDispatcher().subscribe(EventType::MouseButtonPressed, [this](IEvent& e) {
                auto& ev = static_cast<MouseButtonEvent&>(e);
                if (ev.getButton() == GLFW_MOUSE_BUTTON_LEFT && ev.getAction() == GLFW_PRESS && !m_controlActive) {
                    m_controlActive = true;
                    glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    m_cameraController->setEnabled(true);
                }
                else if (ev.getButton() == GLFW_MOUSE_BUTTON_RIGHT && ev.getAction() == GLFW_PRESS && m_controlActive) {
                    m_controlActive = false;
                    glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    m_cameraController->setEnabled(false);
                }
                });

            GetEventDispatcher().subscribe(EventType::KeyPressed, [this](IEvent& e) {
                if (!m_controlActive) return;
                auto& ev = static_cast<KeyEvent&>(e);
                m_cameraController->onKeyPressed(ev.getKey(), ev.getAction());
                });

            GetEventDispatcher().subscribe(EventType::MouseMoved, [this](IEvent& e) {
                if (!m_controlActive) return;
                auto& ev = static_cast<MouseMoveEvent&>(e);
                m_cameraController->onMouseMoved(ev.getX(), ev.getY());
                });

            GetEventDispatcher().subscribe(EventType::MouseScrolled, [this](IEvent& e) {
                auto& ev = static_cast<MouseScrollEvent&>(e);
                if (!m_controlActive) return;

                bool ctrlPressed = glfwGetKey(m_window->getHandle(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                    glfwGetKey(m_window->getHandle(), GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

                float delta = static_cast<float>(ev.getYOffset()) * 1.0f;
                if (ctrlPressed) {
                    m_cameraController->setFov(delta);
                }
                else {
                    m_cameraController->onMouseScrolled(ev.getXOffset(), ev.getYOffset());
                }
                });
        }
    }

    Application::~Application() {
        if (m_rhi) m_rhi->waitIdle();
        m_rhi.reset();
        m_window.reset();
    }
}