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
#include "../renderer/graph/renderpass/Renderpass.hpp"
#include "../renderer/subpassRenderer/GbufferRender.hpp"

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

    static constexpr uint32_t SUBPASS_EXTERNAL = ~0U;
    constexpr uint32_t ATTACHMENT_UNUSED = std::numeric_limits<uint32_t>::max();

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

    private:
        void createShaderProgram();
        void createBuffer();
        void createUniformResources();
        void createPipelineLayout();
        void loadTexture();
        void createPostProcessResources(); // 新增后处理资源创建
        void buildRenderGraph();
        void createFramebuffers();

        Window::Ptr m_window;
        uint32_t m_width = 800;
        uint32_t m_height = 600;
        const char* m_title = "StarryEngine";
        const char* m_icon_path = "assets/icons/window_icon.png";
        bool mFramebufferResized = false;
        uint32_t m_FlightFrame = 2;

        std::shared_ptr<VulkanRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;

        // 几何 Pass 资源
        std::shared_ptr<RenderGraph::Geometry> m_geometry;
        std::shared_ptr<RenderGraph::Material> m_material;
        std::shared_ptr<RenderGraph::GBufferRenderer> m_gbufferRenderer;

        // 后处理 Pass 资源
        std::shared_ptr<RenderGraph::PostProcessRenderer> m_postRenderer;
        RHI::ShaderHandle m_postVS;
        RHI::ShaderHandle m_postFS;
        RHI::DescriptorSetLayoutHandle m_postInputLayout;
        RHI::PipelineLayoutHandle m_postPipelineLayout;
        RHI::DescriptorSetHandle m_postDescriptorSet;
        // 中间纹理 ID
        RenderGraph::TextureId m_intermediateTexId;
        RenderGraph::TextureId m_depthTexId;
        RenderGraph::TextureId m_swapchainTexId;

        // RHI 资源句柄
        RHI::DescriptorPoolHandle mDescriptorPoolHandle;
        RHI::PipelineLayoutHandle mPipelineLayoutHandle;
        RHI::RHIBuffer* mUniformBuffer = nullptr;
        RHI::BufferHandle m_uniformBufferHandle;

        // RenderGraph 相关
        std::unique_ptr<RenderGraph::RenderGraph> m_renderGraph;
        std::vector<RHI::FramebufferHandle> m_geomFramebuffers;     
        std::vector<RHI::FramebufferHandle> m_postFramebuffers;   
    };
} // namespace StarryEngine