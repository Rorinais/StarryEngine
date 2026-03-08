#pragma once
#ifdef __linux__
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <dlfcn.h>
#endif

#include "../base.hpp"
#include "Window.hpp"
#include "../renderer/backend/VulkanRHI.hpp"
#include "../renderer/interface/RHI_TYPES.hpp"
#include "../renderer/interface/RHI_STRUCTS_CONFIG.hpp"
#include "../renderer/interface/RHI_STRUCTS_DESC.hpp"
#include "../renderer/graph/RenderGraph.hpp"
#include "../renderer/graph/RenderPassBuilder.hpp"
#include "../renderer/graph/Renderpass/GbufferPass.hpp"
#include "../renderer/graph/Renderpass/PostProcessPass.hpp"
#include "../renderer/VertexLayout.hpp"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

namespace StarryEngine {
    class FrameMonitor {
    public:
        FrameMonitor(Window::Ptr window, std::shared_ptr<FrameContext> frameContext, uint32_t flightFrame);
        void tick();
        void updateTitle();
        float getTime() const;
        float getDeltaTime() const { return m_deltaTime; }
        float getFPS() const { return m_fps; }
    private:
        Window::Ptr m_window;
        std::shared_ptr<FrameContext> m_frameContext;
        uint32_t m_flightFrame;
        std::chrono::high_resolution_clock::time_point m_startTime;
        std::chrono::high_resolution_clock::time_point m_lastFrameTime;
        float m_deltaTime;
        float m_fps;
        uint64_t m_frameCount;
        float m_fpsUpdateInterval;
        float m_lastFPSUpdate;
        double m_lastTitleUpdate;
    };

    class Application {
    public:
        Application();
        ~Application();

        void run();
        void createDescriptorPool();
        void buildRenderGraph();

    private:
        Window::Ptr m_window;
        uint32_t m_width = 800*1.5;
        uint32_t m_height = 600*1.5;
        const char* m_title = "StarryEngine";
        const char* m_icon_path = "assets/icons/window_icon.png";
        bool mFramebufferResized = false;
        uint32_t m_FlightFrame = 2;

        std::shared_ptr<VulkanRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::unique_ptr<RenderGraph::RenderGraph> m_renderGraph;

        struct PassInfo{
            std::unique_ptr<RenderGraph::IRenderPass> renderpass;
            RenderGraph::TextureId inputTexture;
            RenderGraph::TextureId outputTexture;
            RHI::ImageLayout finalLayout;
        };
        std::vector<PassInfo> renderpasses;
        RHI::DescriptorPoolHandle mDescriptorPoolHandle;

    };
} // namespace StarryEngine