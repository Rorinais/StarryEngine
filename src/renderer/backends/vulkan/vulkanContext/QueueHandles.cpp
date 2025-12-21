#include "QueueHandles.hpp"
#include "Device.hpp" 
#include <stdexcept>

namespace StarryEngine {
    // 添加队列
    void QueueHandles::addQueue(QueueType type, VkQueue handle,
        uint32_t familyIndex, uint32_t indexInFamily,
        float priority) {
        std::lock_guard<std::mutex> lock(mQueueMutex);

        // 检查是否已存在同类型队列
        auto existing = findQueueInfoMutable(type);
        if (existing) {
            std::cerr << "[WARNING] Queue of type " << static_cast<int>(type)
                << " already exists. Replacing." << std::endl;
            existing->handle = handle;
            existing->familyIndex = familyIndex;
            existing->indexInFamily = indexInFamily;
            existing->priority = priority;
            return;
        }

        // 添加新队列
        QueueInfo info;
        info.type = type;
        info.handle = handle;
        info.familyIndex = familyIndex;
        info.indexInFamily = indexInFamily;
        info.priority = priority;

        // 设置队列名称
        switch (type) {
        case QueueType::Graphics:
            info.name = "Graphics";
            break;
        case QueueType::Present:
            info.name = "Present";
            break;
        case QueueType::Compute:
            info.name = "Compute";
            break;
        case QueueType::Transfer:
            info.name = "Transfer";
            break;
        case QueueType::SparseBinding:
            info.name = "SparseBinding";
            break;
        default:
            info.name = "Unknown";
            break;
        }

        mQueues.push_back(info);
    }

    // 获取队列句柄
    VkQueue QueueHandles::getGraphicsQueue() const {
        auto info = findQueueInfo(QueueType::Graphics);
        return info ? info->handle : VK_NULL_HANDLE;
    }

    VkQueue QueueHandles::getPresentQueue() const {
        auto info = findQueueInfo(QueueType::Present);
        return info ? info->handle : VK_NULL_HANDLE;
    }

    VkQueue QueueHandles::getComputeQueue() const {
        auto info = findQueueInfo(QueueType::Compute);
        return info ? info->handle : VK_NULL_HANDLE;
    }

    VkQueue QueueHandles::getTransferQueue() const {
        auto info = findQueueInfo(QueueType::Transfer);
        return info ? info->handle : VK_NULL_HANDLE;
    }

    VkQueue QueueHandles::getQueue(QueueType type) const {
        auto info = findQueueInfo(type);
        return info ? info->handle : VK_NULL_HANDLE;
    }

    // 获取队列信息
    const QueueHandles::QueueInfo* QueueHandles::getQueueInfo(QueueType type) const {
        return findQueueInfo(type);
    }

    // 检查队列是否存在
    bool QueueHandles::hasGraphicsQueue() const {
        return findQueueInfo(QueueType::Graphics) != nullptr;
    }

    bool QueueHandles::hasPresentQueue() const {
        return findQueueInfo(QueueType::Present) != nullptr;
    }

    bool QueueHandles::hasComputeQueue() const {
        return findQueueInfo(QueueType::Compute) != nullptr;
    }

    bool QueueHandles::hasTransferQueue() const {
        return findQueueInfo(QueueType::Transfer) != nullptr;
    }

    bool QueueHandles::hasQueue(QueueType type) const {
        return findQueueInfo(type) != nullptr;
    }

    // 队列等待操作
    void QueueHandles::waitIdle(VkQueue queue) const {
        validateQueue(queue, "waitIdle");
        vkQueueWaitIdle(queue);
    }

    void QueueHandles::waitGraphicsIdle() const {
        if (auto queue = getGraphicsQueue()) {
            vkQueueWaitIdle(queue);
        }
    }

    void QueueHandles::waitAllIdle() const {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        for (const auto& queue : mQueues) {
            if (queue.handle) {
                vkQueueWaitIdle(queue.handle);
            }
        }
    }

    // 提交命令缓冲区
    VkResult QueueHandles::submit(VkQueue queue,
        const std::vector<VkSubmitInfo>& submits,
        VkFence fence) const {
        validateQueue(queue, "submit");

        if (submits.empty()) {
            return VK_SUCCESS;
        }

        return vkQueueSubmit(queue,
            static_cast<uint32_t>(submits.size()),
            submits.data(),
            fence);
    }

    VkResult QueueHandles::submitGraphics(const std::vector<VkSubmitInfo>& submits,
        VkFence fence) const {
        if (auto queue = getGraphicsQueue()) {
            return submit(queue, submits, fence);
        }
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // 呈现操作
    VkResult QueueHandles::present(VkPresentInfoKHR& presentInfo) const {
        if (auto queue = getPresentQueue()) {
            return vkQueuePresentKHR(queue, &presentInfo);
        }
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // 时间戳查询
    VkResult QueueHandles::getQueueTimestamp(VkQueue queue,
        uint64_t* timestamp) const {
        validateQueue(queue, "getQueueTimestamp");

        // 注意：这个方法需要设备支持时间戳查询
        // 在实际实现中，需要使用vkGetDeviceQueueTimestamp
        // 但这是一个设备级别的函数，需要设备支持
        std::cerr << "[WARNING] Queue timestamp query not fully implemented." << std::endl;
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

    // 性能查询 - 简化版本
    uint64_t QueueHandles::getQueueCounterValue(QueueType type,
        VkPerformanceCounterScopeKHR scope) const {
        // 这是一个简化版本，实际实现需要：
        // 1. 检查设备是否支持性能计数器扩展
        // 2. 查询具体的性能计数器
        // 3. 返回相应的值

        std::cerr << "[WARNING] Performance counter query not implemented. "
            << "Requires VK_EXT_performance_query extension." << std::endl;

        // 返回一个占位值
        return 0;
    }

    // 内部查找函数
    const QueueHandles::QueueInfo* QueueHandles::findQueueInfo(QueueType type) const {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        for (const auto& queue : mQueues) {
            if (queue.type == type) {
                return &queue;
            }
        }
        return nullptr;
    }

    QueueHandles::QueueInfo* QueueHandles::findQueueInfoMutable(QueueType type) {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        for (auto& queue : mQueues) {
            if (queue.type == type) {
                return &queue;
            }
        }
        return nullptr;
    }

    // 验证队列有效性
    void QueueHandles::validateQueue(VkQueue queue, const char* operation) const {
        if (!queue) {
            throw std::runtime_error(std::string("Attempted to perform ") +
                operation + " on null queue");
        }
    }

    // 打印队列信息
    void QueueHandles::printQueueInfo() const {
        std::lock_guard<std::mutex> lock(mQueueMutex);

        std::cout << "\n=== Queue Information ===" << std::endl;
        std::cout << std::left << std::setw(15) << "Type"
            << std::setw(20) << "Handle"
            << std::setw(12) << "Family"
            << std::setw(8) << "Index"
            << std::setw(10) << "Priority"
            << std::endl;
        std::cout << std::string(65, '-') << std::endl;

        for (const auto& queue : mQueues) {
            std::cout << std::left << std::setw(15) << queue.name
                << std::setw(20) << reinterpret_cast<void*>(queue.handle)
                << std::setw(12) << queue.familyIndex
                << std::setw(8) << queue.indexInFamily
                << std::setw(10) << queue.priority
                << std::endl;
        }

        std::cout << "\nDevice: " << (mDevice ? "Set" : "Not Set") << std::endl;
        std::cout << "Total Queues: " << mQueues.size() << std::endl;
        std::cout << std::endl;
    }

    // 有效性检查
    bool QueueHandles::isValid() const {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        return !mQueues.empty() && hasGraphicsQueue() && hasPresentQueue();
    }

    // 流输出操作符实现
    std::ostream& operator<<(std::ostream& os, const QueueHandles::QueueType& type) {
        switch (type) {
        case QueueHandles::QueueType::Graphics:
            os << "Graphics";
            break;
        case QueueHandles::QueueType::Present:
            os << "Present";
            break;
        case QueueHandles::QueueType::Compute:
            os << "Compute";
            break;
        case QueueHandles::QueueType::Transfer:
            os << "Transfer";
            break;
        case QueueHandles::QueueType::SparseBinding:
            os << "SparseBinding";
            break;
        default:
            os << "Unknown";
            break;
        }
        return os;
    }

    // SubmissionBatch 方法实现
    void QueueHandles::SubmissionBatch::addGraphicsSubmission(VkCommandBuffer cmdBuffer) {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;
        mGraphicsSubmits.push_back(submitInfo);
    }

    void QueueHandles::SubmissionBatch::addComputeSubmission(VkCommandBuffer cmdBuffer) {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;
        mComputeSubmits.push_back(submitInfo);
    }

    void QueueHandles::SubmissionBatch::addCustomSubmission(VkQueue queue, VkCommandBuffer cmdBuffer) {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        // 查找或创建该队列的提交列表
        auto it = std::find_if(mCustomSubmits.begin(), mCustomSubmits.end(),
            [queue](const auto& pair) { return pair.first == queue; });

        if (it != mCustomSubmits.end()) {
            it->second.push_back(submitInfo);
        }
        else {
            mCustomSubmits.emplace_back(queue, std::vector<VkSubmitInfo>{submitInfo});
        }
    }

    VkResult QueueHandles::SubmissionBatch::submitAll(VkFence fence) {
        VkResult result = VK_SUCCESS;

        // 提交图形队列命令
        if (!mGraphicsSubmits.empty()) {
            if (auto graphicsQueue = mQueues.getGraphicsQueue()) {
                result = vkQueueSubmit(graphicsQueue,
                    static_cast<uint32_t>(mGraphicsSubmits.size()),
                    mGraphicsSubmits.data(),
                    fence);
                if (result != VK_SUCCESS) return result;
            }
        }

        // 提交计算队列命令
        if (!mComputeSubmits.empty()) {
            if (auto computeQueue = mQueues.getComputeQueue()) {
                result = vkQueueSubmit(computeQueue,
                    static_cast<uint32_t>(mComputeSubmits.size()),
                    mComputeSubmits.data(),
                    VK_NULL_HANDLE);
                if (result != VK_SUCCESS) return result;
            }
        }

        // 提交自定义队列命令
        for (const auto& [queue, submits] : mCustomSubmits) {
            if (!submits.empty()) {
                result = vkQueueSubmit(queue,
                    static_cast<uint32_t>(submits.size()),
                    submits.data(),
                    VK_NULL_HANDLE);
                if (result != VK_SUCCESS) return result;
            }
        }

        return result;
    }

    VkResult QueueHandles::SubmissionBatch::submitAndWait() {
        VkResult result = submitAll(VK_NULL_HANDLE);
        if (result == VK_SUCCESS) {
            mQueues.waitAllIdle();
        }
        return result;
    }
}