#pragma once
#include "VulkanCore/VulkanCore.hpp"
#include "WindowContext/WindowContext.hpp"
#include "FrameContext/FrameContext.hpp"
#include "RenderGraph/RenderGraph.hpp"

namespace StarryEngine {
    class VulkanRenderer {
    public:
        using Ptr = std::shared_ptr<VulkanRenderer>;
        static Ptr create() {
            return std::make_shared<VulkanRenderer>();
        }

        ~VulkanRenderer();

        void init(VulkanCore::Ptr vulkanCore, WindowContext::Ptr windowContext);
        void shutdown();

        void setRenderGraph(RenderGraph::Ptr renderGraph);
        RenderGraph::Ptr getRenderGraph() const { return mRenderGraph; }

        void beginFrame();
        void renderFrame();
        void endFrame();

        void recreateSwapChain();

        uint32_t getCurrentFrame() const { return mCurrentFrame; }

    private:
        void createSyncObjects();
        void cleanupSyncObjects();

        VulkanCore::Ptr mVulkanCore;
        WindowContext::Ptr mWindowContext;
        RenderGraph::Ptr mRenderGraph;

        std::vector<FrameContext> mFrameContexts;
        uint32_t mCurrentFrame = 0;
        uint32_t mImageIndex = 0;
        bool mFramebufferResized = false;
    };
}
