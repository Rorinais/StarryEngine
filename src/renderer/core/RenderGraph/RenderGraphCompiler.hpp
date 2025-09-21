#pragma once
#include "RenderGraphTypes.hpp"
#include "ResourceSystem.hpp"
#include "DependencyAnalyzer.hpp"
#include "RenderPass.hpp"

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

namespace StarryEngine {
	struct ResourceAliasGroup {
		std::vector<ResourceHandle> resources;
		size_t requiredSize = 0;
		VkMemoryRequirements memRequirements = {};
		bool canAlias = true;
	};

	class RenderGraphCompiler {
	private:
		VmaAllocator mAllocator = VK_NULL_HANDLE;
		VkDevice mDevice = VK_NULL_HANDLE;

		mutable std::string mDebugInfo;

		std::vector<RenderPassHandle> mExecutionOrder;
		std::unordered_map<RenderPassHandle, BarrierBatch> mBarriers;
		std::vector<ResourceAliasGroup> mAliasGroups;
	public:
        RenderGraphCompiler(VkDevice device, VmaAllocator allocator);
        ~RenderGraphCompiler();

        // 编译整个渲染图
        bool compile(class RenderGraph& graph);

        // 获取编译后的执行顺序
        const std::vector<RenderPassHandle>& getExecutionOrder() const {
            return mExecutionOrder;
        }

        // 获取屏障信息
        const std::unordered_map<RenderPassHandle, BarrierBatch>& getBarriers() const {
            return mBarriers;
        }

        // 获取资源别名信息
        const std::vector<ResourceAliasGroup>& getAliasGroups() const {
            return mAliasGroups;
        }

        // 调试功能：输出编译信息
        void dumpCompilationInfo() const;
	private:
        // 编译步骤
        bool validateGraph(const class RenderGraph& graph) const;
        bool computeResourceLifetimes(class RenderGraph& graph);
        bool topologicalSort(class RenderGraph& graph);
        bool analyzeDependencies(class RenderGraph& graph);
        bool generateSynchronization(class RenderGraph& graph);
        bool allocateResources(class RenderGraph& graph);
        bool createVulkanObjects(class RenderGraph& graph);

        // 辅助函数
        bool detectCycles(const class RenderGraph& graph) const;
        void computeMemoryRequirements(class RenderGraph& graph);
        bool optimizeResourceAliasing(class RenderGraph& graph);
	}
}

