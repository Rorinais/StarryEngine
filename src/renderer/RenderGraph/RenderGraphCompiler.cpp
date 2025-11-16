#include "RenderGraphCompiler.hpp"
#include "RenderGraph.hpp"
#include <algorithm>
#include <queue>
#include <stack>
#include <sstream>
#include <iostream>

namespace StarryEngine {

    RenderGraphCompiler::RenderGraphCompiler(VkDevice device, VmaAllocator allocator)
        : mDevice(device), mAllocator(allocator), mSyncGenerator(device) {
    }

    CompilationResult RenderGraphCompiler::compile(RenderGraph& graph) {
        CompilationResult result;

        // 编译流水线
        auto validationResult = validateInput(graph);
        if (!validationResult.success) {
            return validationResult;
        }

        auto analysisResult = analyzeGraphStructure(graph);
        if (!analysisResult.success) {
            return analysisResult;
        }
        // 将分析结果转换为 RenderGraphAnalysisResult
        const RenderGraphAnalysisResult& graphAnalysisResult = mAnalyzer.analyzeGraph(
            graph.getPasses(),
            graph.getResourceRegistry().getAllVirtualResources()
        );

        auto syncResult = generateSynchronization(graph, graphAnalysisResult);
        if (!syncResult.success) {
            return syncResult;
        }

        auto allocationResult = allocateResources(graph, graphAnalysisResult);
        if (!allocationResult.success) {
            return allocationResult;
        }

        auto vulkanResult = createVulkanObjects(graph);
        if (!vulkanResult.success) {
            return vulkanResult;
        }

        result.success = true;
        result.executionOrder = mExecutionOrder;
        result.barriers = mBarriers;
        result.debugInfo = "Compilation completed successfully";
        return result;
    }

    CompilationResult RenderGraphCompiler::compileStepByStep(RenderGraph& graph) {
        // 分步编译实现
        return compile(graph);
    }

    CompilationResult RenderGraphCompiler::validateInput(RenderGraph& graph) {
        CompilationResult result;

        const auto& passes = graph.getPasses();
        const auto& registry = graph.getResourceRegistry();

        // 基本验证
        if (passes.empty()) {
            result.errorMessage = "Render graph has no passes";
            return result;
        }

        // 验证资源引用
        for (const auto& pass : passes) {
            for (const auto& usage : pass->getResourceUsages()) {
                if (!registry.getVirtualResource(usage.resource).handle.isValid()) {
                    result.errorMessage = "Invalid resource reference in pass: " + pass->getName();
                    return result;
                }
            }
        }

        result.success = true;
        return result;
    }

    CompilationResult RenderGraphCompiler::analyzeGraphStructure(RenderGraph& graph) {
        CompilationResult result;

        // 使用GraphAnalyzer进行分析
        auto analysis = mAnalyzer.analyzeGraph(graph.getPasses(),
            graph.getResourceRegistry().getAllVirtualResources());

        if (analysis.hasCycles) {
            result.errorMessage = "Render graph contains cycles";
            return result;
        }

        if (analysis.executionOrder.empty()) {
            result.errorMessage = "Failed to determine execution order";
            return result;
        }

        mExecutionOrder = analysis.executionOrder;
        result.success = true;

        // 更新统计信息
        mStats.passCount = static_cast<uint32_t>(graph.getPasses().size());
        mStats.resourceCount = graph.getResourceRegistry().getVirtualResourceCount();

        return result;
    }

    CompilationResult RenderGraphCompiler::generateSynchronization(RenderGraph& graph,
        const RenderGraphAnalysisResult& analysis) {
        CompilationResult result;

        // 使用SynchronizationGenerator生成同步
        mBarriers = mSyncGenerator.generateSynchronization(
            analysis,
            graph.getPasses(),
            graph.getResourceRegistry().getAllVirtualResources(),
            graph.getResourceRegistry());

        if (mBarriers.empty() && !analysis.executionOrder.empty()) {
            // 如果没有屏障但图不为空，可能有问题
            result.errorMessage = "Failed to generate synchronization";
            return result;
        }

        result.success = true;

        // 更新统计信息
        mStats.barrierCount = 0;
        for (const auto& barrier : mBarriers) {
            mStats.barrierCount += static_cast<uint32_t>(barrier.second.imageBarriers.size() +
                barrier.second.bufferBarriers.size() +
                barrier.second.memoryBarriers.size());
        }

        return result;
    }

    CompilationResult RenderGraphCompiler::allocateResources(RenderGraph& graph,
        const RenderGraphAnalysisResult& analysis) {
        CompilationResult result;

        // 计算资源生命周期
        auto lifetimes = mAnalyzer.computeResourceLifetimes(
            graph.getPasses(),
            graph.getResourceRegistry().getAllVirtualResources());

        // 分析资源别名
        auto aliasGroups = mAnalyzer.analyzeResourceAliasing(
            lifetimes,
            graph.getResourceRegistry().getAllVirtualResources());

        // 分配实际资源
        if (!graph.getResourceRegistry().allocateActualResources(graph.getConcurrentFrame())) { //默认2帧并行
            result.errorMessage = "Failed to allocate resources";
            return result;
        }

        result.aliasGroups = aliasGroups;
        result.success = true;

        // 更新统计信息
        mStats.aliasGroups = static_cast<uint32_t>(aliasGroups.size());

        return result;
    }

    CompilationResult RenderGraphCompiler::createVulkanObjects(RenderGraph& graph) {
        CompilationResult result;

        // 创建Vulkan对象
        for (const auto& pass : graph.getPasses()) {
            if (!pass->compile()) {
                result.errorMessage = "Failed to compile pass: " + pass->getName();
                return result;
            }
        }

        result.success = true;
        return result;
    }

    bool RenderGraphCompiler::optimizeResourceAllocation(RenderGraph& graph,
        const RenderGraphAnalysisResult& analysis) {
        // 资源优化实现
        return true;
    }

    void RenderGraphCompiler::validateCompilationResult(const CompilationResult& result) const {
        // 验证编译结果
    }

    void RenderGraphCompiler::collectDebugInfo(CompilationResult& result, const std::string& info) {
        result.debugInfo += info + "\n";
    }

} // namespace StarryEngine