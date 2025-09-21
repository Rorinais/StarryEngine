#include <algorithm>
#include <queue>
#include <stack>
#include"DependencyAnalyzer.hpp"
#include "RenderPass.hpp"

namespace StarryEngine{
	DependencyAnalyzer::DependencyAnalyzer(){}

	bool DependencyAnalyzer::analyze(const std::vector<class RenderPass>& passes, const std::vector<VirtualResource>& resources) {
		// 1. 构建依赖图
		for (const auto& resource : resources) {
			if(!resource.isImported&&resource.firstUse==UINT32_MAX)continue;

			// 查找资源的读写关系
			// 假设每个资源只有一个生产者和多个消费者
			RenderPassHandle producer = RenderPassHandle(UINT32_MAX);
			for (uint32_t i = resource.firstUse; i <= resource.lastUse; ++i) {
				const auto& pass = passes[i];
				const auto& usages = pass.getResourceUsages();

				for (const auto& usage :usages) {
					if (usage.resource.getId() == resource.handle.getId() && usage.isWrite) {
						producer = RenderPassHandle(pass.getIndex());	
					}
				}
				if (producer.isValid())break;
			}

			// 为每个消费者添加依赖边
			if (producer.isValid()) {
				for (uint32_t i = resource.firstUse; i <= resource.lastUse; ++i) {
					if (i != producer.getId()) {
						Dependency edge;
						edge.producer = producer;
						edge.consumer = RenderPassHandle(i);
						edge.resource = resource.handle;
						mDependencies.push_back(edge);
						
					}
				
				}
			}
		}
		// 2. 拓扑排序
		topologicalSort(passes);

		// 3. 生成同步
		generateSynchronization(resources, passes);
	}

	void DependencyAnalyzer::topologicalSort(const std::vector<class RenderPass>& passes) {
		//构建邻接表和入度表
		std::vector<std::vector<uint32_t>> adjList(passes.size());
		std::vector<uint32_t> inDegree(passes.size(), 0);

		for(const auto& dep : mDependencies) {
			uint32_t u = dep.producer.getId();
			uint32_t v = dep.consumer.getId();
			adjList[u].push_back(v);
			inDegree[v]++;
		}

		// 使用Kahn算法进行拓扑排序
		std::queue<uint32_t> queue;
		for (uint32_t i = 0; i < passes.size(); i++) {
			if (inDegree[i] == 0) queue.push(i);
		}
		
		mExecutionOrder.clear();
		while (!queue.empty()) {
			uint32_t current = queue.front();
			queue.pop();
			mExecutionOrder.push_back(RenderPassHandle(current));

			for (uint32_t neighbor : adjList[current]) {
				if (--inDegree[neighbor] == 0) queue.push(neighbor);
			}
		}

		// 检测环
		if (mExecutionOrder.size() != passes.size()) {
			// 图中存在环，无法进行拓扑排序
			mExecutionOrder.clear();
			throw std::runtime_error("RenderPass dependency graph has a cycle!");
		}

	}

	void DependencyAnalyzer::generateSynchronization(const std::vector<VirtualResource>& resource, const std::vector<class RenderPass>& passes) {
		// 为每个pass生成屏障
		for (uint32_t i = 0; i < mExecutionOrder.size(); ++i) {
			RenderPassHandle passHandle = mExecutionOrder[i];
			const auto& pass = passes[passHandle.getId()];

			BarrierBatch barriers;

			// 处理每个资源的状态转换
			for (const auto& usage : pass.getResourceUsages()) {
				// 这里需要访问资源注册表来获取资源的当前状态
				// 简化实现：假设所有资源初始状态都是UNDEFINED
				ResourceState currentState;
				currentState.layout = VK_IMAGE_LAYOUT_UNDEFINED;
				currentState.accessMask = 0;
				currentState.stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

				ResourceState requiredState;
				requiredState.layout = usage.layout;
				requiredState.accessMask = usage.accessFlags;
				requiredState.stageMask = usage.stageFlags;

				// 检查是否需要状态转换
				if (currentState.layout != requiredState.layout ||
					currentState.accessMask != requiredState.accessMask ||
					currentState.stageMask != requiredState.stageMask) {

					// 创建图像屏障（简化实现）
					VkImageMemoryBarrier barrier{};
					barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					barrier.oldLayout = currentState.layout;
					barrier.newLayout = requiredState.layout;
					barrier.srcAccessMask = currentState.accessMask;
					barrier.dstAccessMask = requiredState.accessMask;
					barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

					// 注意：这里需要实际的图像句柄，简化实现中省略
					// barrier.image = ...;

					barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					barrier.subresourceRange.baseMipLevel = 0;
					barrier.subresourceRange.levelCount = 1;
					barrier.subresourceRange.baseArrayLayer = 0;
					barrier.subresourceRange.layerCount = 1;

					barriers.imageBarriers.push_back(barrier);
				}
			}

			mBarriers[passHandle] = barriers;
		}
	}

	void DependencyAnalyzer::computeResourceStates(const std::vector<VirtualResource>& resources, const std::vector<class RenderPass>& passes) {

	}

	static bool needsBarrier(const ResourceState& current, const ResourceState& required) {
		return false;
	}

	static VkImageMemoryBarrier createImageBarrier(const ResourceState& current, const ResourceState& required, VkImageAspectFlags aspect) {
		return VkImageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = current.accessMask,
			.dstAccessMask = required.accessMask,
			.oldLayout = current.layout,
			.newLayout = required.layout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = VK_NULL_HANDLE, // To be filled later
			.subresourceRange = {
				.aspectMask = aspect,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};
	}
}