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

#include <event/Events.hpp>
#include <logging/Logger.hpp>
#include <core/FrameMonitor.hpp>
#include <core/Clock.hpp>
#include <renderer/IRenderer.hpp>
#include <renderer/renderPaths/BaseRenderPath.hpp>
#include <renderer/graph/RenderGraph.hpp>
#include <renderer/interface/RHIFactory.hpp>
#include <renderer/interface/RHIEnums.hpp>
#include <scene/camera/CameraController.hpp>
#include <renderer/passExecutor/SceneDrawExecutor.hpp>
#include <renderer/passExecutor/DeferredLightingExecutor.hpp>

#include <ui/ImGuiManager.hpp>
#include <ui/ImGuiExecutor.hpp>
#include <assets/Assets.hpp>

#include <TextEditor.h>
#include <ImGuiFileDialog.h>


namespace StarryEngine {
    class Application {
    public:
        struct Config {
            int posX = -1;
            int posY = -1;

            uint32_t width = 1200;
            uint32_t height = 720;
            const char* title = "StarryEngine";
            const char* iconPath = "assets/icons/window_icon.png";
            bool resizable = true;
            bool transparent = false;  
            bool borderless = false;    
            bool alwaysOnTop = false;  
            bool clickThrough = false; 
            bool highDPI = false;
            bool nativeWayland = false; 
        };

        Application();
        explicit Application(const Config& config);
        ~Application();

        void run();

        void initialize();
        void step();

        bool tryRecreateSwapChain();
        void shutdown();
        bool isWindowOpen() const;

        void createDescriptorPool();
        void initEventDispatcher();
        /// 惰性绑定摄像机控制（场景/相机晚于事件初始化出现时也能生效）
        void tryBindCameraControl();
        void setRenderer(std::shared_ptr<IRenderer> renderer) { m_renderer = renderer; }

        /// 一步注册：渲染器 + 渲染路径，并完成彼此的接线（renderer->setRenderPath）。
        /// 之后 setScene 会自动把场景同步给渲染路径。
        void setRenderer(std::shared_ptr<IRenderer> renderer,
                         std::shared_ptr<BaseRenderPath> path) {
            m_renderer = renderer;
            m_renderPath = path;
            if (m_renderer && m_renderPath) {
                m_renderer->setRenderPath(m_renderPath);
                if (m_scene) m_renderPath->setScene(m_scene.get());
            }
        }

        void setScene(std::shared_ptr<Scene::Scene> scene) {
            m_scene = scene;
            if (m_renderPath && m_scene) m_renderPath->setScene(m_scene.get());
        }

        using UpdateCallback = std::function<void(float deltaTime)>;
        void setUpdateCallback(UpdateCallback cb) { m_updateCallback = std::move(cb); }

        using WorldFactory = std::function<void()>;
        void setWorldFactory(WorldFactory f) { m_worldFactory = std::move(f); }

        bool recoverFromDeviceLoss();

        void simulateDeviceLossForTest();

        using PostRenderCallback = std::function<void()>;
        void setPostRenderCallback(PostRenderCallback cb) { m_postRenderCallback = std::move(cb); }

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
        std::chrono::steady_clock::time_point m_lastSwapchainRecreate{};   ///< 上次重建交换链的时刻
        std::chrono::milliseconds m_resizeThrottle{600};                   ///< 重建节流间隔(默认 600ms)
        bool m_deviceLost = false;                           ///< 本帧渲染抛出致命异常(疑似设备丢失)
        uint32_t m_skippedFrames = 0;                        ///< 连续拿不到交换链图像的帧数(遮挡探测)
        double   m_lastPresentProbe = 0.0;
        double   m_lastPendingResizeFrame = 0.0;
        double   m_lastResizeEventTime = 0.0;
        double   m_pausedSince = 0.0;
        int      m_stuckEscalations = 0;         ///< 遮挡期间重建的指数退避计数
        double   m_lastPresentedFrameTime = 0.0;             
        uint32_t m_flightFrame = 2;

        std::shared_ptr<Scene::Scene> m_scene;
        std::shared_ptr<IRenderer> m_renderer;
        std::shared_ptr<BaseRenderPath> m_renderPath;
        WorldFactory m_worldFactory;                        ///< 设备丢失后重建世界的钩子
        bool         m_transparent = false;                 ///< 创建 RHI 时用的透明标志（软重启需一致）
        UpdateCallback m_updateCallback;
        PostRenderCallback m_postRenderCallback;

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        RHI::DescriptorPoolHandle m_descriptorPool;

        std::unique_ptr<CameraController> m_cameraController;
        bool m_cameraControlBound = false;
        bool m_controlActive = false;
        bool m_enableControl = true;

        Clock m_clock;

        std::unique_ptr<FrameMonitor> m_monitor;

        std::unordered_map<std::string, std::filesystem::file_time_type> m_shaderTimestamps;
        std::chrono::steady_clock::time_point m_lastFileCheck;
        std::chrono::steady_clock::time_point m_nextAllowedReload;
        static constexpr auto kReloadCooldown = std::chrono::milliseconds(1000); // 防抖冷却

        std::unique_ptr<ImGuiManager>                m_imguiManager;
        std::shared_ptr<ImGuiExecutor>               m_imguiExecutor;
        bool m_imguiEnabled = true;
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

        TextEditor m_shaderEditor;
        std::string m_currentShaderPath;
        
        void openShaderFile(const std::string& path);
        void saveCurrentShaderFile();

    };
} // namespace StarryEngine