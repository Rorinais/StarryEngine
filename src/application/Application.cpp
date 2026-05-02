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
                m_renderer->onResize(m_width, m_height);

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
            int newWidth = ev.getWidth();
            int newHeight = ev.getHeight();

            // 防止最小化时出现零尺寸
            if (newWidth == 0 || newHeight == 0) {
                // 仍然记录尺寸变化，但跳过相机投影更新
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
                auto& ev = static_cast<MouseScrollEvent&>(e);
                if (!m_controlActive) return;  // 仅在控制模式下生效

                // 检查 Ctrl 键是否按下
                bool ctrlPressed = glfwGetKey(m_window->getHandle(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                    glfwGetKey(m_window->getHandle(), GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

                float delta = static_cast<float>(ev.getYOffset()) * 1.0f;
                if (ctrlPressed) {
                    // Ctrl+滚轮：调整视野（FOV）
                    m_cameraController->setFov(delta);
                }
                else {
                    // 普通滚轮：调整移动速度（沿用原有逻辑）
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