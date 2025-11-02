#pragma once
#include "IVulkanBackend.hpp"
#include "IRenderGraph.hpp"
#include "IResourceManager.hpp"
#include <memory>

namespace StarryEngine {

    class VulkanRenderer {
    public:
        VulkanRenderer() = default;
        ~VulkanRenderer() { shutdown(); }

        // 初始化，接管各个模块的所有权
        void init(std::unique_ptr<IVulkanBackend> backend,
            std::unique_ptr<IRenderGraph> renderGraph,
            std::unique_ptr<IResourceManager> resourceManager);

        void shutdown();

        // 渲染一帧
        void render();

        // 获取各个模块的指针（用于配置）
        IVulkanBackend* getBackend() const { return mBackend.get(); }
        IRenderGraph* getRenderGraph() const { return mRenderGraph.get(); }
        IResourceManager* getResourceManager() const { return mResourceManager.get(); }

        // 事件处理
        void onSwapchainRecreated();

    private:
        std::unique_ptr<IVulkanBackend> mBackend;
        std::unique_ptr<IRenderGraph> mRenderGraph;
        std::unique_ptr<IResourceManager> mResourceManager;
    };

} // namespace StarryEngine