#pragma once
#ifdef __linux__
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <dlfcn.h>
#endif

#include"../logging/Logger.hpp"
#include"../core/FrameMonitor.hpp"
#include"../event/Events.hpp"
#include"../renderer/backend/RHIFactory.hpp"

#include "../renderer/backend/vulkan/VulkanRHI.hpp"
#include "../renderer/interface/RHI_TYPES.hpp"
#include "../renderer/interface/RHI_STRUCTS_CONFIG.hpp"
#include "../renderer/interface/RHI_STRUCTS_DESC.hpp"
#include "../renderer/graph/RenderGraph.hpp"
#include "../renderer/graph/RenderPassBuilder.hpp"
#include "../renderer/graph/Renderpass/GbufferPass.hpp"
#include "../renderer/graph/Renderpass/PostProcessPass.hpp"
#include "../renderer/graph/Renderpass/ImguiPass.hpp"
#include "../renderer/VertexLayout.hpp"

namespace StarryEngine {
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

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        std::unique_ptr<RenderGraph::RenderGraph> m_renderGraph;

        struct PassInfo {
            std::unique_ptr<RenderGraph::IRenderPass> renderpass;
            RenderGraph::TextureId inputTexture;    // 输入纹理（深度或中间）
            RenderGraph::TextureId outputTexture;   // 输出纹理（颜色）
            RHI::ImageLayout inputInitial;          // 深度/输入初始布局
            RHI::ImageLayout inputFinal;            // 深度/输入最终布局
            RHI::ImageLayout outputInitial;         // 颜色初始布局
            RHI::ImageLayout outputFinal;           // 颜色最终布局

        };
        std::vector<PassInfo> renderpasses;
        RHI::DescriptorPoolHandle mDescriptorPoolHandle;

    };
} // namespace StarryEngine