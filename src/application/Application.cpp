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

    void Application::initImGui() {
        if (!m_imguiEnabled) return;

        m_imguiManager = std::make_unique<ImGuiManager>();

        bool ok = m_imguiManager->initialize(
            m_rhi.get(),
            m_resMgr.get(),
            m_window,
            m_width,
            m_height,
            m_flightFrame,
            RHI::Format::RGBA8_UNorm,
            m_descriptorPool
        );

        if (!ok) {
            LOG_ERROR("Failed to initialize ImGui (GLFW)");
            m_imguiManager.reset();
            return;
        }

        m_imguiRecorder = std::make_shared<ImGuiRecorder>(m_imguiManager.get());

        if (m_renderer) {
            m_renderer->addOverlayPass("ImGui", m_imguiRecorder);
            m_renderer->setImGuiManager(m_imguiManager.get(), m_flightFrame);
            m_renderer->setNeedRebuildGraph();
        }
    }

    void Application::drawImGuiPanels(float deltaTime) {
        ImGuiManager::SetCurrent(m_imguiManager.get());

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Engine")) {
                ImGui::MenuItem("Performance", nullptr, &m_showPerformancePanel);
                ImGui::MenuItem("Scene Graph", nullptr, &m_showSceneGraph);
                ImGui::MenuItem("Material", nullptr, &m_showMaterialEditor);
                ImGui::MenuItem("Demo Window", nullptr, &m_showDemoWindow);
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Esc")) {
                    glfwSetWindowShouldClose(m_window->getHandle(), GLFW_TRUE);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Tools")) {
                ImGui::MenuItem("Developer", nullptr, &m_showDeveloperTools);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // ---------- 全屏 DockSpace 容器 ----------
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGuiWindowFlags dockspace_flags = ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;

        ImGui::Begin("MainDockSpace", nullptr, dockspace_flags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        static bool first_frame = true;
        if (first_frame) {
            first_frame = false;

            if (ImGui::DockBuilderGetNode(dockspace_id) == NULL) {

                ImGui::DockBuilderRemoveNode(dockspace_id); 
                ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

                ImGuiID dock_left, dock_right, dock_right_up, dock_right_down, dock_bottom;
                dock_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.7f, nullptr, &dockspace_id);
                dock_right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.3f, nullptr, &dockspace_id);
                ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Up, 0.5f, &dock_right_up, &dock_right_down);
                dock_bottom = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.2f, nullptr, &dockspace_id);

                ImGui::DockBuilderDockWindow("Scene View", dock_left);
                ImGui::DockBuilderDockWindow("Inspector", dock_right_up);
                ImGui::DockBuilderDockWindow("Asset Browser", dock_right_down);
                ImGui::DockBuilderDockWindow("Console", dock_bottom);

                ImGui::DockBuilderFinish(dockspace_id);
            }
        }

        ImGui::End(); 

        // ---------- 具体的面板窗口 ----------
        if (ImGui::Begin("Scene View")) {
            ImTextureID texID = m_imguiManager->getSceneTextureID();
            if (texID) {
                ImVec2 size = ImGui::GetContentRegionAvail();
                ImGui::Image(texID, size);
            }
        }
        ImGui::End();

        if (ImGui::Begin("Inspector")) {
            ImGui::Text("Object Properties");
        }
        ImGui::End();

        if (ImGui::Begin("Asset Browser")) {
            ImGui::Text("Assets...");
        }
        ImGui::End();

        if (ImGui::Begin("Console")) {
            ImGui::Text("Logs...");
        }
        ImGui::End();
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

        initImGui();

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

                if (m_imguiManager) {
                    m_imguiManager->registerSceneTexture(m_resMgr.get());
                }
                continue;
            }
            if (m_width == 0 || m_height == 0) continue;

            // 每帧逻辑
            float deltaTime = monitor.getDeltaTime();
            if (m_cameraController) {
                m_cameraController->update(deltaTime);
            }

            // 在主循环中
            if (m_imguiManager && m_imguiManager->isInitialized()) {
                m_imguiManager->beginFrame();
                drawImGuiPanels(deltaTime);
                m_imguiManager->endFrame();
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

        // 键盘事件：数字键切换相机 + Ctrl 键切换摄像机控制模式
        GetEventDispatcher().subscribe(EventType::KeyPressed, [this](IEvent& e) {
            auto& ev = static_cast<KeyEvent&>(e);
            if (ev.getAction() == GLFW_PRESS) {
                int key = ev.getKey();
                // 数字键切换相机
                if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
                    uint32_t index = key - GLFW_KEY_1;
                    GetEventDispatcher().dispatch<CameraSwitchEvent>(index);
                }
                // Ctrl 键切换摄像机控制状态
                if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL) {
                    m_controlActive = !m_controlActive;
                    if (m_controlActive) {
                        glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                        if (m_cameraController) m_cameraController->setEnabled(true);
                    }
                    else {
                        glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                        if (m_cameraController) m_cameraController->setEnabled(false);
                    }
                }
            }
            });

        // 摄像机控制相关事件（仅在 m_controlActive == true 时有效）
        if (m_scene->getActiveCamera() && m_enableControl) {
            m_cameraController = std::make_unique<CameraController>(m_scene->getActiveCamera());
            m_cameraController->setEnabled(false);

            // 键盘移动（WASD 等），已在 m_controlActive 下生效
            GetEventDispatcher().subscribe(EventType::KeyPressed, [this](IEvent& e) {
                if (!m_controlActive) return;
                auto& ev = static_cast<KeyEvent&>(e);
                if (m_cameraController)
                    m_cameraController->onKeyPressed(ev.getKey(), ev.getAction());
                });

            // 鼠标移动：旋转视角
            GetEventDispatcher().subscribe(EventType::MouseMoved, [this](IEvent& e) {
                if (!m_controlActive) return;
                auto& ev = static_cast<MouseMoveEvent&>(e);
                if (m_cameraController)
                    m_cameraController->onMouseMoved(ev.getX(), ev.getY());
                });

            // 鼠标滚轮：缩放 / 调整 FOV
            GetEventDispatcher().subscribe(EventType::MouseScrolled, [this](IEvent& e) {
                if (!m_controlActive) return;
                auto& ev = static_cast<MouseScrollEvent&>(e);
                bool ctrlPressed = glfwGetKey(m_window->getHandle(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                    glfwGetKey(m_window->getHandle(), GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
                float delta = static_cast<float>(ev.getYOffset()) * 1.0f;
                if (ctrlPressed) {
                    if (m_cameraController) m_cameraController->setFov(delta);
                }
                else {
                    if (m_cameraController) m_cameraController->onMouseScrolled(ev.getXOffset(), ev.getYOffset());
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