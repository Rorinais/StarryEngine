#pragma once
#include "interface/IRenderGraph.hpp"
#include "interface/IVulkanBackend.hpp"
#include "interface/IResourceManager.hpp"
#include <memory>

namespace StarryEngine {
    class VulkanRenderer {
    public:
        VulkanRenderer() = default;
        ~VulkanRenderer() { shutdown(); }

        void init(std::unique_ptr<IVulkanBackend> backend,std::unique_ptr<IRenderGraph> renderGraph,std::unique_ptr<IResourceManager> resourceManager);
        void shutdown();

        void render();
        void onSwapchainRecreated();

        IVulkanBackend* getBackend() const { return mBackend.get(); }
        IRenderGraph* getRenderGraph() const { return mRenderGraph.get(); }
        IResourceManager* getResourceManager() const { return mResourceManager.get(); }

    private:
        std::unique_ptr<IVulkanBackend> mBackend;
        std::unique_ptr<IRenderGraph> mRenderGraph;
        std::unique_ptr<IResourceManager> mResourceManager;
    };

} // namespace StarryEngine