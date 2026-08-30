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
        void shutdown();
        bool isWindowOpen() const;

        void createDescriptorPool();
        void initEventDispatcher();
        void setRenderer(std::shared_ptr<IRenderer> renderer) { m_renderer = renderer; }
        void setScene(std::shared_ptr<Scene::Scene> scene) { m_scene = scene; }

        using UpdateCallback = std::function<void(float deltaTime)>;
        void setUpdateCallback(UpdateCallback cb) { m_updateCallback = std::move(cb); }

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
        uint32_t m_flightFrame = 2;

        std::shared_ptr<Scene::Scene> m_scene;
        std::shared_ptr<IRenderer> m_renderer;
        UpdateCallback m_updateCallback;
        PostRenderCallback m_postRenderCallback;

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        RHI::DescriptorPoolHandle m_descriptorPool;

        std::unique_ptr<CameraController> m_cameraController;
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

        TextEditor m_shaderEditor;
        std::string m_currentShaderPath;
        
        void openShaderFile(const std::string& path);
        void saveCurrentShaderFile();

    };
} // namespace StarryEngine