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
#include "../renderer/backend/RHIFactory.hpp"
#include "../renderer/interface/RHI_TYPES.hpp"
#include "../renderer/graph/RenderGraph.hpp"
#include "../renderer/Renderer.hpp"

namespace StarryEngine {
    class Application {
    public:
        Application();
        ~Application();

        void run();
        void createDescriptorPool();
        void createRenderer();
        
    private:
        Window::Ptr m_window;
        uint32_t m_width = 800*1.5;
        uint32_t m_height = 600*1.5;
        const char* m_title = "StarryEngine";
        const char* m_icon_path = "assets/icons/window_icon.png";
        bool m_framebufferResized = false;
        uint32_t m_flightFrame = 2;

        float m_deltaTime = 0.0f;

        std::shared_ptr<Scene::Scene> m_scene;
        std::unique_ptr<Renderer> m_renderer;

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        RHI::DescriptorPoolHandle m_descriptorPool;

    };
} // namespace StarryEngine