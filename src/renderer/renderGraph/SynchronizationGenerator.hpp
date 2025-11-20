#pragma once
#include "RenderGraphTypes.hpp"
#include "ResourceSystem.hpp"
#include "RenderGraphAnalyzer.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>

namespace StarryEngine {

    // 同步统计信息
    struct SynchronizationStats {
        uint32_t totalBarriers = 0;
        uint32_t imageBarriers = 0;
        uint32_t bufferBarriers = 0;
        uint32_t memoryBarriers = 0;
        uint32_t optimizedBarriers = 0;

        void reset() {
            totalBarriers = 0;
            imageBarriers = 0;
            bufferBarriers = 0;
            memoryBarriers = 0;
            optimizedBarriers = 0;
        }
    };

    // 屏障优化配置
    struct BarrierOptimizationConfig {
        bool enableBarrierBatching = true;
        bool enableLayoutTransitionOptimization = true;
        bool enableQueueFamilyOwnershipOptimization = true;
        uint32_t maxBarriersPerBatch = 100;
    };

    // 资源状态跟踪器
    class ResourceStateTracker {
    public:
        struct TrackedState {
            ResourceState state;
            RenderPassHandle lastWriter;
            std::vector<RenderPassHandle> readers;
        };

        void initialize(const std::vector<VirtualResource>& resources);
        void updateState(ResourceHandle resource, const ResourceState& newState, RenderPassHandle pass);
        const ResourceState& getCurrentState(ResourceHandle resource) const;
        RenderPassHandle getLastWriter(ResourceHandle resource) const;
        bool needsTransition(ResourceHandle resource, const ResourceState& requiredState) const;

    private:
        std::unordered_map<ResourceHandle, TrackedState> mResourceStates;
    };

    // 同步生成器
    class SynchronizationGenerator {
    public:
        SynchronizationGenerator(VkDevice device);

        // 生成同步信息
        std::unordered_map<RenderPassHandle, BarrierBatch> generateSynchronization(
            const RenderGraphAnalysisResult& analysis,
            const std::vector<std::unique_ptr<RenderPass>>& passes,
            const std::vector<VirtualResource>& resources,
            ResourceRegistry& registry);

        // 优化屏障合并
        void optimizeBarriers(std::unordered_map<RenderPassHandle, BarrierBatch>& barriers);

        // 配置优化选项
        void setOptimizationConfig(const BarrierOptimizationConfig& config) {
            mConfig = config;
        }

        // 获取统计信息
        const SynchronizationStats& getStats() const { return mStats; }

        // 调试功能
        void dumpBarrierInfo(const std::unordered_map<RenderPassHandle, BarrierBatch>& barriers) const;

    private:
        VkDevice mDevice;
        BarrierOptimizationConfig mConfig;
        SynchronizationStats mStats;
        ResourceStateTracker mStateTracker;

        // 屏障生成核心方法
        BarrierBatch generateBarriersForPass(
            RenderPassHandle passHandle,
            const RenderPass& pass,
            const std::vector<VirtualResource>& resources,
            ResourceRegistry& registry,
            const RenderGraphAnalysisResult& analysis);

        // 单个屏障创建方法
        VkImageMemoryBarrier createImageBarrier(
            const ActualResource::Image& imageResource,  // 改为接受具体的 Image 结构
            const VirtualResource& virtResource,
            const ResourceState& currentState,
            const ResourceState& requiredState) const;

        VkBufferMemoryBarrier createBufferBarrier(
            const ActualResource::Buffer& bufferResource,  // 改为接受具体的 Buffer 结构
            const VirtualResource& virtResource,
            const ResourceState& currentState,
            const ResourceState& requiredState) const;

        VkMemoryBarrier createMemoryBarrier(
            const ResourceState& currentState,
            const ResourceState& requiredState) const;

        // 状态转换验证和优化
        bool validateStateTransition(const ResourceState& from, const ResourceState& to) const;
        bool isNoOpTransition(const ResourceState& from, const ResourceState& to) const;

        VkPipelineStageFlags determineSrcStageMask(const ResourceState& state) const;
        VkPipelineStageFlags determineDstStageMask(const ResourceState& state) const;

        // 屏障优化方法
        void batchSimilarBarriers(BarrierBatch& batch);
        void removeRedundantBarriers(BarrierBatch& batch);
        void optimizeLayoutTransitions(BarrierBatch& batch);

        // 辅助方法
        VkImageAspectFlags determineAspectMask(VkFormat format) const;
        bool isDepthStencilFormat(VkFormat format) const;
        bool isColorFormat(VkFormat format) const;

        // 调试工具
        std::string resourceStateToString(const ResourceState& state) const;
        std::string imageLayoutToString(VkImageLayout layout) const;
    };
} // namespace StarryEngine