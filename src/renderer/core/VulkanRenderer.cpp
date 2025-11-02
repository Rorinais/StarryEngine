#include "VulkanRenderer.hpp"

namespace StarryEngine {

    void VulkanRenderer::init(std::unique_ptr<IVulkanBackend> backend,
        std::unique_ptr<IRenderGraph> renderGraph,
        std::unique_ptr<IResourceManager> resourceManager) {
        mBackend = std::move(backend);
        mRenderGraph = std::move(renderGraph);
        mResourceManager = std::move(resourceManager);

        // 建立模块间的连接
        mBackend->setResourceManager(mResourceManager.get());
        mRenderGraph->setResourceManager(mResourceManager.get());
    }

    void VulkanRenderer::shutdown() {
        // 按依赖顺序关闭（先关闭可能依赖其他模块的）
        mRenderGraph->shutdown();
        mBackend->shutdown();
        mResourceManager->shutdown();
    }

    void VulkanRenderer::render() {
        // 开始帧
        mBackend->beginFrame();

        // 获取命令缓冲区并执行渲染图
        VkCommandBuffer cmd = mBackend->getCommandBuffer();
        uint32_t frameIndex = mBackend->getCurrentFrameIndex();

        // 渲染图帧开始
        mRenderGraph->beginFrame(frameIndex);

        // 执行渲染图
        mRenderGraph->execute(cmd, frameIndex);

        // 渲染图帧结束
        mRenderGraph->endFrame(frameIndex);

        // 提交帧
        mBackend->submitFrame();
    }

    void VulkanRenderer::onSwapchainRecreated() {
        mBackend->onSwapchainRecreated();
    }

} // namespace StarryEngine