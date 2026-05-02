#pragma once
#ifdef __linux__
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <dlfcn.h>
#endif

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
    };
} // namespace StarryEngine