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
    class VulkanQueueHandles {
    public:
        enum class QueueType {
            Graphics,
            Present,
            Compute,
            Transfer,
            SparseBinding
        };

        struct QueueInfo {
            QueueType type;
            VkQueue handle = VK_NULL_HANDLE;
            uint32_t familyIndex = 0;
            uint32_t indexInFamily = 0;
            float priority = 1.0f;
            std::string name;
        };

        VulkanQueueHandles() = default;

        void setDevice(void* device) { mDevicePtr = device; }
        void* getDevice() const { return mDevicePtr; }

        void addQueue(QueueType type, VkQueue handle, uint32_t familyIndex,
            uint32_t indexInFamily = 0, float priority = 1.0f);

        VkQueue getGraphicsQueue() const;
        VkQueue getPresentQueue() const;
        VkQueue getComputeQueue() const;
        VkQueue getTransferQueue() const;
        VkQueue getQueue(QueueType type) const;

        const QueueInfo* getQueueInfo(QueueType type) const;
        const std::vector<QueueInfo>& getAllQueues() const { return mQueues; }

        bool hasGraphicsQueue() const;
        bool hasPresentQueue() const;
        bool hasComputeQueue() const;
        bool hasTransferQueue() const;
        bool hasQueue(QueueType type) const;

        void waitIdle(VkQueue queue) const;
        void waitGraphicsIdle() const;
        void waitAllIdle() const;

        VkResult submit(VkQueue queue,
            const std::vector<VkSubmitInfo>& submits,
            VkFence fence = VK_NULL_HANDLE) const;

        VkResult submitGraphics(const std::vector<VkSubmitInfo>& submits,
            VkFence fence = VK_NULL_HANDLE) const;

        VkResult present(VkQueue presentQueue, VkPresentInfoKHR& presentInfo) const;

        VkResult getQueueTimestamp(VkQueue queue,
            uint64_t* timestamp) const;

        uint64_t getQueueCounterValue(QueueType type,
            VkPerformanceCounterScopeKHR scope) const;

        void setQueueFamilyIndices(const std::vector<uint32_t>& indices) {
            mQueueFamilyIndices = indices;
        }

        const std::vector<uint32_t>& getQueueFamilyIndices() const {
            return mQueueFamilyIndices;
        }

        void printQueueInfo() const;

        bool isValid() const;

        VkQueue getDefaultGraphicsQueue() const { return getGraphicsQueue(); }
        VkQueue getDefaultComputeQueue() const { return getComputeQueue(); }

        class SubmissionBatch {
        public:
            explicit SubmissionBatch(const VulkanQueueHandles& queues) : mQueues(queues) {}

            void addGraphicsSubmission(VkCommandBuffer cmdBuffer);
            void addComputeSubmission(VkCommandBuffer cmdBuffer);
            void addCustomSubmission(VkQueue queue, VkCommandBuffer cmdBuffer);

            VkResult submitAll(VkFence fence = VK_NULL_HANDLE);
            VkResult submitAndWait();

        private:
            const VulkanQueueHandles& mQueues;
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
        void* mDevicePtr = nullptr; 
        mutable std::mutex mQueueMutex; 

        const QueueInfo* findQueueInfo(QueueType type) const;
        QueueInfo* findQueueInfoMutable(QueueType type);

        void validateQueue(VkQueue queue, const char* operation) const;
    };

    std::ostream& operator<<(std::ostream& os, const VulkanQueueHandles::QueueType& type);
}