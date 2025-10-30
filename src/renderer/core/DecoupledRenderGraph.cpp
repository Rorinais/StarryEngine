#include "IRenderGraph.h"
#include "IResourceManager.h"
#include "RenderGraph/RenderGraph.hpp"

namespace StarryEngine {

    class DecoupledRenderGraph : public IRenderGraph {
    public:
        DecoupledRenderGraph() = default;

        bool initialize(VkDevice device, VmaAllocator allocator) override {
            mDevice = device;
            mAllocator = allocator;

            try {
                mInternalGraph = std::make_unique<RenderGraph>(device, allocator);
                return true;
            }
            catch (const std::exception& e) {
                std::cerr << "Failed to initialize RenderGraph: " << e.what() << std::endl;
                return false;
            }
        }

        void shutdown() override {
            mInternalGraph.reset();
        }

        RenderPassHandle addPass(const std::string& name,
            std::function<void(RenderPass&)> setupCallback) override {
            if (mInternalGraph) {
                return mInternalGraph->addPass(name, setupCallback);
            }
            return RenderPassHandle();
        }

        ResourceHandle createResource(const std::string& name,
            const ResourceDescription& desc) override {
            if (mInternalGraph) {
                return mInternalGraph->createResource(name, desc);
            }
            return ResourceHandle();
        }

        bool importResource(ResourceHandle handle, VkImage image,
            VkImageView view, ResourceState initialState) override {
            if (mInternalGraph) {
                return mInternalGraph->importResource(handle, image, view, initialState);
            }
            return false;
        }

        bool compile() override {
            if (mInternalGraph) {
                return mInternalGraph->compile();
            }
            return false;
        }

        void execute(VkCommandBuffer cmd, uint32_t frameIndex) override {
            if (mInternalGraph) {
                mInternalGraph->execute(cmd, frameIndex);
            }
        }

        void setResourceManager(IResourceManager* manager) override {
            mResourceManager = manager;

            // 导入交换链资源（如果可用）
            if (mResourceManager && mInternalGraph) {
                auto swapchainHandle = mResourceManager->getSwapchainResource();
                // 这里需要根据你的资源系统设计来导入交换链图像
            }
        }

        void beginFrame(uint32_t frameIndex) override {
            if (mInternalGraph) {
                mInternalGraph->beginFrame();
            }
        }

        void endFrame(uint32_t frameIndex) override {
            if (mInternalGraph) {
                mInternalGraph->endFrame();
            }
        }

    private:
        VkDevice mDevice = VK_NULL_HANDLE;
        VmaAllocator mAllocator = VK_NULL_HANDLE;
        IResourceManager* mResourceManager = nullptr;
        std::unique_ptr<RenderGraph> mInternalGraph;
    };

} // namespace StarryEngine