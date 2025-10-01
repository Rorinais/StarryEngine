#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include "RenderGraphTypes.hpp"
#include "ResourceSystem.hpp"
#include "RenderPass.hpp"

namespace StarryEngine {

    // 前向声明
    class RenderPass;

    // 资源生命周期信息
    struct ResourceLifetime {
        ResourceHandle resource;
        uint32_t firstUse;
        uint32_t lastUse;
        bool isTransient;
        std::vector<RenderPassHandle> readers;
        std::vector<RenderPassHandle> writers;
    };

    // 图分析结果
    struct RenderGraphAnalysisResult {
        std::vector<RenderPassHandle> executionOrder;
        std::vector<Dependency> dependencies;
        std::vector<ResourceHandle> unusedResources;
        bool hasCycles = false;
        std::string debugInfo;
    };

    // 图分析器
    class RenderGraphAnalyzer {
    public:
        RenderGraphAnalyzer() = default;

        // 主要分析接口
        RenderGraphAnalysisResult analyzeGraph(
            const std::vector<std::unique_ptr<RenderPass>>& passes,
            const std::vector<VirtualResource>& resources);

        // 资源生命周期分析
        std::vector<ResourceLifetime> computeResourceLifetimes(
            const std::vector<std::unique_ptr<RenderPass>>& passes,
            const std::vector<VirtualResource>& resources);

        // 别名分析
        std::vector<ResourceAliasGroup> analyzeResourceAliasing(
            const std::vector<ResourceLifetime>& lifetimes,
            const std::vector<VirtualResource>& resources);

        // 验证图结构
        bool validateGraph(const std::vector<Dependency>& dependencies, uint32_t passCount) const;

    private:
        // 图算法实现
        bool detectCycles(const std::vector<Dependency>& dependencies, uint32_t passCount) const;
        std::vector<RenderPassHandle> topologicalSort(const std::vector<Dependency>& dependencies, uint32_t passCount) const;

        void buildDependencyGraph(
            const std::vector<std::unique_ptr<RenderPass>>& passes,
            const std::vector<VirtualResource>& resources,
            std::vector<Dependency>& outDependencies) const;

        // 调试工具
        void dumpGraphInfo(const RenderGraphAnalysisResult& result) const;
    };

} // namespace StarryEngine