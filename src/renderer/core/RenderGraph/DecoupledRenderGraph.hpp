#include "IRenderGraph.hpp"
#include "../IResourceManager.hpp"
#include "RenderGraph.hpp"

namespace StarryEngine {

    class DecoupledRenderGraph : public IRenderGraph {
    public:
        DecoupledRenderGraph() = default;

        bool initialize(VkDevice device, VmaAllocator allocator) override;

        void shutdown() override;

        RenderPassHandle addPass(const std::string& name,
            std::function<void(RenderPass&)> setupCallback) override;

        ResourceHandle createResource(const std::string& name,
            const ResourceDescription& desc) override;

        bool importResource(ResourceHandle handle, VkImage image,
            VkImageView view, ResourceState initialState) override;

        bool compile() override;

        void execute(VkCommandBuffer cmd, uint32_t frameIndex) override;

        void setResourceManager(IResourceManager* manager) override;

        void beginFrame(uint32_t frameIndex) override;

        void endFrame(uint32_t frameIndex) override;

    private:
        VkDevice mDevice = VK_NULL_HANDLE;
        VmaAllocator mAllocator = VK_NULL_HANDLE;
        IResourceManager* mResourceManager = nullptr;
        std::unique_ptr<RenderGraph> mInternalGraph;
    };

} // namespace StarryEngine