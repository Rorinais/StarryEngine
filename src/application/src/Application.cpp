#include <cstdlib>
#include <application/Application.hpp>
#include <renderer/interface/vulkan/VulkanFactory.hpp>


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
        config.nativeWayland = cfg.nativeWayland;   
        m_window = Window::create(config);

        m_transparent = cfg.transparent;
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
                if (m_scene) {
                    const auto& objects = m_scene->getAllObjects();
                    if (!objects.empty() && objects[0] && !objects[0]->materials.empty() && objects[0]->materials[0]) {
                        auto block = objects[0]->materials[0]->getBlock("LightingUBO");
                        if (block) {
                            block->setVec4("lights.color", glm::vec4(col1[0], col1[1], col1[2], col1[3]));
                        }
                    }
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
            // 注意：窗口最小化时 X11 会把帧缓冲报成 1x1（不是 0）。必须把这种退化尺寸
            // 当作"不可呈现"，否则会把渲染图与交换链反复重建到 1x1 再重建回来 ——
            // 实测这是资源风暴、设备丢失与风暴后卡住的真正来源。
            if (fbW > 1 && fbH > 1 && (static_cast<uint32_t>(fbW) != m_width || static_cast<uint32_t>(fbH) != m_height)) {
                m_width = static_cast<uint32_t>(fbW);
                m_height = static_cast<uint32_t>(fbH);
                m_framebufferResized = true;
                m_lastResizeEventTime = glfwGetTime();
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

    void Application::simulateDeviceLossForTest() {
        m_deviceLost = true;
        if (!recoverFromDeviceLoss()) {
            LOG_ERROR("[Application] (测试) 软重启不可用或失败，受控退出");
            shutdown();
        }
    }

    bool Application::recoverFromDeviceLoss() {
        if (!m_rhi) { LOG_ERROR("[Application] 软重启失败：RHI 为空"); return false; }
        if (!m_worldFactory) {
            LOG_ERROR("[Application] 软重启不可用：上层未注册 world factory");
            return false;
        }
        LOG_WARN("[Application] 设备丢失：开始软重启 RHI + Renderer");

        // 1) 先释放上层世界：Renderer/Scene/材质持有 RHI 句柄，必须先于 RHI 释放
        m_monitor.reset();
        if (m_imguiManager) {
            // 先释放 ImGui 的 Vulkan 后端（它持有 RHI 句柄），再销毁管理器本身
            try { m_imguiManager->shutdownVulkanBackend(); } catch (...) {}
            if (m_resMgr) { try { m_imguiManager->shutdown(m_resMgr.get()); } catch (...) {} }
            m_imguiManager.reset();
        }
        m_renderer.reset();
        m_scene.reset();

        // 2) 销毁 RHI（设备一旦丢失即不可修复，只能整体重建）
        //    VulkanRHI 的析构负责 clear()；接口层没有 clear，直接释放即可。
        m_rhi.reset();
        m_resMgr.reset();

        // 3) 重建 RHI 与描述符池（参数与构造函数严格一致）
        m_rhi = VulkanRHIFactory::createDefault(RHI::API::Vulkan, m_window, m_width, m_height, m_flightFrame, m_transparent);
        if (!m_rhi) { LOG_ERROR("[Application] 软重启失败：RHI 重建失败"); return false; }
        m_resMgr = m_rhi->getResourceManager();
        createDescriptorPool();

        // 4) 让上层用新 RHI 重建 Renderer/Scene/材质/回调（必须在 initialize 之前：
        //    ImGui 后端初始化依赖 renderer 的渲染路径）
        try { m_worldFactory(); }
        catch (const std::exception& e) { LOG_ERROR("[Application] 软重启失败：重建世界异常: {}", e.what()); return false; }

        // 5) 重建 Monitor / ImGui / 尺寸对齐（沿用 initialize 的完整流程）
        try { initialize(); }
        catch (const std::exception& e) { LOG_ERROR("[Application] 软重启失败：initialize 异常: {}", e.what()); return false; }

        m_deviceLost = false;
        m_framebufferResized = true;        // 重新对齐尺寸
        m_lastPresentedFrameTime = glfwGetTime();
        m_skippedFrames = 0;
        m_pausedSince = 0.0;
        LOG_WARN("[Application] 软重启完成，恢复出帧");
        return true;
    }

    bool Application::tryRecreateSwapChain() {
        const auto now = std::chrono::steady_clock::now();
        if (m_lastSwapchainRecreate.time_since_epoch().count() != 0 &&
            (now - m_lastSwapchainRecreate) < m_resizeThrottle) {
            return false;                      // 节流窗口内：不重建，但调用方照常继续渲染
        }
        if (m_width == 0 || m_height == 0) return false;

        m_lastSwapchainRecreate = now;
        m_rhi->waitFramesInFlight();       // 只等 in-flight 帧，不做全设备等待
        if (!m_rhi->recreateSwapChain(m_width, m_height)) {
            LOG_ERROR("Failed to recreate swap chain");
            return false;
        }
        if (m_renderer) m_renderer->onResize(m_width, m_height);
        m_framebufferResized = false;
        return true;
    }

    void Application::step() {
        if (!m_monitor) return;
        if (!isWindowOpen()) return;
        tryBindCameraControl();   // 相机晚到时首帧补绑

        glfwPollEvents();
        m_monitor->tick();

        if (m_window) {
            GLFWwindow* win = m_window->getHandle();
            if (glfwGetWindowAttrib(win, GLFW_ICONIFIED) != 0 || m_width == 0 || m_height == 0) {
                glfwWaitEventsTimeout(0.05);
                return;
            }

            // ── 遮挡/不可呈现时暂停出帧（工业级 resize 目标项 3）──
            // 判据：上一帧是否拿到交换链图像（VulkanRHI 的有限超时负责探测，超时自愈仍作为安全网）。
            // 连续多帧拿不到，说明窗口当前根本无法呈现（被完全遮挡等），此时继续做完整帧
            // 只是白烧 CPU/GPU；改为只泵事件 + 限速探测，拿回图像后立即自动恢复。
            constexpr uint32_t kSkipFramesBeforePause = 5;        // 100ms × 5 ≈ 0.5 秒
            constexpr double   kPresentProbeInterval  = 0.1;      // 暂停期间每 100ms 放行一帧探测
            if (m_rhi->lastFrameSkipped()) {
                if (m_skippedFrames < 1000000u) ++m_skippedFrames;
            } else {
                m_skippedFrames = 0;
            }
            if (m_skippedFrames >= kSkipFramesBeforePause) {
                const double now = glfwGetTime();
                if (m_pausedSince == 0.0) m_pausedSince = now;

                // 卡死升级：暂停超过 1.5 秒仍取不到图，说明不是"暂时不可呈现"（遮挡/最小化），
                // 而是交换链已卡死 —— 继续暂停会永久停在"画面不动"（实测压测后末帧率 2~21 FPS）。
                // 此时清掉节流、强制重建一次交换链，让它重新开始出帧。
                constexpr double kStuckEscalateSeconds = 1.5;
                if (now - m_pausedSince > kStuckEscalateSeconds) {
                    m_pausedSince = 0.0;
                    m_lastSwapchainRecreate = {};
                    if (tryRecreateSwapChain()) {
                        m_skippedFrames = 0;
                        LOG_WARN("[Application] 交换链疑似卡死，已强制重建交换链并恢复出帧");
                    }
                }

                if (now - m_lastPresentProbe < kPresentProbeInterval) {
                    glfwWaitEventsTimeout(0.02);   // 泵事件 + 让出 CPU（保持窗口可响应）
                    return;                        // 本帧不做任何渲染/UI 工作
                }
                m_lastPresentProbe = now;          // 到点：放行一次完整帧，用于探测是否恢复可呈现
            }
            else {
                m_pausedSince = 0.0;               // 恢复正常出帧
            }
        }

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
            // 关键：节流只限制"重建交换链"的频率，绝不再整帧跳过 ——
            // 原来的 `m_framebufferResized = true; return;` 会让拖拽期间帧率塌到 ~5 FPS。
            // 未到重建间隔时照常渲染（合成器会拉伸上一帧，最多持续一个节流周期），
            // 到点后重建一次，尺寸稳定后自然收敛。
            if (m_framebufferResized) {
                if (m_width <= 1 || m_height <= 1) return;   // 退化尺寸：不做任何重建，等恢复可见
                // 实测设备丢失的唯一触发源是"高频尺寸变化导致的交换链重建风暴"
                // （只做最大化/最小化/恢复的对照组 2/2 完全正常）。因此拖拽中不急着重建：
                //  · 尺寸稳定（距最后一次尺寸事件 ≥120ms）→ 立即补一次最终尺寸的重建；
                //  · 仍在拖拽中 → 按更宽的节流间隔（默认 600ms）兜底，避免重建风暴。
                constexpr double kResizeSettleSeconds = 0.12;
                if ((glfwGetTime() - m_lastResizeEventTime) >= kResizeSettleSeconds) {
                    m_lastSwapchainRecreate = {};          // 清除节流，强制重建一次
                }
                if (!tryRecreateSwapChain()) {
                    // 节流窗口内不重建，但也不要对着"已过期"的交换链满速呈现：
                    // 实测拖拽期帧率可达 1000+ FPS（既白烧 CPU/GPU，也更容易把驱动拖到设备丢失）。
                    // 限速到约 120Hz 后继续渲染；画面由合成器拉伸，最多持续一个节流周期。
                    constexpr double kPendingResizeFrameInterval = 1.0 / 120.0;
                    const double pendingNow = glfwGetTime();
                    if (pendingNow - m_lastPendingResizeFrame < kPendingResizeFrameInterval) {
                        glfwWaitEventsTimeout(0.001);
                        return;
                    }
                    m_lastPendingResizeFrame = pendingNow;
                }
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

            // 设备丢失等致命错误目前以异常形式跨 RHI 接口抛出（VulkanFrameContext::beginFrame），
            // 无人捕获会直接 std::terminate → abort（实测随机窗口操作下 6/6 复现）。
            // 工业做法是返回错误状态并走受控恢复；在没有设备重建能力之前，这里至少保证
            // 不崩溃：记录原因后优雅退出，而不是把进程炸掉。
            bool success = false;
            try {
                success = m_rhi->renderFrame([this](RHI::RHICommandEncoder* encoder, uint32_t imageIndex) {
                    m_renderer->renderFrame(encoder, imageIndex, m_clock);
                });
            }
            catch (const std::exception& e) {
                LOG_ERROR("[Application] 渲染帧抛出异常(疑似设备丢失): {}", e.what());
                m_deviceLost = true;
                // 工业级做法：设备丢失不可修复，只能销毁重建。先尝试进程内软重启；
                // 未注册 world factory 或重建失败时，退回受控退出（不崩溃）。
                if (!recoverFromDeviceLoss()) {
                    LOG_ERROR("[Application] 软重启不可用或失败，受控退出");
                    shutdown();
                }
                return;
            }

            if (!m_renderer) return;

            // ── 出帧看门狗（与具体失败模式无关）──
            // 之前的"卡死升级"挂在"跳过帧计数"上，实测在 10 FPS 状态下从未触发（判据与
            // 实际失败模式错开：超时/out-of-date/呈现失败会走不同分支）。改为直接盯"是否出过帧"：
            // 只要超过 1.5 秒没有任何一帧真正成功出图，就清掉节流强制重建交换链。
            if (success && !m_rhi->lastFrameSkipped()) {
                m_lastPresentedFrameTime = glfwGetTime();
            }
            {
                constexpr double kNoPresentEscalateSeconds = 1.5;
                const double nowT = glfwGetTime();
                if (m_lastPresentedFrameTime == 0.0) m_lastPresentedFrameTime = nowT;
                if (nowT - m_lastPresentedFrameTime > kNoPresentEscalateSeconds) {
                    m_lastPresentedFrameTime = nowT;          // 重新计时，避免每帧都重建
                    m_lastSwapchainRecreate = {};             // 清除节流
                    if (tryRecreateSwapChain()) {
                        m_skippedFrames = 0;
                        LOG_WARN("[Application] 超过 {:.1f}s 未成功出帧，已强制重建交换链", kNoPresentEscalateSeconds);
                    } else {
                        LOG_WARN("[Application] 超过 {:.1f}s 未成功出帧，交换链重建未成功", kNoPresentEscalateSeconds);
                    }
                }
            }

            if (!success) {
                // 交换链 out-of-date / 呈现失败：同样走节流重建，避免连续失败时每帧重建
                tryRecreateSwapChain();
                return;
            }

            // 渲染+呈现成功：通知后处理挂钩（帧读回等；本帧 GPU 命令已提交，读回走同队列序）
            // 注意：lastFrameSkipped() 为真时本帧只是"没画"（最小化/遮挡），
            // 读回会重复捕获上一帧内容，故跳过。
            if (m_postRenderCallback && !m_rhi->lastFrameSkipped()) {
                m_postRenderCallback();
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
            if (newWidth <= 1 || newHeight <= 1) {   // 退化尺寸(最小化为 1x1)一律不视为有效尺寸变化
                m_width = newWidth;
                m_height = newHeight;
                m_framebufferResized = true;
                m_lastResizeEventTime = glfwGetTime();
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
            if (!m_scene) return;

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
                    // 开启原始鼠标运动：位移直接取设备增量，不受光标屏幕边界与系统加速影响。
                    // 否则光标被推到屏幕边缘后位置增量归零 —— 表现为"按住右键转不动画面"。
                    if (glfwRawMouseMotionSupported())
                        glfwSetInputMode(m_window->getHandle(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
                    // 仅在确实有相机控制器时才 warp 指针（Wayland 不支持，无控制器时空转也免了）
                    glfwSetCursorPos(m_window->getHandle(), m_width / 2, m_height / 2);
                    glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
            }
            else if (ev.getAction() == GLFW_RELEASE) {
                m_controlActive = false;
                if (glfwRawMouseMotionSupported())
                    glfwSetInputMode(m_window->getHandle(), GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
                glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                if (m_cameraController) m_cameraController->setEnabled(false);
            }
            });

        // 摄像机控制相关订阅改由 tryBindCameraControl() 惰性绑定：
        // 场景或相机晚于 initEventDispatcher 出现时，也能在首帧自动接上。
        tryBindCameraControl();
    }

    void Application::tryBindCameraControl() {
        if (m_cameraControlBound || !m_enableControl) return;
        if (!m_scene || !m_scene->getActiveCamera()) return;
        m_cameraControlBound = true;

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

        LOG_INFO("[app] 摄像机控制已绑定（右键拖动旋转 / WASD 移动 / 滚轮缩放）");
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

        // 注：此前的 "Wayland WSI 修复"（隐藏窗口 + 泵事件）已移除 ——
        // 它声称能把 vkDestroySwapchainKHR 的 ~38 秒阻塞降到 ~25ms，但实测在当前
        // X11 后端并无收益（关闭仍需数十秒才退出），属于无效代码。

        // 提前释放 demo（回调持有它，而它持有 RHI 的最后一份引用）：
        // 让 RHI/交换链的销毁发生在析构体内，顺序可控可观测，而不是拖到成员析构阶段。
        m_updateCallback = nullptr;
        m_postRenderCallback = nullptr;

        if (m_rhi) m_rhi->waitIdle();
        m_rhi.reset();          // 提前释放引擎自身持有的 RHI 引用
    }
}