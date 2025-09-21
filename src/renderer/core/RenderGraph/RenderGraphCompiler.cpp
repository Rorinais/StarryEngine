#include "RenderGraphCompiler.hpp"
#include "RenderGraph.hpp"
#include <algorithm>
#include <queue>
#include <stack>
#include <sstream>
#include <iostream>

namespace StarryEngine {
    RenderGraphCompiler::RenderGraphCompiler(VkDevice device, VmaAllocator allocator)
        : mDevice(device), mAllocator(allocator) {
    }

    RenderGraphCompiler::~RenderGraphCompiler() {
    }

    bool RenderGraphCompiler::compile(RenderGraph& graph) {
        mDebugInfo.clear();
        mExecutionOrder.clear();
        mBarriers.clear();
        mAliasGroups.clear();

        // 编译步骤
        if (!validateGraph(graph)) {
            mDebugInfo += "Graph validation failed\n";
            return false;
        }

        if (!computeResourceLifetimes(graph)) {
            mDebugInfo += "Resource lifetime computation failed\n";
            return false;
        }

        if (!topologicalSort(graph)) {
            mDebugInfo += "Topological sort failed\n";
            return false;
        }

        if (!analyzeDependencies(graph)) {
            mDebugInfo += "Dependency analysis failed\n";
            return false;
        }

        if (!generateSynchronization(graph)) {
            mDebugInfo += "Synchronization generation failed\n";
            return false;
        }

        if (!allocateResources(graph)) {
            mDebugInfo += "Resource allocation failed\n";
            return false;
        }

        if (!createVulkanObjects(graph)) {
            mDebugInfo += "Vulkan object creation failed\n";
            return false;
        }

        mDebugInfo += "Compilation successful\n";
        return true;
    }

    bool RenderGraphCompiler::validateGraph(const RenderGraph& graph) const {
        // 检查图是否有环
        if (detectCycles(graph)) {
            mDebugInfo += "Cycle detected in render graph\n";
            return false;
        }

        // 检查所有资源是否被正确声明
        auto& registry = graph.getResourceRegistry();
        for (const auto& pass : graph.getPasses()) {
            for (const auto& usage : pass->getResourceUsages()) {
                if (!registry.getVirtualResource(usage.resource).handle.isValid()) {
                    mDebugInfo += "Invalid resource usage in pass: " + pass->getName() + "\n";
                    return false;
                }
            }
        }

        return true;
    }

    bool RenderGraphCompiler::computeResourceLifetimes(RenderGraph& graph) {
        auto& registry = graph.getResourceRegistry();
        const auto& passes = graph.getPasses();

        // 初始化所有资源的生命周期
        for (uint32_t i = 0; i < registry.getVirtualResourceCount(); ++i) {
            ResourceHandle handle{ i };
            auto& resource = registry.getVirtualResource(handle);
            resource.firstUse = UINT32_MAX;
            resource.lastUse = 0;
        }

        // 计算每个资源的首次和末次使用
        for (uint32_t passIdx = 0; passIdx < passes.size(); ++passIdx) {
            const auto& pass = passes[passIdx];

            for (const auto& usage : pass->getResourceUsages()) {
                auto& resource = registry.getVirtualResource(usage.resource);

                if (passIdx < resource.firstUse) {
                    resource.firstUse = passIdx;
                }
                if (passIdx > resource.lastUse) {
                    resource.lastUse = passIdx;
                }

                // 更新资源状态
                if (usage.isWrite) {
                    resource.finalState.layout = usage.layout;
                    resource.finalState.access = usage.access;
                    resource.finalState.stage = usage.stages;
                }
            }
        }

        return true;
    }

    bool RenderGraphCompiler::topologicalSort(RenderGraph& graph) {
        const auto& passes = graph.getPasses();
        const auto& dependencies = graph.getDependencies();

        // 构建邻接表
        std::vector<std::vector<uint32_t>> adjList(passes.size());
        std::vector<int> inDegree(passes.size(), 0);

        for (const auto& dep : dependencies) {
            adjList[dep.producer.id].push_back(dep.consumer.id);
            inDegree[dep.consumer.id]++;
        }

        // 使用Kahn算法进行拓扑排序
        std::queue<uint32_t> queue;
        for (uint32_t i = 0; i < passes.size(); ++i) {
            if (inDegree[i] == 0) {
                queue.push(i);
            }
        }

        mExecutionOrder.clear();
        while (!queue.empty()) {
            uint32_t current = queue.front();
            queue.pop();
            mExecutionOrder.push_back(RenderPassHandle(current));

            for (uint32_t neighbor : adjList[current]) {
                if (--inDegree[neighbor] == 0) {
                    queue.push(neighbor);
                }
            }
        }

        // 检查是否有环
        if (mExecutionOrder.size() != passes.size()) {
            mDebugInfo += "Graph has cycles, topological sort failed\n";
            return false;
        }

        // 设置每个pass的索引
        for (uint32_t i = 0; i < mExecutionOrder.size(); ++i) {
            passes[mExecutionOrder[i].id]->setIndex(i);
        }

        return true;
    }

    bool RenderGraphCompiler::analyzeDependencies(RenderGraph& graph) {
        // 使用DependencyAnalyzer进行依赖分析
        DependencyAnalyzer analyzer;
        if (!analyzer.analyze(graph.getPasses(), graph.getResourceRegistry().getVirtualResources())) {
            mDebugInfo += "Dependency analysis failed\n";
            return false;
        }

        // 获取分析结果
        mExecutionOrder = analyzer.getExecutionOrder();
        mBarriers = analyzer.getBarriers();

        return true;
    }

    bool RenderGraphCompiler::generateSynchronization(RenderGraph& graph) {
        // 这里已经通过DependencyAnalyzer生成了同步信息
        // 可以添加额外的同步优化逻辑
        return true;
    }

    bool RenderGraphCompiler::allocateResources(RenderGraph& graph) {
        auto& registry = graph.getResourceRegistry();

        // 计算内存需求
        computeMemoryRequirements(graph);

        // 优化资源别名
        if (!optimizeResourceAliasing(graph)) {
            mDebugInfo += "Resource aliasing optimization failed\n";
            return false;
        }

        // 分配实际资源
        if (!registry.allocateActualResources(2)) { // 假设2帧并行
            mDebugInfo += "Resource allocation failed\n";
            return false;
        }

        return true;
    }

    bool RenderGraphCompiler::createVulkanObjects(RenderGraph& graph) {
        const auto& passes = graph.getPasses();

        // 为每个pass创建Vulkan渲染通道和帧缓冲区
        for (const auto& pass : passes) {
            if (!pass->compile()) {
                mDebugInfo += "Failed to compile pass: " + pass->getName() + "\n";
                return false;
            }
        }

        return true;
    }

    bool RenderGraphCompiler::detectCycles(const RenderGraph& graph) const {
        const auto& passes = graph.getPasses();
        const auto& dependencies = graph.getDependencies();

        // 构建邻接表
        std::vector<std::vector<uint32_t>> adjList(passes.size());
        for (const auto& dep : dependencies) {
            adjList[dep.producer.id].push_back(dep.consumer.id);
        }

        // 使用DFS检测环
        std::vector<bool> visited(passes.size(), false);
        std::vector<bool> recStack(passes.size(), false);

        std::function<bool(uint32_t)> hasCycle = [&](uint32_t v) -> bool {
            if (!visited[v]) {
                visited[v] = true;
                recStack[v] = true;

                for (uint32_t neighbor : adjList[v]) {
                    if (!visited[neighbor] && hasCycle(neighbor)) {
                        return true;
                    }
                    else if (recStack[neighbor]) {
                        return true;
                    }
                }
            }
            recStack[v] = false;
            return false;
            };

        for (uint32_t i = 0; i < passes.size(); ++i) {
            if (hasCycle(i)) {
                return true;
            }
        }

        return false;
    }

    void RenderGraphCompiler::computeMemoryRequirements(RenderGraph& graph) {
        auto& registry = graph.getResourceRegistry();

        // 计算每个资源的内存需求
        for (uint32_t i = 0; i < registry.getVirtualResourceCount(); ++i) {
            ResourceHandle handle{ i };
            auto& resource = registry.getVirtualResource(handle);

            if (resource.isImported) {
                continue; // 外部资源不需要分配
            }

            // 根据资源类型计算内存需求
            if (resource.description.type == ResourceType::SampledImage ||
                resource.description.type == ResourceType::ColorAttachment) {

                VkImageCreateInfo imageInfo{};
                imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                imageInfo.imageType = VK_IMAGE_TYPE_2D;
                imageInfo.format = resource.description.format;
                imageInfo.extent = resource.description.extent;
                imageInfo.mipLevels = resource.description.levels;
                imageInfo.arrayLayers = resource.description.layers;
                imageInfo.samples = resource.description.samples;
                imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                imageInfo.usage = resource.description.imageUsage;
                imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

                VkMemoryRequirements memRequirements;
                vkGetImageMemoryRequirements(mDevice, VK_NULL_HANDLE, &memRequirements);

                // 存储内存需求
                resource.description.size = memRequirements.size;
            }
        }
    }

    bool RenderGraphCompiler::optimizeResourceAliasing(RenderGraph& graph) {
        auto& registry = graph.getResourceRegistry();

        // 简单的别名分析：找出生命周期不重叠的资源
        for (uint32_t i = 0; i < registry.getVirtualResourceCount(); ++i) {
            ResourceHandle handle1{ i };
            auto& resource1 = registry.getVirtualResource(handle1);

            if (resource1.isImported || resource1.isTransient) {
                continue;
            }

            for (uint32_t j = i + 1; j < registry.getVirtualResourceCount(); ++j) {
                ResourceHandle handle2{ j };
                auto& resource2 = registry.getVirtualResource(handle2);

                if (resource2.isImported || resource2.isTransient) {
                    continue;
                }

                // 检查生命周期是否重叠
                bool lifetimesOverlap = (resource1.firstUse <= resource2.lastUse) &&
                    (resource2.firstUse <= resource1.lastUse);

                // 检查内存需求是否兼容
                bool memoryCompatible = (resource1.description.size == resource2.description.size) &&
                    (resource1.description.type == resource2.description.type);

                if (!lifetimesOverlap && memoryCompatible) {
                    // 可以别名
                    ResourceAliasGroup group;
                    group.resources = { handle1, handle2 };
                    group.requiredSize = resource1.description.size;
                    group.canAlias = true;

                    mAliasGroups.push_back(group);
                }
            }
        }

        return true;
    }

    void RenderGraphCompiler::dumpCompilationInfo() const {
        std::cout << "Render Graph Compilation Info:" << std::endl;
        std::cout << mDebugInfo << std::endl;

        std::cout << "Execution Order:" << std::endl;
        for (uint32_t i = 0; i < mExecutionOrder.size(); ++i) {
            std::cout << "  " << i << ": Pass " << mExecutionOrder[i].getId() << std::endl;
        }

        std::cout << "Resource Aliasing Groups:" << std::endl;
        for (const auto& group : mAliasGroups) {
            std::cout << "  Group: ";
            for (const auto& resource : group.resources) {
                std::cout << resource.getId() << " ";
            }
            std::cout << "(Size: " << group.requiredSize << " bytes)" << std::endl;
        }
    }
}
