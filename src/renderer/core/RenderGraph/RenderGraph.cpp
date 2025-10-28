#include "RenderGraph.hpp"
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <iostream>

namespace StarryEngine {

    RenderGraph::RenderGraph(VkDevice device, VmaAllocator allocator)
        : mDevice(device), mAllocator(allocator),
        mResourceRegistry(device, allocator),
        mCompiler(device, allocator) {
        initializeInternalComponents();
    }

    RenderGraph::~RenderGraph() {
        // 清理所有资源
        mResourceRegistry.destroyActualResources();
    }

    void RenderGraph::initializeInternalComponents() {
        // 创建描述符分配器和管线缓存
        mDescriptorAllocator = std::make_unique<DescriptorAllocator>(mDevice);
        mPipelineCache = std::make_unique<PipelineCache>(mDevice);

        // 初始化它们
        if (mDescriptorAllocator) {
            mDescriptorAllocator->initialize();
        }
        if (mPipelineCache) {
            mPipelineCache->initialize();
        }

        // 创建执行器
        mExecutor = std::make_unique<RenderGraphExecutor>(
            mDevice,
            mDescriptorAllocator.get(),
            mPipelineCache.get()
        );
    }

    RenderPassHandle RenderGraph::addPass(const std::string& name, std::function<void(RenderPass&)> setupCallback) {
        auto pass = std::make_unique<RenderPass>();
        pass->setName(name); 

        setupCallback(*pass);

        RenderPassHandle handle;
        handle.setId(static_cast<uint32_t>(mPasses.size()));
        pass->setIndex(handle.getId());
        mPasses.push_back(std::move(pass));

        return handle;
    }

    ResourceHandle RenderGraph::createResource(const std::string& name, const ResourceDescription& desc) {
        return mResourceRegistry.createVirtualResource(name, desc);
    }

    bool RenderGraph::importResource(ResourceHandle handle, VkImage image, VkImageView view, ResourceState initialState) {
        return mResourceRegistry.importResource(handle, image, view, initialState);
    }

    bool RenderGraph::compile() {
        // 使用编译器编译渲染图
        auto result = mCompiler.compile(*this);
        if (!result.success) {
            return false;
        }

        // 初始化执行器
        if (mExecutor && !mExecutor->initialize(mConcurrentFrame)) {
            return false;
        }

        mIsCompiled = true;
        return true;
    }

    void RenderGraph::execute(VkCommandBuffer cmd, uint32_t frameIndex) {
        if (!mIsCompiled) {
            // 如果没有编译，先编译
            if (!compile()) {
                return;
            }
        }

        // 使用执行器执行渲染图
        if (mExecutor) {
            mExecutor->execute(*this, cmd, frameIndex);
        }
    }

    void RenderGraph::beginFrame() {
        if (mExecutor) {
            mExecutor->beginFrame(mCurrentFrame);
        }
    }

    void RenderGraph::endFrame() {
        if (mExecutor) {
            mExecutor->endFrame(mCurrentFrame);
        }
        mCurrentFrame = (mCurrentFrame + 1) % mConcurrentFrame;
    }

    const std::string& RenderGraph::getPassName(RenderPassHandle handle) const {
        static std::string empty;
        if (handle.getId() < mPasses.size()) {
            return mPasses[handle.getId()]->getName();
        }
        return empty;
    }

    void RenderGraph::exportToDot(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return;
        }

        file << "digraph RenderGraph {\n";
        file << "  rankdir=TB;\n";
        file << "  node [shape=rectangle, style=filled, fillcolor=lightblue];\n\n";

        // 添加渲染通道节点
        for (const auto& pass : mPasses) {
            file << "  \"" << pass->getName() << "\" [label=\"" << pass->getName() << "\"];\n";
        }

        file << "\n  // 依赖关系\n";
        for (const auto& dep : mDependencies) {
            const auto& producerName = getPassName(dep.producer);
            const auto& consumerName = getPassName(dep.consumer);
            file << "  \"" << producerName << "\" -> \"" << consumerName << "\";\n";
        }

        file << "}\n";
        file.close();
    }

    void RenderGraph::dumpCompilationInfo() const {
        const auto& stats = mCompiler.getStats();

        std::cout << "=== Render Graph Compilation Info ===\n";
        std::cout << "Pass Count: " << stats.passCount << "\n";
        std::cout << "Resource Count: " << stats.resourceCount << "\n";
        std::cout << "Barrier Count: " << stats.barrierCount << "\n";
        std::cout << "Alias Groups: " << stats.aliasGroups << "\n";
        std::cout << "Estimated Memory: " << stats.estimatedMemory << " bytes\n";
        std::cout << "=====================================\n";
    }

} // namespace StarryEngine