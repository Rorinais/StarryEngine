#pragma once
#ifdef __linux__
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <dlfcn.h>
#endif

#include <filesystem>
#include <chrono>

#include "../event/Events.hpp"
#include "../logging/Logger.hpp"
#include "../core/FrameMonitor.hpp"
#include "../renderer/Renderer.hpp"
#include "../renderer/graph/RenderGraph.hpp"
#include "../renderer/backend/RHIFactory.hpp"
#include "../renderer/interface/RHI_TYPES.hpp"
#include "../scene/camera/CameraController.hpp"
#include "../renderer/subpassRecorder/GbufferRecorder.hpp"
#include "../renderer/subpassRecorder/DeferredLightingRecorder.hpp"

#include "../ui/ImGuiManager.hpp"
#include "../ui/ImGuiRecorder.hpp"

#include "TextEditor.h"
#include <windows.h>
#include <commdlg.h>
#define GLFW_EXPOSE_NATIVE_WIN32   
#include <GLFW/glfw3native.h>

namespace StarryEngine {
    class Application {
    public:
        Application();
        ~Application();

        void run();
        void createDescriptorPool();
        void initEventDispatcher();
        void setRenderer(std::shared_ptr<Renderer> renderer) { m_renderer = renderer; }
        void setScene(std::shared_ptr<Scene::Scene> scene) { m_scene = scene; }

        std::shared_ptr<RHI::IRHI> getRenderHardwareInterface() { return m_rhi; }
        std::shared_ptr<RHI::ResourceManager> getResourceManager() { return m_resMgr; }
        RHI::DescriptorPoolHandle getGlobalDescriptorPool() { return m_descriptorPool; }
        uint32_t getWidth() { return m_width; }
        uint32_t getHeight() { return m_height; }

        ImGuiManager* getImGuiManager() { return m_imguiManager.get(); }
        bool isImGuiEnabled() const { return m_imguiEnabled; }

        std::string OpenFileDialog() {
            OPENFILENAMEA ofn;
            char szFile[260] = { 0 };
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = glfwGetWin32Window(m_window->getHandle());
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = "Shader Files\0*.vert;*.frag;*.comp;*.glsl\0All\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileNameA(&ofn) == TRUE) {
                return std::string(ofn.lpstrFile);
            }
            return "";
        }

    private:
        Window::Ptr m_window;
        uint32_t m_width = 800 * 1.5;
        uint32_t m_height = 600 * 1.5;
        const char* m_title = "StarryEngine";
        const char* m_icon_path = "assets/icons/window_icon.png";
        bool m_framebufferResized = false;
        uint32_t m_flightFrame = 2;

        std::shared_ptr<Scene::Scene> m_scene;
        std::shared_ptr<Renderer> m_renderer;

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        RHI::DescriptorPoolHandle m_descriptorPool;

        std::unique_ptr<CameraController> m_cameraController;
        bool m_controlActive = false;
        bool m_enableControl = true;

        // ---------- 自动 Shader 热重载相关 ----------
        std::unordered_map<std::string, std::filesystem::file_time_type> m_shaderTimestamps;
        std::chrono::steady_clock::time_point m_lastFileCheck;
        std::chrono::steady_clock::time_point m_nextAllowedReload;
        static constexpr auto kReloadCooldown = std::chrono::milliseconds(1000); // 防抖冷却

        std::unique_ptr<ImGuiManager>                m_imguiManager;
        std::shared_ptr<ImGuiRecorder>               m_imguiRecorder;
        bool m_imguiEnabled = true;
        bool m_showDemoWindow = true;
        bool m_showPerformancePanel = true;
        bool m_showSceneGraph = false;
        bool m_showMaterialEditor = false;
        bool m_showDeveloperTools = true;
        void initImGui();
        void drawImGuiPanels(float deltaTime);


        // 代码编辑器相关
        TextEditor m_shaderEditor;
        std::string m_currentShaderPath;
        bool m_showCodeEditor = true;
        // 辅助函数
        void openShaderFile(const std::string& path);
        void saveCurrentShaderFile();
    };
} // namespace StarryEngine