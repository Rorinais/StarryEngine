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

        // ════════════════════════════════════════
        // 具体的面板窗口
        //   （必须在 DockSpace End() 之后）
        // ════════════════════════════════════════

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
        poolDesc.maxSets = 50;  
        poolDesc.poolSizes = {
            { RHI::DescriptorType::UniformBuffer,        20 },
            { RHI::DescriptorType::CombinedImageSampler, 30 },
            { RHI::DescriptorType::InputAttachment,      20 },
            { RHI::DescriptorType::StorageBuffer,         5 }  
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
        //initComputePipeline();

        // 确保 swap chain 使用实际帧缓冲尺寸（考虑 scaleToMonitor 缩放）
        {
            int fbW, fbH;
            glfwGetFramebufferSize(m_window->getHandle(), &fbW, &fbH);
            if (fbW > 0 && fbH > 0 && (static_cast<uint32_t>(fbW) != m_width || static_cast<uint32_t>(fbH) != m_height)) {
                m_width = static_cast<uint32_t>(fbW);
                m_height = static_cast<uint32_t>(fbH);
                m_framebufferResized = true;
            }
        }

        while (!glfwWindowShouldClose(m_window->getHandle())) {
            glfwPollEvents();
            monitor.tick();

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

            // 在主循环中
            if (m_imguiManager && m_imguiManager->isInitialized()) {
                m_imguiManager->beginFrame();
                drawImGuiPanels(deltaTime);
                m_imguiManager->endFrame();
            }

            bool success = m_rhi->renderFrame([this, deltaTime](RHI::RHICommandEncoder* encoder, uint32_t imageIndex) {

                //auto* computePipeline = m_resMgr->getPipeline(m_computePipeline);
                //auto* computeLayout = m_resMgr->getPipelineLayout(m_computePipelineLayout);

                //// 绑定计算管线
                //encoder->bindComputePipeline(computePipeline);

                //// 绑定描述符集
                //encoder->bindDescriptorSets(
                //    RHI::PipelineBindPoint::Compute,
                //    computeLayout,
                //    0,                               // firstSet
                //    { m_computeDescriptorSet },      // descriptor sets
                //    {}                               // dynamic offsets
                //);

                //// 派发工作组 (1024 / 256 = 4 个工作组)
                //encoder->dispatch(4, 1, 1);

                //RHI::BufferCopyRegion region{ 0, 0, sizeof(float) * 1024 };
                //encoder->copyBuffer(m_resMgr->getBuffer(m_computeBuffer), m_resMgr->getBuffer(m_computeStaging), { region });

                //// 插入内存屏障：确保计算写入对后续图形阶段可见
                //RHI::BufferBarrier barrier;
                //barrier.buffer = m_computeBuffer;
                //barrier.srcAccessMask = RHI::AccessFlag::ShaderWrite;
                //barrier.dstAccessMask = RHI::AccessFlag::ShaderRead;
                //barrier.offset = 0;
                //barrier.size = VK_WHOLE_SIZE;
                //encoder->pipelineBarrier(
                //    static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::ComputeShader),
                //    static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::VertexShader),   // 假设下一步图形管线读取该 SSBO
                //    RHI::DependencyFlags{},
                //    {},         // memory barriers
                //    { barrier },
                //    {}          // image barriers
                //);

                m_renderer->renderFrame(encoder, imageIndex, deltaTime);
                });

            //auto* stagingBuf = m_resMgr->getBuffer(m_computeStaging);
            //if (stagingBuf) {
            //    void* ptr = stagingBuf->map(0, sizeof(float) * 1024);
            //    if (ptr) {
            //        std::vector<float> result(1024);
            //        memcpy(result.data(), ptr, sizeof(float) * 1024);
            //        stagingBuf->unmap();

            //        // 打印前 10 个元素，避免刷屏
            //        for (int i = 0; i < 10; ++i) {
            //            LOG_INFO("Compute result[{}] = {}", i, result[i]);
            //        }
            //    }
            //    else {
            //        LOG_ERROR("Failed to map staging buffer");
            //    }
            //}

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
            // 直接从 GLFW 获取实际帧缓冲尺寸（比回调参数更准确）
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
        // ✅ 先关闭 ImGui
        if (m_imguiManager) {
            m_imguiManager->shutdown(m_resMgr.get());
        }
        m_imguiManager.reset();
        m_imguiRecorder.reset();

        // 然后 RHI
        if (m_rhi) m_rhi->waitIdle();
        m_rhi.reset();
        m_window.reset();
    }
}