#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <iomanip>

namespace StarryEngine {
    // 前置声明
    class Device;

    class QueueHandles {
    public:
        // 队列类型枚举
        enum class QueueType {
            Graphics,
            Present,
            Compute,
            Transfer,
            SparseBinding
        };

        // 队列信息结构
        struct QueueInfo {
            QueueType type;
            VkQueue handle = VK_NULL_HANDLE;
            uint32_t familyIndex = 0;
            uint32_t indexInFamily = 0;
            float priority = 1.0f;
            std::string name;
        };

        QueueHandles() = default;

        // 设置关联的设备
        void setDevice(Device* device) { mDevice = device; }
        Device* getDevice() const { return mDevice; }

        // 添加队列
        void addQueue(QueueType type, VkQueue handle, uint32_t familyIndex,
            uint32_t indexInFamily = 0, float priority = 1.0f);

        // 获取队列句柄
        VkQueue getGraphicsQueue() const;
        VkQueue getPresentQueue() const;
        VkQueue getComputeQueue() const;
        VkQueue getTransferQueue() const;

        // 根据类型获取队列
        VkQueue getQueue(QueueType type) const;

        // 获取队列信息
        const QueueInfo* getQueueInfo(QueueType type) const;
        const std::vector<QueueInfo>& getAllQueues() const { return mQueues; }

        // 检查队列是否存在
        bool hasGraphicsQueue() const;
        bool hasPresentQueue() const;
        bool hasComputeQueue() const;
        bool hasTransferQueue() const;
        bool hasQueue(QueueType type) const;

        // 队列等待操作
        void waitIdle(VkQueue queue) const;
        void waitGraphicsIdle() const;
        void waitAllIdle() const;

        // 提交命令缓冲区
        VkResult submit(VkQueue queue,
            const std::vector<VkSubmitInfo>& submits,
            VkFence fence = VK_NULL_HANDLE) const;

        VkResult submitGraphics(const std::vector<VkSubmitInfo>& submits,
            VkFence fence = VK_NULL_HANDLE) const;

        // 呈现操作
        VkResult present(VkPresentInfoKHR& presentInfo) const;

        // 时间戳查询
        VkResult getQueueTimestamp(VkQueue queue,
            uint64_t* timestamp) const;

        // 性能查询 - 简化版本
        uint64_t getQueueCounterValue(QueueType type,
            VkPerformanceCounterScopeKHR scope) const;

        // 队列家族信息
        void setQueueFamilyIndices(const std::vector<uint32_t>& indices) {
            mQueueFamilyIndices = indices;
        }

        const std::vector<uint32_t>& getQueueFamilyIndices() const {
            return mQueueFamilyIndices;
        }

        // 调试信息
        void printQueueInfo() const;

        // 有效性检查
        bool isValid() const;

        // 获取默认队列（用于简化接口）
        VkQueue getDefaultGraphicsQueue() const { return getGraphicsQueue(); }
        VkQueue getDefaultComputeQueue() const { return getComputeQueue(); }

        // 创建队列提交批处理
        class SubmissionBatch {
        public:
            explicit SubmissionBatch(const QueueHandles& queues) : mQueues(queues) {}

            void addGraphicsSubmission(VkCommandBuffer cmdBuffer);
            void addComputeSubmission(VkCommandBuffer cmdBuffer);
            void addCustomSubmission(VkQueue queue, VkCommandBuffer cmdBuffer);

            VkResult submitAll(VkFence fence = VK_NULL_HANDLE);
            VkResult submitAndWait();

        private:
            const QueueHandles& mQueues;
            std::vector<VkSubmitInfo> mGraphicsSubmits;
            std::vector<VkSubmitInfo> mComputeSubmits;
            std::vector<std::pair<VkQueue, std::vector<VkSubmitInfo>>> mCustomSubmits;
        };

        SubmissionBatch createSubmissionBatch() const {
            return SubmissionBatch(*this);
        }

    private:
        std::vector<QueueInfo> mQueues;
        std::vector<uint32_t> mQueueFamilyIndices;
        Device* mDevice = nullptr;
        mutable std::mutex mQueueMutex; // 用于线程安全操作

        // 查找队列信息
        const QueueInfo* findQueueInfo(QueueType type) const;
        QueueInfo* findQueueInfoMutable(QueueType type);

        // 内部辅助函数
        void validateQueue(VkQueue queue, const char* operation) const;
    };

    // 流输出操作符
    std::ostream& operator<<(std::ostream& os, const QueueHandles::QueueType& type);
}