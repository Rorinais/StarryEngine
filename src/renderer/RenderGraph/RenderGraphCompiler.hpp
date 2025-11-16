#pragma once
#include "RenderGraphTypes.hpp"
#include "ResourceSystem.hpp"
#include "RenderGraphAnalyzer.hpp"
#include "RenderPassSystem.hpp"
#include "SynchronizationGenerator.hpp"
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

namespace StarryEngine {

    // 前向声明
    class RenderGraph;

    // 编译结果
    struct CompilationResult {
        bool success = false;
        std::vector<RenderPassHandle> executionOrder;
        std::unordered_map<RenderPassHandle, BarrierBatch> barriers;
        std::vector<ResourceAliasGroup> aliasGroups;
        std::string errorMessage;
        std::string debugInfo;
    };

    // 编译统计信息
    struct CompilationStats {
        uint32_t passCount = 0;
        uint32_t resourceCount = 0;
        uint32_t barrierCount = 0;
        uint32_t aliasGroups = 0;
        size_t estimatedMemory = 0;
    };

    // 渲染图编译器
    class RenderGraphCompiler {
    public:
        RenderGraphCompiler(VkDevice device, VmaAllocator allocator);
        ~RenderGraphCompiler() = default;

        // 主要编译接口
        CompilationResult compile(RenderGraph& graph);

        // 分步编译接口
        CompilationResult compileStepByStep(RenderGraph& graph);

        // 获取编译统计信息
        CompilationStats getStats() const { return mStats; }

        // 获取编译结果
        const std::vector<RenderPassHandle>& getExecutionOrder() const { return mExecutionOrder; }
        const std::unordered_map<RenderPassHandle, BarrierBatch>& getBarriers() const { return mBarriers; }
    private:
        VkDevice mDevice;
        VmaAllocator mAllocator;

        // 子组件
        RenderGraphAnalyzer mAnalyzer;
        SynchronizationGenerator mSyncGenerator;

        // 编译状态
        CompilationStats mStats;
        std::vector<RenderPassHandle> mExecutionOrder;
        std::unordered_map<RenderPassHandle, BarrierBatch> mBarriers;

        // 编译步骤
        CompilationResult validateInput(RenderGraph& graph);
        CompilationResult analyzeGraphStructure(RenderGraph& graph);
        CompilationResult generateSynchronization(RenderGraph& graph, const RenderGraphAnalysisResult& analysis);
        CompilationResult allocateResources(RenderGraph& graph, const RenderGraphAnalysisResult& analysis);
        CompilationResult createVulkanObjects(RenderGraph& graph);

        // 资源优化
        bool optimizeResourceAllocation(RenderGraph& graph, const RenderGraphAnalysisResult& analysis);

        // 调试和验证
        void validateCompilationResult(const CompilationResult& result) const;
        void collectDebugInfo(CompilationResult& result, const std::string& info);
    };

} // namespace StarryEngine