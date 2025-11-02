#include"DecoupledRenderGraph.hpp"

namespace StarryEngine {

        bool DecoupledRenderGraph::initialize(VkDevice device, VmaAllocator allocator)  {
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

        void DecoupledRenderGraph::shutdown() {
            mInternalGraph.reset();
        }

        RenderPassHandle DecoupledRenderGraph::addPass(const std::string& name,
            std::function<void(RenderPass&)> setupCallback) {
            if (mInternalGraph) {
                return mInternalGraph->addPass(name, setupCallback);
            }
            return RenderPassHandle();
        }

        ResourceHandle DecoupledRenderGraph::createResource(const std::string& name,
            const ResourceDescription& desc)  {
            if (mInternalGraph) {
                return mInternalGraph->createResource(name, desc);
            }
            return ResourceHandle();
        }

        bool DecoupledRenderGraph::importResource(ResourceHandle handle, VkImage image,
            VkImageView view, ResourceState initialState) {
            if (mInternalGraph) {
                return mInternalGraph->importResource(handle, image, view, initialState);
            }
            return false;
        }

        bool DecoupledRenderGraph::compile() {
            if (mInternalGraph) {
                return mInternalGraph->compile();
            }
            return false;
        }

        void DecoupledRenderGraph::execute(VkCommandBuffer cmd, uint32_t frameIndex) {
            if (mInternalGraph) {
                mInternalGraph->execute(cmd, frameIndex);
            }
        }

        void DecoupledRenderGraph::setResourceManager(IResourceManager* manager) {
            mResourceManager = manager;

            // 导入交换链资源（如果可用）
            if (mResourceManager && mInternalGraph) {
                auto swapchainHandle = mResourceManager->getSwapchainResource();
                // 这里需要根据你的资源系统设计来导入交换链图像
            }
        }

        void DecoupledRenderGraph::beginFrame(uint32_t frameIndex) {
            if (mInternalGraph) {
                mInternalGraph->beginFrame();
            }
        }

        void DecoupledRenderGraph::endFrame(uint32_t frameIndex) {
            if (mInternalGraph) {
                mInternalGraph->endFrame();
            }
        }
} // namespace StarryEngine