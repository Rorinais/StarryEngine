#pragma once
#include "VulkanCore/VulkanCore.hpp"
#include "WindowContext/WindowContext.hpp"
#include "FrameContext/FrameContext.hpp"
#include "RenderGraph/RenderGraph.hpp"  // 包含新的RenderGraph头文件

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

        void setRenderGraph(std::shared_ptr<RenderGraph> renderGraph);  
        std::shared_ptr<RenderGraph> getRenderGraph() const { return mRenderGraph; }

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
        std::shared_ptr<RenderGraph> mRenderGraph;  

        std::vector<FrameContext> mFrameContexts;
        uint32_t mCurrentFrame = 0;
        uint32_t mImageIndex = 0;
        bool mFramebufferResized = false;
    };
};


//#pragma once
//#include "IVulkanBackend.h"
//#include "IRenderGraph.h"
//#include "IResourceManager.h"
//#include <memory>
//
//namespace StarryEngine {
//
//    class VulkanRenderer {
//    public:
//        VulkanRenderer() = default;
//        ~VulkanRenderer() { shutdown(); }
//
//        // 初始化，接管各个模块的所有权
//        void init(std::unique_ptr<IVulkanBackend> backend,
//            std::unique_ptr<IRenderGraph> renderGraph,
//            std::unique_ptr<IResourceManager> resourceManager);
//
//        void shutdown();
//
//        // 渲染一帧
//        void render();
//
//        // 获取各个模块的指针（用于配置）
//        IVulkanBackend* getBackend() const { return mBackend.get(); }
//        IRenderGraph* getRenderGraph() const { return mRenderGraph.get(); }
//        IResourceManager* getResourceManager() const { return mResourceManager.get(); }
//
//        // 事件处理
//        void onSwapchainRecreated();
//
//    private:
//        std::unique_ptr<IVulkanBackend> mBackend;
//        std::unique_ptr<IRenderGraph> mRenderGraph;
//        std::unique_ptr<IResourceManager> mResourceManager;
//    };
//
//} // namespace StarryEngine