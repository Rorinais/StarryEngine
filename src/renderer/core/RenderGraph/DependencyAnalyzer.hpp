#pragma once
#include <vector>
#include <unordered_map>
#include "RenderGraphTypes.hpp"
#include "ResourceSystem.hpp"

namespace StarryEngine {
	class DependencyAnalyzer {
	private:
		std::vector<Dependency> mDependencies;
		std::vector<RenderPassHandle> mExecutionOrder;
		std::unordered_map<RenderPassHandle, BarrierBatch> mBarriers;
	public:
		bool analyze(const std::vector<class RenderPass>& passes, const std::vector<VirtualResource>& resources);
		const std::vector<RenderPassHandle>& getExecutionOrder() const { return mExecutionOrder; }
		const std::vector<Dependency>& getDependencies() const { return mDependencies; }
		const std::unordered_map<RenderPassHandle, BarrierBatch>& getBarriers() const { return mBarriers; }

	private:
		void topologicalSort(const std::vector<class RenderPass>& passes);
		void generateSynchronization(const std::vector<VirtualResource>& resource, const std::vector<class RenderPass>& passes);
		void computeResourceStates(const std::vector<VirtualResource>& resources, const std::vector<class RenderPass>& passes);
		static bool needsBarrier(const ResourceState& current, const ResourceState& required);
		static VkImageMemoryBarrier createImageBarrier(const ResourceState& current, const ResourceState& required, VkImageAspectFlags aspect);

	};
}