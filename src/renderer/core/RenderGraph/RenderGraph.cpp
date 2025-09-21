#include "RenderGraph.hpp"
#include <stdexcept>

namespace StarryEngine {
    RenderGraph::RenderGraph(VkDevice device, VmaAllocator allocator)
        : mDevice(device), mAllocator(allocator),
        mResourceRegistry(device, allocator),
        mCompiler(device, allocator),
        mExecutor(device) {
    }

    RenderGraph::~RenderGraph() {
        // 清理所有资源
        mResourceRegistry.destroyActualResources();
    }

    RenderPassHandle RenderGraph::addPass(const std::string& name,
        std::function<void(RenderPass&)> setupCallback) {
        auto pass = std::make_unique<RenderPass>(name);
        setupCallback(*pass);

        RenderPassHandle handle;
        handle.setId(static_cast<uint32_t>(mPasses.size()));
        mPasses.push_back(std::move(pass));

        return handle;
    }

    ResourceHandle RenderGraph::createResource(const std::string& name,
        const ResourceDescription& desc) {
        return mResourceRegistry.createVirtualResource(name, desc);
    }

    bool RenderGraph::importResource(ResourceHandle handle, VkImage image, VkImageView view,
        ResourceState initialState) {
        return mResourceRegistry.importResource(handle, image, view, initialState);
    }

    bool RenderGraph::compile() {
        // 使用编译器编译渲染图
        if (!mCompiler.compile(*this)) {
            return false;
        }

        // 初始化执行器
        if (!mExecutor.initialize(2)) { // 假设2帧并行
            return false;
        }

        mIsCompiled = true;
        return true;
    }

    void RenderGraph::execute(VkCommandBuffer cmd, uint32_t frameIndex) {
        if (!mIsCompiled) {
            //如果没有编译，先编译
            if (!compile()) {
                return;
            }
        }

        // 使用执行器执行渲染图
        mExecutor.execute(*this, cmd, frameIndex);
    }

    void RenderGraph::beginFrame() {
        mExecutor.beginFrame(mCurrentFrame);
    }

    void RenderGraph::endFrame() {
        mExecutor.endFrame(mCurrentFrame);
        mCurrentFrame = (mCurrentFrame + 1) % 2; // 假设2帧并行
    }
}