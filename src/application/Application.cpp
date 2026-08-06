#include "Application.hpp"


namespace StarryEngine {
    Application::Application() : Application(Config{}) {}

    Application::Application(const Config& cfg) {
        m_width = cfg.width;
        m_height = cfg.height;
        m_title = cfg.title;
        m_icon_path = cfg.iconPath;

        Window::Config config;
        config.posX = cfg.posX;
        config.posY = cfg.posY;
        config.width = m_width;
        config.height = m_height;
        config.title = m_title;
        config.iconPath = m_icon_path;
        config.highDPI = cfg.highDPI;
        config.resizable = cfg.resizable;
        config.fullScreen = false;
        config.transparent = cfg.transparent;
        config.decorated = !cfg.borderless;
        config.floating = cfg.alwaysOnTop;
        config.clickThrough = cfg.clickThrough;
        config.nativeWayland = cfg.nativeWayland;   // 仅 Linux 生效，其他平台忽略
        m_window = Window::create(config);

        m_rhi = VulkanRHIFactory::createDefault(RHI::API::Vulkan, m_window, m_width, m_height, m_flightFrame, cfg.transparent);
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

        m_imguiExecutor = std::make_shared<ImGuiExecutor>(m_imguiManager.get());

        if (m_renderer) {
            m_renderer->addOverlayPass(makeUIOverlay("ImGui", m_imguiExecutor));
            m_renderer->setImGuiManager(m_imguiManager.get(), m_flightFrame);
            m_renderer->setNeedRebuildGraph();
        }

        auto lang = TextEditor::LanguageDefinition::GLSL();
        m_shaderEditor.SetLanguageDefinition(lang);
        openShaderFile("assets/shaders/pbr/shpere_pbr.frag");
    }

    void Application::openShaderFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open file: {}", path);
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        m_shaderEditor.SetText(buffer.str());
        m_currentShaderPath = path;

        LOG_INFO("Opened shader: {}", path);
    }

    void Application::saveCurrentShaderFile() {
        if (m_currentShaderPath.empty()) return;

        std::ofstream file(m_currentShaderPath);
        if (!file.is_open()) {
            LOG_ERROR("Failed to save file: {}", m_currentShaderPath);
            return;
        }

        file << m_shaderEditor.GetText();
        file.close();

        LOG_INFO("Saved shader: {}", m_currentShaderPath);
    }

    void Application::drawImGuiPanels(float deltaTime) {
        ImGuiManager::SetCurrent(m_imguiManager.get());

        static bool layout_initialized = false;

        // ════════════════════════════════════════
        // 主菜单栏
        // ════════════════════════════════════════
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
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Scene View", nullptr, &m_showSceneView);
                ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
                ImGui::MenuItem("Code Editor", nullptr, &m_showCodeEditor);
                ImGui::MenuItem("Console", nullptr, &m_showConsole);
                ImGui::Separator();
                if (ImGui::MenuItem("Reset Layout")) {
                    layout_initialized = false;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Tools")) {
                ImGui::MenuItem("Developer", nullptr, &m_showDeveloperTools);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // ════════════════════════════════════════
        // 全屏 DockSpace
        // ════════════════════════════════════════
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGuiWindowFlags dockspace_flags =
            ImGuiWindowFlags_NoTitleBar |
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

        if (!layout_initialized) {
            layout_initialized = true;

            // 清除默认布局
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

            ImGuiID dock_right;
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.30f, &dock_right, &dockspace_id);

            ImGuiID dock_right_up, dock_right_down;
            ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Up, 0.20f, &dock_right_up, &dock_right_down);

            ImGuiID dock_bottom;
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.30f, &dock_bottom, &dockspace_id);

            ImGui::DockBuilderDockWindow("Scene View", dockspace_id);  // 左侧主区域
            ImGui::DockBuilderDockWindow("Inspector", dock_right_up);  // 右上方
            ImGui::DockBuilderDockWindow("Code Editor", dock_right_down); // 右下方
            ImGui::DockBuilderDockWindow("Console", dock_bottom);   // 底部

            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::End(); // MainDockSpace

        // Scene View — 渲染画面
        if (m_showSceneView && ImGui::Begin("Scene View", &m_showSceneView)) {
            ImTextureID texID = m_imguiManager->getSceneTextureID();
            if (texID) {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                ImGui::Image(texID, avail);
            }
            ImGui::End();
        }

        // Inspector
        if (m_showInspector && ImGui::Begin("Inspector", &m_showInspector)) {
            ImGui::Text("Object Properties");

            static float col1[4] = { 1.0f, 0.0f, 0.2f ,1.0f};

            if (ImGui::ColorEdit4("color 2", col1)) {
                 auto block = m_scene->getAllObjects()[0]->materials[0]->getBlock("LightingUBO");
                if (block)
                {
                    block->setVec4("lights.color", glm::vec4(col1[0], col1[1], col1[2], col1[3]));
                }

            }
            ImGui::End();
        }

        // Code Editor
        if (m_showCodeEditor && ImGui::Begin("Code Editor", &m_showCodeEditor)) {
            if (ImGui::Button("Open")) {
                IGFD::FileDialogConfig config;
                config.path = ".";
                config.countSelectionMax = 1;
                config.flags = ImGuiFileDialogFlags_None;
                ImGuiFileDialog::Instance()->OpenDialog(
                    "ChooseShaderFile",
                    "Open Shader File",
                    ".vert,.frag,.comp,.glsl,.h,.c,.cpp",
                    config
                );
            }
            ImGui::SameLine();
            if (ImGui::Button("Save")) {
                saveCurrentShaderFile();
            }
            ImGui::SameLine();
            ImGui::Text(" %s", m_currentShaderPath.c_str());

            ImGui::Separator();

            if (ImGuiFileDialog::Instance()->Display("ChooseShaderFile")) {
                if (ImGuiFileDialog::Instance()->IsOk()) {
                    std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
                    openShaderFile(filePath);
                }
                ImGuiFileDialog::Instance()->Close();
            }

            m_shaderEditor.Render("##editor", ImGui::GetContentRegionAvail());
            ImGui::End();
        }


        // Console
        if (m_showConsole && ImGui::Begin("Console", &m_showConsole)) {
            auto sink = StarryEngine::Logger::getImGuiSink();
            if (!sink) {
                ImGui::Text("Logger not available.");
            }
            else {
                if (ImGui::Button("Clear")) sink->clear();

                ImGui::BeginChild("LogRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

                auto logs = sink->getLogs();
                for (const auto& entry : logs) {
                    ImVec4 color;
                    switch (entry.level) {
                    case spdlog::level::trace:    color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); break;
                    case spdlog::level::debug:    color = ImVec4(0.5f, 0.5f, 1.0f, 1.0f); break;
                    case spdlog::level::info:     color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break;
                    case spdlog::level::warn:     color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); break;
                    case spdlog::level::err:      color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); break;
                    case spdlog::level::critical: color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); break;
                    default:                      color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); break;
                    }
                    ImGui::TextColored(color, "%s", entry.message.c_str());
                }

                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    ImGui::SetScrollHereY(1.0f);

                ImGui::PopStyleVar();
                ImGui::EndChild();
            }
            ImGui::End();
        }

        //if (m_showDemoWindow) {
        //    ImGui::ShowDemoWindow(&m_showDemoWindow);
        //}
    }

    void Application::createDescriptorPool() {
        m_resMgr = m_rhi->getResourceManager();

        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 200;  
        poolDesc.poolSizes = {
            { RHI::DescriptorType::UniformBuffer,        80 },
            { RHI::DescriptorType::CombinedImageSampler, 120 },
            { RHI::DescriptorType::InputAttachment,      40 },
            { RHI::DescriptorType::StorageBuffer,         10 }  
        };
        poolDesc.freeDescriptorSet = true;
        poolDesc.debugName = "GlobalDescriptorPool";
        m_descriptorPool = m_resMgr->createDescriptorPool(poolDesc);
    }

    void Application::run() {
        initialize();
        while (isWindowOpen()) {
            step();
        }
    }

    void Application::initialize() {
        auto frameContext = m_rhi->getFrameContext();
        m_monitor = std::make_unique<FrameMonitor>(m_window, frameContext, m_flightFrame);

        // 初始化 shader 文件时间戳
        for (const auto& entry : std::filesystem::recursive_directory_iterator("assets/shaders")) {
            if (entry.is_regular_file())
                m_shaderTimestamps[entry.path().string()] = std::filesystem::last_write_time(entry);
        }
        m_lastFileCheck = std::chrono::steady_clock::now();
        m_nextAllowedReload = m_lastFileCheck;

        initImGui();

        {
            int fbW, fbH;
            glfwGetFramebufferSize(m_window->getHandle(), &fbW, &fbH);
            if (fbW > 0 && fbH > 0 && (static_cast<uint32_t>(fbW) != m_width || static_cast<uint32_t>(fbH) != m_height)) {
                m_width = static_cast<uint32_t>(fbW);
                m_height = static_cast<uint32_t>(fbH);
                m_framebufferResized = true;
            }
        }

        // 初始帧缓冲尺寸与交换链不一致时（如 HiDPI）同步重建，避免依赖实时节奏的 200ms 防抖
        // （否则 Python 步进等高频驱动会一直卡在防抖里，交换链永不重建）
        if (m_framebufferResized && m_width > 0 && m_height > 0) {
            if (m_rhi->recreateSwapChain(m_width, m_height)) {
                if (m_renderer) m_renderer->onResize(m_width, m_height);
                m_framebufferResized = false;
            }
        }
    }

    void Application::step() {
        if (!m_monitor) return;
        if (!isWindowOpen()) return;

        glfwPollEvents();
        m_monitor->tick();

            auto now = std::chrono::steady_clock::now();
            if (now - m_lastFileCheck > std::chrono::milliseconds(500)) {
                m_lastFileCheck = now;
                bool anyChange = false;

                std::error_code ec;
                if (std::filesystem::exists("assets/shaders", ec)) {
                    try {
                        for (const auto& entry : std::filesystem::recursive_directory_iterator("assets/shaders", ec)) {
                            if (ec) break;
                            if (!entry.is_regular_file()) continue;
                            std::string path = entry.path().string();
                            auto currentTime = std::filesystem::last_write_time(entry, ec);
                            if (ec) continue;
                            auto it = m_shaderTimestamps.find(path);
                            if (it == m_shaderTimestamps.end() || currentTime > it->second) {
                                anyChange = true;
                                m_shaderTimestamps[path] = currentTime;
                            }
                        }
                    }
                    catch (const std::exception& e) {
                        LOG_ERROR("Shader file watch error: {}", e.what());
                    }
                }

                if (anyChange && now >= m_nextAllowedReload && m_renderer) {
                    m_rhi->waitIdle();
                    m_renderer->reloadAllShaders();
                    m_nextAllowedReload = now + kReloadCooldown;
                }
            }

            // 窗口尺寸变化处理
            if (m_framebufferResized) {
                static auto lastResize = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                if (now - lastResize < std::chrono::milliseconds(200)) {
                    m_framebufferResized = true; return;
                }
                lastResize = now;
                m_rhi->waitIdle();
                m_framebufferResized = false;
                if (m_width == 0 || m_height == 0) return;

                if (!m_rhi->recreateSwapChain(m_width, m_height)) {
                    std::cerr << "Failed to recreate swap chain!" << std::endl;
                    return;
                }

                m_renderer->onResize(m_width, m_height);
                return;
            }
            if (m_width == 0 || m_height == 0) return;

            // 每帧逻辑
            float deltaTime = m_monitor->getDeltaTime();
            m_clock.advance(deltaTime);          // 推进全局时钟（动画/时间源）
            if (m_cameraController) {
                m_cameraController->update(deltaTime);
            }
            if (m_scene) {
                m_scene->update(m_clock);        // 场景逻辑（动画等）
            }
            if (m_updateCallback) {
                m_updateCallback(deltaTime);     // demo 自定义每帧逻辑（如骨骼动画上传）
            }

            // 在主循环中
            if (m_imguiManager && m_imguiManager->isInitialized()) {
                m_imguiManager->beginFrame();
                drawImGuiPanels(deltaTime);
                m_imguiManager->endFrame();
            }

            bool success = m_rhi->renderFrame([this](RHI::RHICommandEncoder* encoder, uint32_t imageIndex) {
                m_renderer->renderFrame(encoder, imageIndex, m_clock);
            });

            if (!success) {
                m_rhi->waitIdle();
                if (m_rhi->recreateSwapChain(m_width, m_height)) {
                    m_renderer->onResize(m_width, m_height);
                } else {
                    LOG_ERROR("Failed to recreate swap chain after renderFrame");
                }
                return;
            }

            m_monitor->updateTitle();

            // 执行延迟销毁（资源释放队列）
            m_resMgr->tickFrame();
    }

    void Application::shutdown() {
        if (m_window) {
            glfwSetWindowShouldClose(m_window->getHandle(), GLFW_TRUE);
        }
    }

    bool Application::isWindowOpen() const {
        return m_window && !glfwWindowShouldClose(m_window->getHandle());
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
            // 直接从 GLFW 获取实际帧缓冲尺寸
            int newWidth, newHeight;
            glfwGetFramebufferSize(m_window->getHandle(), &newWidth, &newHeight);
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
            }
            });

        // 鼠标右键按住：进入/退出摄像机控制（比 Ctrl 更不易误触）
        GetEventDispatcher().subscribe(EventType::MouseButtonPressed, [this](IEvent& e) {
            auto& ev = static_cast<MouseButtonEvent&>(e);
            if (ev.getButton() != GLFW_MOUSE_BUTTON_RIGHT) return;

            if (ev.getAction() == GLFW_PRESS) {
                m_controlActive = true;
                // 先启用 + 重置鼠标基准，再移到中心禁用 ——
                // 这样 glfwSetCursorPos 触发的第一帧会以中心为基准，视角不跳变
                if (m_cameraController) {
                    m_cameraController->setEnabled(true);
                    m_cameraController->resetMouse();
                    // 仅在确实有相机控制器时才 warp 指针（Wayland 不支持，无控制器时空转也免了）
                    glfwSetCursorPos(m_window->getHandle(), m_width / 2, m_height / 2);
                    glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
            }
            else if (ev.getAction() == GLFW_RELEASE) {
                m_controlActive = false;
                glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                if (m_cameraController) m_cameraController->setEnabled(false);
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
        if (m_imguiManager) {
            m_imguiManager->shutdown(m_resMgr.get());
        }
        m_imguiManager.reset();
        m_imguiExecutor.reset();

        // 析构顺序必须保证：RHI 在窗口(wayland 连接)之前销毁。
        // Renderer/Scene/Monitor/更新回调(捕获 demo) 都持有 RHI 资源的引用；
        // 释放 RHI 最后引用发生在成员析构阶段，此时 GLFW 若已 terminate，
        // 原生 Wayland + NVIDIA 下 vkDestroySwapchainKHR 会在死连接上 marshal 而段错误。
        // 因此绝不能在这里 reset m_window —— m_window 是第一个成员、最后析构，
        // 它活着，wayland 连接就活着，RHI 无论何时销毁都是安全的。
        m_scene.reset();
        m_renderer.reset();
        m_monitor.reset();

        if (m_rhi) m_rhi->waitIdle();
        m_rhi.reset();          // 提前释放引擎自身持有的 RHI 引用
    }
}