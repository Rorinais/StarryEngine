#pragma once
#include "RenderGraphTypes.hpp"
#include "ResourceSystem.hpp"
#include "RenderPassSystem.hpp"
#include "RenderGraphAnalyzer.hpp"
#include "RenderGraphCompiler.hpp"
#include "RenderGraphExecutor.hpp"
#include <memory>
#include <vector>
#include <functional>

namespace StarryEngine {

    class RenderGraph {
    public:
        RenderGraph(VkDevice device, VmaAllocator allocator);
        ~RenderGraph();

        // 构建接口
        RenderPassHandle addPass(const std::string& name,std::function<void(RenderPass&)> setupCallback);

        // 资源管理
        ResourceHandle createResource(const std::string& name, const ResourceDescription& desc);

        // 导入外部资源
        bool importResource(ResourceHandle handle, VkImage image, VkImageView view,ResourceState initialState = { VK_IMAGE_LAYOUT_UNDEFINED });

        // 编译和执行
        bool compile();
        void execute(VkCommandBuffer cmd, uint32_t frameIndex);

        // 帧管理
        void beginFrame();
        void endFrame();

        // 获取内部组件
        ResourceRegistry& getResourceRegistry() { return mResourceRegistry; }
        const ResourceRegistry& getResourceRegistry() const { return mResourceRegistry; }
        const std::vector<std::unique_ptr<RenderPass>>& getPasses() const { return mPasses; }
        const std::vector<Dependency>& getDependencies() const { return mDependencies; }
        const RenderGraphCompiler& getCompiler() const { return mCompiler; }
		const uint32_t getConcurrentFrame() const { return mConcurrentFrame; }

    private:
        VkDevice mDevice;
        VmaAllocator mAllocator;

        ResourceRegistry mResourceRegistry;
        std::vector<std::unique_ptr<RenderPass>> mPasses;
        std::vector<Dependency> mDependencies;

        RenderGraphCompiler mCompiler;
        RenderGraphExecutor mExecutor;

        // 编译状态
        bool mIsCompiled = false;
        uint32_t mCurrentFrame = 0;
        uint32_t mConcurrentFrame = 2;
    };

} // namespace StarryEngine