#pragma once
#ifdef __linux__
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <dlfcn.h>
#endif

#ifdef _WIN32
    #include <windows.h>
    #include <commdlg.h>
    #include <GLFW/glfw3.h>
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
#endif

#include <filesystem>
#include <chrono>
#include <functional>

#include "../event/Events.hpp"
#include "../logging/Logger.hpp"
#include "../core/FrameMonitor.hpp"
#include "../core/Clock.hpp"
#include "../renderer/Renderer.hpp"
#include "../renderer/graph/RenderGraph.hpp"
#include "../renderer/backend/RHIFactory.hpp"
#include "../renderer/interface/RHI_TYPES.hpp"
#include "../scene/camera/CameraController.hpp"
#include "../renderer/passExecutor/MeshDrawExecutor.hpp"
#include "../renderer/passExecutor/DeferredLightingExecutor.hpp"

#include "../ui/ImGuiManager.hpp"
#include "../ui/ImGuiExecutor.hpp"
#include "../assets/Assets.hpp"

#include "TextEditor.h"
#include "ImGuiFileDialog.h"  


namespace StarryEngine {
    class Application {
    public:
        // 启动配置：窗口尺寸 / 透明窗口（桌面角色）等
        struct Config {
            uint32_t width = 1200;
            uint32_t height = 720;
            const char* title = "StarryEngine";
            const char* iconPath = "assets/icons/window_icon.png";
            bool resizable = true;
            bool transparent = false;   // 透明窗口（需 swapchain alpha 合成）
            bool borderless = false;    // 无边框
            bool alwaysOnTop = false;   // 置顶
            bool clickThrough = false;  // 鼠标穿透（透明区不挡点击）
            bool highDPI = false;
            bool nativeWayland = false; // Wayland 会话下走原生 Wayland（透明窗口需要，XWayland 不支持 alpha 合成）
        };

        Application();
        explicit Application(const Config& config);
        ~Application();

        void run();

        // 分步运行接口：initialize() + 循环 step()，供 Python 绑定逐帧驱动引擎
        void initialize();
        void step();
        void shutdown();
        bool isWindowOpen() const;

        void createDescriptorPool();
        void initEventDispatcher();
        void setRenderer(std::shared_ptr<Renderer> renderer) { m_renderer = renderer; }
        void setScene(std::shared_ptr<Scene::Scene> scene) { m_scene = scene; }

        // 每帧逻辑回调（demo 挂载动画/自定义更新）
        using UpdateCallback = std::function<void(float deltaTime)>;
        void setUpdateCallback(UpdateCallback cb) { m_updateCallback = std::move(cb); }

        std::shared_ptr<RHI::IRHI> getRenderHardwareInterface() { return m_rhi; }
        std::shared_ptr<RHI::ResourceManager> getResourceManager() { return m_resMgr; }
        RHI::DescriptorPoolHandle getGlobalDescriptorPool() { return m_descriptorPool; }
        Window::Ptr getWindow() { return m_window; }
        uint32_t getWidth() { return m_width; }
        uint32_t getHeight() { return m_height; }

        ImGuiManager* getImGuiManager() { return m_imguiManager.get(); }
        bool isImGuiEnabled() const { return m_imguiEnabled; }

    private:
        Window::Ptr m_window;
        uint32_t m_width = 1200;
        uint32_t m_height = 720;
        const char* m_title = "StarryEngine";
        const char* m_icon_path = "assets/icons/window_icon.png";
        bool m_framebufferResized = false;
        uint32_t m_flightFrame = 2;

        std::shared_ptr<Scene::Scene> m_scene;
        std::shared_ptr<Renderer> m_renderer;
        UpdateCallback m_updateCallback;

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        RHI::DescriptorPoolHandle m_descriptorPool;

        std::unique_ptr<CameraController> m_cameraController;
        bool m_controlActive = false;
        bool m_enableControl = true;

        // 全局游戏时钟：动画、shader time、场景逻辑的统一时间源
        Clock m_clock;

        // 帧监控器（initialize() 中创建，step() 中推进）
        std::unique_ptr<FrameMonitor> m_monitor;

        // ---------- 自动 Shader 热重载相关 ----------
        std::unordered_map<std::string, std::filesystem::file_time_type> m_shaderTimestamps;
        std::chrono::steady_clock::time_point m_lastFileCheck;
        std::chrono::steady_clock::time_point m_nextAllowedReload;
        static constexpr auto kReloadCooldown = std::chrono::milliseconds(1000); // 防抖冷却

        std::unique_ptr<ImGuiManager>                m_imguiManager;
        std::shared_ptr<ImGuiExecutor>               m_imguiExecutor;
        bool m_imguiEnabled = false;
        bool m_showDemoWindow = true;
        bool m_showPerformancePanel = true;
        bool m_showSceneGraph = false;
        bool m_showMaterialEditor = false;
        bool m_showDeveloperTools = true;
        bool m_showSceneView = true;
        bool m_showInspector = true;
        bool m_showCodeEditor = true;
        bool m_showConsole = true;
        void initImGui();
        void drawImGuiPanels(float deltaTime);


        // 代码编辑器相关
        TextEditor m_shaderEditor;
        std::string m_currentShaderPath;
        // 辅助函数
        void openShaderFile(const std::string& path);
        void saveCurrentShaderFile();

    };
} // namespace StarryEngine