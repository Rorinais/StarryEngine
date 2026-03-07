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
#include "../renderer/subpassRecorder/GbufferRecorder.hpp"
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

    struct Uniforms {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    class Application {
    public:
        Application();
        ~Application();

        void run();
        void createDescriptorPool();
        void createGbuffer();
        void createGrid();
        void createPostBuffer();
        void buildRenderGraph();
        void createImGui();

    private:
        Window::Ptr m_window;
        uint32_t m_width = 800;
        uint32_t m_height = 600;
        const char* m_title = "StarryEngine";
        const char* m_icon_path = "assets/icons/window_icon.png";
        bool mFramebufferResized = false;
        uint32_t m_FlightFrame = 2;

        std::shared_ptr<VulkanRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::unique_ptr<RenderGraph::RenderGraph> m_renderGraph;

        RHI::TextureHandle m_sceneFinalTexHandle;

        std::shared_ptr<RenderGraph::GBufferRecorder> m_gbufferRecorder;
        std::shared_ptr<RenderGraph::GridRecorder> m_gridRecorder;
        std::shared_ptr<RenderGraph::PostProcessRecorder> m_postRecorder;
        std::shared_ptr<RenderGraph::ImGuiRecorder> m_imguiRecorder;

        RHI::DescriptorPoolHandle mDescriptorPoolHandle;
        RHI::PipelineLayoutHandle mPipelineLayoutHandle;
        RHI::BufferHandle m_uniformBufferHandle;
        RHI::BufferHandle m_gridUniformBufferHandle;

        RenderGraph::PassNode* m_imguiPassNode = nullptr;

    };
} // namespace StarryEngine