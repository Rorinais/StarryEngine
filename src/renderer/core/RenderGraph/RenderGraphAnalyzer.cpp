#include <algorithm>
#include <queue>
#include <stack>
#include <sstream>
#include <functional>
#include "RenderGraphAnalyzer.hpp"

namespace StarryEngine {

    RenderGraphAnalysisResult RenderGraphAnalyzer::analyzeGraph(
        const std::vector<std::unique_ptr<RenderPass>>& passes,
        const std::vector<VirtualResource>& resources) {

        RenderGraphAnalysisResult result;

        // 1. 构建依赖图
        buildDependencyGraph(passes, resources, result.dependencies);

        // 2. 验证图结构
        if (!validateGraph(result.dependencies, static_cast<uint32_t>(passes.size()))) {
            result.hasCycles = true;
            result.debugInfo = "Graph contains cycles";
            return result;
        }

        // 3. 拓扑排序
        result.executionOrder = topologicalSort(result.dependencies, static_cast<uint32_t>(passes.size()));

        // 4. 查找未使用资源
        for (const auto& resource : resources) {
            if (resource.firstUse == UINT32_MAX && !resource.isImported) {
                result.unusedResources.push_back(resource.handle);
            }
        }

        result.debugInfo = "Graph analysis completed successfully";
        return result;
    }

    std::vector<ResourceLifetime> RenderGraphAnalyzer::computeResourceLifetimes(
        const std::vector<std::unique_ptr<RenderPass>>& passes,
        const std::vector<VirtualResource>& resources) {

        std::vector<ResourceLifetime> lifetimes;
        lifetimes.reserve(resources.size());

        // 初始化生命周期
        for (const auto& resource : resources) {
            ResourceLifetime lifetime;
            lifetime.resource = resource.handle;
            lifetime.firstUse = UINT32_MAX;
            lifetime.lastUse = 0;
            lifetime.isTransient = resource.isTransient;
            lifetimes.push_back(lifetime);
        }

        // 计算每个资源的读写关系
        for (uint32_t passIdx = 0; passIdx < passes.size(); ++passIdx) {
            const auto& pass = passes[passIdx];

            for (const auto& usage : pass->getResourceUsages()) {
                auto& lifetime = lifetimes[usage.resource.getId()];

                // 更新首次和末次使用
                if (passIdx < lifetime.firstUse) lifetime.firstUse = passIdx;
                if (passIdx > lifetime.lastUse) lifetime.lastUse = passIdx;

                // 记录读写关系
                if (usage.isWrite) {
                    lifetime.writers.push_back(RenderPassHandle(passIdx));
                }
                else {
                    lifetime.readers.push_back(RenderPassHandle(passIdx));
                }
            }
        }

        return lifetimes;
    }

    std::vector<ResourceAliasGroup> RenderGraphAnalyzer::analyzeResourceAliasing(
        const std::vector<ResourceLifetime>& lifetimes,
        const std::vector<VirtualResource>& resources) {

        std::vector<ResourceAliasGroup> aliasGroups;

        for (size_t i = 0; i < lifetimes.size(); ++i) {
            const auto& lifetime1 = lifetimes[i];
            const auto& resource1 = resources[lifetime1.resource.getId()];

            if (resource1.isImported || !resource1.isTransient) {
                continue;
            }

            for (size_t j = i + 1; j < lifetimes.size(); ++j) {
                const auto& lifetime2 = lifetimes[j];
                const auto& resource2 = resources[lifetime2.resource.getId()];

                if (resource2.isImported || !resource2.isTransient) {
                    continue;
                }

                // 检查生命周期是否重叠
                bool lifetimesOverlap = (lifetime1.firstUse <= lifetime2.lastUse) &&
                    (lifetime2.firstUse <= lifetime1.lastUse);

                // 检查内存兼容性
                bool memoryCompatible = (resource1.description.size == resource2.description.size) &&
                    (resource1.description.type == resource2.description.type);

                if (!lifetimesOverlap && memoryCompatible) {
                    // 创建别名组或添加到现有组
                    bool foundGroup = false;
                    for (auto& group : aliasGroups) {
                        if (group.canAlias && group.requiredSize == resource1.description.size) {
                            group.resources.push_back(lifetime1.resource);
                            group.resources.push_back(lifetime2.resource);
                            foundGroup = true;
                            break;
                        }
                    }

                    if (!foundGroup) {
                        ResourceAliasGroup newGroup;
                        newGroup.resources = { lifetime1.resource, lifetime2.resource };
                        newGroup.requiredSize = resource1.description.size;
                        newGroup.canAlias = true;
                        aliasGroups.push_back(newGroup);
                    }
                }
            }
        }

        return aliasGroups;
    }

    bool RenderGraphAnalyzer::validateGraph(const std::vector<Dependency>& dependencies, uint32_t passCount) const {
        return !detectCycles(dependencies, passCount);
    }

    bool RenderGraphAnalyzer::detectCycles(const std::vector<Dependency>& dependencies, uint32_t passCount) const {
        // 构建邻接表
        std::vector<std::vector<uint32_t>> adjList(passCount);
        for (const auto& dep : dependencies) {
            adjList[dep.producer.getId()].push_back(dep.consumer.getId());
        }

        // DFS检测环
        std::vector<bool> visited(passCount, false);
        std::vector<bool> recStack(passCount, false);

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

        for (uint32_t i = 0; i < passCount; ++i) {
            if (hasCycle(i)) {
                return true;
            }
        }

        return false;
    }

    std::vector<RenderPassHandle> RenderGraphAnalyzer::topologicalSort(
        const std::vector<Dependency>& dependencies,
        uint32_t passCount) const {

        // 构建邻接表和入度表
        std::vector<std::vector<uint32_t>> adjList(passCount);
        std::vector<int> inDegree(passCount, 0);

        for (const auto& dep : dependencies) {
            adjList[dep.producer.getId()].push_back(dep.consumer.getId());
            inDegree[dep.consumer.getId()]++;
        }

        // Kahn算法拓扑排序
        std::queue<uint32_t> queue;
        for (uint32_t i = 0; i < passCount; ++i) {
            if (inDegree[i] == 0) {
                queue.push(i);
            }
        }

        std::vector<RenderPassHandle> order;
        while (!queue.empty()) {
            uint32_t current = queue.front();
            queue.pop();
            order.push_back(RenderPassHandle(current));

            for (uint32_t neighbor : adjList[current]) {
                if (--inDegree[neighbor] == 0) {
                    queue.push(neighbor);
                }
            }
        }

        // 检查是否有环
        if (order.size() != passCount) {
            return {};
        }

        return order;
    }

    void RenderGraphAnalyzer::buildDependencyGraph(
        const std::vector<std::unique_ptr<RenderPass>>& passes,
        const std::vector<VirtualResource>& resources,
        std::vector<Dependency>& outDependencies) const {

        outDependencies.clear();

        // 为每个资源建立生产者-消费者关系
        for (const auto& resource : resources) {
            if (resource.firstUse == UINT32_MAX) {
                continue;
            }

            // 查找生产者（第一个写入该资源的pass）
            RenderPassHandle producer = RenderPassHandle(UINT32_MAX);
            for (uint32_t i = resource.firstUse; i <= resource.lastUse; ++i) {
                const auto& pass = passes[i];
                for (const auto& usage : pass->getResourceUsages()) {
                    if (usage.resource.getId() == resource.handle.getId() && usage.isWrite) {
                        producer = RenderPassHandle(i);
                        break;
                    }
                }
                if (producer.isValid()) break;
            }

            // 为每个消费者建立依赖
            if (producer.isValid()) {
                for (uint32_t i = resource.firstUse; i <= resource.lastUse; ++i) {
                    if (i != producer.getId()) {
                        Dependency dep;
                        dep.producer = producer;
                        dep.consumer = RenderPassHandle(i);
                        dep.resource = resource.handle;
                        outDependencies.push_back(dep);
                    }
                }
            }
        }
    }

    void RenderGraphAnalyzer::dumpGraphInfo(const RenderGraphAnalysisResult& result) const {
        // 调试信息输出
    }

} // namespace StarryEngine