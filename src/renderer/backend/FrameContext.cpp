#include "FrameContext.hpp"
#include "Device.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <stdexcept>

namespace StarryEngine {

    namespace {
        // 高精度时间获取
        uint64_t getCurrentTimeNanoseconds() {
            auto now = std::chrono::high_resolution_clock::now();
            auto duration = now.time_since_epoch();
            return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        }

        // 转换为毫秒
        float nanosecondsToMilliseconds(uint64_t ns) {
            return static_cast<float>(ns) / 1'000'000.0f;
        }
    }

    FrameContext::FrameContext(std::shared_ptr<Device> device, const Config& config)
        : mDevice(device), mConfig(config) {

        if (!mDevice) {
            throw std::runtime_error("Device must be valid");
        }
    }

    FrameContext::~FrameContext() {
        cleanup();
    }

    bool FrameContext::initialize(uint32_t graphicsQueueFamilyIndex) {
        // 创建主命令池
        try {
            mMainCommandPool = mDevice->createCommandPool(
                graphicsQueueFamilyIndex,
                mConfig.commandPoolFlags
            );
        }
        catch (const std::runtime_error& e) {
            std::cerr << "[ERROR] Failed to create command pool: " << e.what() << std::endl;
            return false;
        }

        // 创建同步对象和命令缓冲区
        if (!createSyncObjects()) {
            return false;
        }

        if (!createCommandBuffers()) {
            return false;
        }

        // 获取时间戳周期
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(mDevice->getPhysicalDevice(), &properties);
        mTimestampPeriod = properties.limits.timestampPeriod;

        // 创建时间戳查询（如果需要）
        if (mConfig.enableTimestamps) {
            mTimestampsEnabled = createTimestampQueries();
        }

        return true;
    }

    void FrameContext::cleanup() {
        if (!mDevice) {
            return;
        }

        mDevice->waitIdle();

        cleanupFrameData();

        if (mMainCommandPool != VK_NULL_HANDLE) {
            mDevice->destroyCommandPool(mMainCommandPool);
            mMainCommandPool = VK_NULL_HANDLE;
        }
    }

    // ==================== 帧循环接口 ====================

    FrameContext::FrameInfo FrameContext::beginFrame(const AcquireImageFunc& acquireFunc) {
        if (mFrameInProgress) {
            throw std::runtime_error("Frame already in progress");
        }

        FrameInfo frameInfo;
        frameInfo.frameIndex = mCurrentFrameIndex;

        // 获取当前帧数据
        FrameData& frameData = mFrameData[mCurrentFrameIndex];

        // 等待上一帧完成
        if (!waitForFrame(mCurrentFrameIndex)) {
            throw std::runtime_error("Failed to wait for previous frame");
        }

        // 重置栅栏
        resetFrame(mCurrentFrameIndex);

        // 获取图像（通过回调）
        uint32_t imageIndex = 0;
        VkResult acquireResult = acquireFunc(frameData.imageAvailableSemaphore,
            VK_NULL_HANDLE,
            imageIndex);

        frameInfo.acquireResult = acquireResult;

        // 处理获取结果
        if (!handleAcquireResult(acquireResult, frameInfo)) {
            return frameInfo;
        }

        frameInfo.imageIndex = imageIndex;
        frameInfo.commandBuffer = frameData.commandBuffer;
        frameInfo.imageAvailableSemaphore = frameData.imageAvailableSemaphore;
        frameInfo.renderFinishedSemaphore = frameData.renderFinishedSemaphore;
        frameInfo.inFlightFence = frameData.inFlightFence;

        // 重置命令缓冲区
        VkCommandBufferResetFlags flags = mConfig.commandBufferResetFlags;
        vkResetCommandBuffer(frameData.commandBuffer, flags);

        // 开始记录命令缓冲区
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(frameData.commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer");
        }

        // 开始时间戳查询
        if (mTimestampsEnabled) {
            beginTimestampQuery(frameInfo);
        }

        // 记录CPU开始时间
        frameInfo.cpuBeginTime = getCurrentTimeNanoseconds();

        mFrameInProgress = true;

        return frameInfo;
    }

    void FrameContext::endFrame(FrameInfo& frameInfo) {
        if (!mFrameInProgress) {
            return;
        }

        // 验证帧信息
        validateFrameInfo(frameInfo);

        // 结束时间戳查询
        if (mTimestampsEnabled) {
            endTimestampQuery(frameInfo);
        }

        // 结束命令缓冲区记录
        if (vkEndCommandBuffer(frameInfo.commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to end recording command buffer");
        }

        // 记录CPU结束时间
        frameInfo.cpuEndTime = getCurrentTimeNanoseconds();

        mFrameInProgress = false;
    }

    VkResult FrameContext::submitFrame(FrameInfo& frameInfo,
        VkQueue graphicsQueue,
        const PresentImageFunc& presentFunc) {

        if (!graphicsQueue) {
            throw std::runtime_error("Invalid graphics queue");
        }

        // 如果不需要重建，检查是否应该跳过提交
        if (frameInfo.needsRecreate) {
            return VK_ERROR_OUT_OF_DATE_KHR;
        }

        // 等待信号量：图像可用
        VkSemaphore waitSemaphores[] = { frameInfo.imageAvailableSemaphore };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        // 信号信号量：渲染完成
        VkSemaphore signalSemaphores[] = { frameInfo.renderFinishedSemaphore };

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &frameInfo.commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        // 提交命令缓冲区
        VkResult submitResult = vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameInfo.inFlightFence);

        if (submitResult != VK_SUCCESS) {
            std::cerr << "[ERROR] Failed to submit command buffer: " << submitResult << std::endl;
            return submitResult;
        }

        // 呈现图像（通过回调）
        VkResult presentResult = presentFunc(graphicsQueue, frameInfo.imageIndex, frameInfo.renderFinishedSemaphore);
        frameInfo.presentResult = presentResult;

        // 处理呈现结果
        handlePresentResult(presentResult, frameInfo);

        // 如果启用了自动重建并且需要重建，尝试重建
        if (mConfig.autoRecreate && frameInfo.needsRecreate && mRecreateCallback) {
            // 注意：这里需要知道宽度和高度，通常需要从其他地方获取
            // 在实际应用中，可能需要传递这些参数或从其他地方获取
            std::cout << "[INFO] Auto-recreating swap chain..." << std::endl;
        }

        // 更新帧索引
        mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mConfig.frameCount;

        // 更新统计信息
        updateStatistics(frameInfo);

        return presentResult;
    }

    FrameContext::FrameInfo FrameContext::processFrame(const AcquireImageFunc& acquireFunc,
        VkQueue graphicsQueue,
        const PresentImageFunc& presentFunc) {

        FrameInfo frameInfo = beginFrame(acquireFunc);

        // 检查是否需要重建
        if (frameInfo.needsRecreate) {
            return frameInfo;
        }

        endFrame(frameInfo);
        VkResult presentResult = submitFrame(frameInfo, graphicsQueue, presentFunc);
        frameInfo.presentResult = presentResult;

        return frameInfo;
    }

    // ==================== 重建处理 ====================

    bool FrameContext::recreateSwapChain(uint32_t width, uint32_t height) {
        if (!mRecreateCallback) {
            std::cerr << "[ERROR] No recreate callback set" << std::endl;
            return false;
        }

        if (mRecreateAttempts >= mConfig.maxRecreateAttempts) {
            std::cerr << "[ERROR] Max recreate attempts reached: " << mRecreateAttempts << std::endl;
            return false;
        }

        mRecreateAttempts++;

        std::cout << "[INFO] Recreating swap chain (attempt " << mRecreateAttempts
            << " of " << mConfig.maxRecreateAttempts << ")" << std::endl;

        // 等待设备空闲
        mDevice->waitIdle();

        // 调用重建回调
        bool success = mRecreateCallback(width, height);

        if (success) {
            mNeedsRecreate = false;
            mRecreateAttempts = 0;
            std::cout << "[INFO] Swap chain recreated successfully" << std::endl;
        }
        else {
            std::cerr << "[ERROR] Failed to recreate swap chain" << std::endl;
        }

        return success;
    }

    void FrameContext::handleRecreateResult(bool success) {
        if (success) {
            mNeedsRecreate = false;
            mRecreateAttempts = 0;
        }
        else {
            mNeedsRecreate = true;  // 标记为需要继续重建
        }
    }

    // ==================== 重建处理辅助函数 ====================

    bool FrameContext::handleAcquireResult(VkResult result, FrameInfo& frameInfo) {
        switch (result) {
        case VK_SUCCESS:
            return true;

        case VK_SUBOPTIMAL_KHR:
            std::cout << "[INFO] Swap chain is suboptimal (acquire)" << std::endl;
            frameInfo.needsRecreate = true;
            mNeedsRecreate = true;
            return true;  // 继续使用，但标记需要重建

        case VK_ERROR_OUT_OF_DATE_KHR:
            notifyRecreateNeeded(frameInfo, "Swap chain out of date (acquire)");
            return false;

        case VK_ERROR_SURFACE_LOST_KHR:
            notifyRecreateNeeded(frameInfo, "Surface lost (acquire)");
            return false;

        case VK_TIMEOUT:
            std::cerr << "[WARNING] Timeout while acquiring swap chain image" << std::endl;
            return false;

        case VK_NOT_READY:
            std::cerr << "[WARNING] Swap chain not ready" << std::endl;
            return false;

        default:
            std::cerr << "[ERROR] Failed to acquire swap chain image: " << result << std::endl;
            return false;
        }
    }

    bool FrameContext::handlePresentResult(VkResult result, FrameInfo& frameInfo) {
        switch (result) {
        case VK_SUCCESS:
            return true;

        case VK_SUBOPTIMAL_KHR:
            std::cout << "[INFO] Swap chain is suboptimal (present)" << std::endl;
            frameInfo.needsRecreate = true;
            mNeedsRecreate = true;
            return true;  // 继续运行，但标记需要重建

        case VK_ERROR_OUT_OF_DATE_KHR:
            notifyRecreateNeeded(frameInfo, "Swap chain out of date (present)");
            return false;

        case VK_ERROR_SURFACE_LOST_KHR:
            notifyRecreateNeeded(frameInfo, "Surface lost (present)");
            return false;

        default:
            std::cerr << "[ERROR] Failed to present swap chain image: " << result << std::endl;
            return false;
        }
    }

    bool FrameContext::shouldRecreate(VkResult result) const {
        return result == VK_ERROR_OUT_OF_DATE_KHR ||
            result == VK_ERROR_SURFACE_LOST_KHR ||
            result == VK_SUBOPTIMAL_KHR;
    }

    void FrameContext::notifyRecreateNeeded(FrameInfo& frameInfo, const std::string& reason) {
        std::cout << "[INFO] " << reason << std::endl;
        frameInfo.needsRecreate = true;
        frameInfo.imageIndex = UINT32_MAX;  // 标记无效图像索引
        mNeedsRecreate = true;
    }

    // ==================== 资源访问 ====================

    FrameContext::FrameInfo FrameContext::getCurrentFrameInfo() const {
        FrameInfo frameInfo;
        frameInfo.frameIndex = mCurrentFrameIndex;

        if (mCurrentFrameIndex < mFrameData.size()) {
            const FrameData& frameData = mFrameData[mCurrentFrameIndex];
            frameInfo.commandBuffer = frameData.commandBuffer;
            frameInfo.imageAvailableSemaphore = frameData.imageAvailableSemaphore;
            frameInfo.renderFinishedSemaphore = frameData.renderFinishedSemaphore;
            frameInfo.inFlightFence = frameData.inFlightFence;
        }

        return frameInfo;
    }

    VkCommandBuffer FrameContext::getCurrentCommandBuffer() const {
        if (mCurrentFrameIndex < mFrameData.size()) {
            return mFrameData[mCurrentFrameIndex].commandBuffer;
        }
        return VK_NULL_HANDLE;
    }

    // ==================== 状态查询 ====================

    bool FrameContext::waitForFrame(uint32_t frameIndex, uint64_t timeout) {
        if (frameIndex >= mFrameData.size()) {
            return false;
        }

        VkFence fence = mFrameData[frameIndex].inFlightFence;
        if (fence == VK_NULL_HANDLE) {
            return true;
        }

        VkResult result = vkWaitForFences(mDevice->getLogicalDevice(), 1, &fence, VK_TRUE, timeout);
        return result == VK_SUCCESS;
    }

    void FrameContext::resetFrame(uint32_t frameIndex) {
        if (frameIndex >= mFrameData.size()) {
            return;
        }

        VkFence fence = mFrameData[frameIndex].inFlightFence;
        if (fence != VK_NULL_HANDLE) {
            vkResetFences(mDevice->getLogicalDevice(), 1, &fence);
        }
    }

    // ==================== 多线程支持 ====================

    VkCommandBuffer FrameContext::allocateThreadCommandBuffer(uint32_t threadIndex, VkCommandBufferLevel level) {
        if (threadIndex >= mFrameData.size()) {
            return VK_NULL_HANDLE;
        }

        FrameData& frameData = mFrameData[threadIndex];

        // 创建线程命令池（如果不存在）
        if (frameData.threadCommandPool == VK_NULL_HANDLE) {
            createThreadCommandPool(frameData);
        }

        try {
            return mDevice->allocateCommandBuffer(frameData.threadCommandPool, level);
        }
        catch (const std::runtime_error& e) {
            std::cerr << "[ERROR] Failed to allocate thread command buffer: " << e.what() << std::endl;
            return VK_NULL_HANDLE;
        }
    }

    void FrameContext::freeThreadCommandBuffers(uint32_t threadIndex) {
        if (threadIndex >= mFrameData.size()) {
            return;
        }

        FrameData& frameData = mFrameData[threadIndex];
        if (frameData.threadCommandPool != VK_NULL_HANDLE) {
            // 注意：这里应该重置命令池而不是销毁，以便重用
            vkResetCommandPool(mDevice->getLogicalDevice(),
                frameData.threadCommandPool,
                VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
        }
    }

    VkCommandPool FrameContext::getThreadCommandPool(uint32_t threadIndex) {
        if (threadIndex < mFrameData.size()) {
            return mFrameData[threadIndex].threadCommandPool;
        }
        return VK_NULL_HANDLE;
    }

    // ==================== 时间戳查询 ====================

    void FrameContext::enableTimestamps(bool enable) {
        if (enable && !mTimestampsEnabled) {
            mTimestampsEnabled = createTimestampQueries();
        }
        else if (!enable && mTimestampsEnabled) {
            for (auto& frameData : mFrameData) {
                if (frameData.timestampQueryPool != VK_NULL_HANDLE) {
                    vkDestroyQueryPool(mDevice->getLogicalDevice(),
                        frameData.timestampQueryPool,
                        nullptr);
                    frameData.timestampQueryPool = VK_NULL_HANDLE;
                }
            }
            mTimestampsEnabled = false;
        }
    }

    std::pair<uint64_t, uint64_t> FrameContext::getFrameGPUTimestamps(uint32_t frameIndex) const {
        if (!mTimestampsEnabled || frameIndex >= mFrameData.size()) {
            return { 0, 0 };
        }

        const FrameData& frameData = mFrameData[frameIndex];
        if (frameData.timestampQueryPool == VK_NULL_HANDLE) {
            return { 0, 0 };
        }

        uint64_t timestamps[2] = { 0 };
        VkResult result = vkGetQueryPoolResults(
            mDevice->getLogicalDevice(),
            frameData.timestampQueryPool,
            0, 2,
            sizeof(timestamps),
            timestamps,
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
        );

        if (result == VK_SUCCESS) {
            return { timestamps[0], timestamps[1] };
        }

        return { 0, 0 };
    }

    float FrameContext::getFrameGPUTime(uint32_t frameIndex) const {
        auto [beginTime, endTime] = getFrameGPUTimestamps(frameIndex);
        if (beginTime == 0 || endTime == 0) {
            return 0.0f;
        }

        uint64_t diff = endTime - beginTime;
        return static_cast<float>(diff) * mTimestampPeriod / 1'000'000.0f;
    }

    // ==================== 内部创建函数 ====================

    bool FrameContext::createSyncObjects() {
        mFrameData.resize(mConfig.frameCount);

        for (size_t i = 0; i < mFrameData.size(); i++) {
            FrameData& frameData = mFrameData[i];

            try {
                // 创建信号量
                frameData.imageAvailableSemaphore = mDevice->createSemaphore();
                frameData.renderFinishedSemaphore = mDevice->createSemaphore();

                // 创建栅栏（初始为已信号状态，这样第一帧不会等待）
                frameData.inFlightFence = mDevice->createFence(VK_FENCE_CREATE_SIGNALED_BIT);

                // 设置调试名称
                std::string semaphoreName = "Frame" + std::to_string(i) + "_ImageAvailableSemaphore";
                mDevice->setObjectName(reinterpret_cast<uint64_t>(frameData.imageAvailableSemaphore),
                    VK_OBJECT_TYPE_SEMAPHORE,
                    semaphoreName.c_str());

                semaphoreName = "Frame" + std::to_string(i) + "_RenderFinishedSemaphore";
                mDevice->setObjectName(reinterpret_cast<uint64_t>(frameData.renderFinishedSemaphore),
                    VK_OBJECT_TYPE_SEMAPHORE,
                    semaphoreName.c_str());

                mDevice->setObjectName(reinterpret_cast<uint64_t>(frameData.inFlightFence),
                    VK_OBJECT_TYPE_FENCE,
                    ("Frame" + std::to_string(i) + "_InFlightFence").c_str());

            }
            catch (const std::runtime_error& e) {
                std::cerr << "[ERROR] Failed to create sync objects for frame " << i
                    << ": " << e.what() << std::endl;
                cleanupFrameData();
                return false;
            }
        }

        return true;
    }

    bool FrameContext::createCommandBuffers() {
        for (size_t i = 0; i < mFrameData.size(); i++) {
            FrameData& frameData = mFrameData[i];

            try {
                // 分配命令缓冲区
                frameData.commandBuffer = mDevice->allocateCommandBuffer(
                    mMainCommandPool,
                    VK_COMMAND_BUFFER_LEVEL_PRIMARY
                );

                // 设置调试名称
                mDevice->setObjectName(reinterpret_cast<uint64_t>(frameData.commandBuffer),
                    VK_OBJECT_TYPE_COMMAND_BUFFER,
                    ("Frame" + std::to_string(i) + "_CommandBuffer").c_str());

            }
            catch (const std::runtime_error& e) {
                std::cerr << "[ERROR] Failed to create command buffer for frame " << i
                    << ": " << e.what() << std::endl;
                cleanupFrameData();
                return false;
            }
        }

        return true;
    }

    bool FrameContext::createTimestampQueries() {
        if (!mConfig.enableTimestamps) {
            return false;
        }

        VkQueryPoolCreateInfo queryPoolInfo = {};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = 2;  // 开始和结束时间戳

        for (size_t i = 0; i < mFrameData.size(); i++) {
            FrameData& frameData = mFrameData[i];

            VkResult result = vkCreateQueryPool(
                mDevice->getLogicalDevice(),
                &queryPoolInfo,
                nullptr,
                &frameData.timestampQueryPool
            );

            if (result != VK_SUCCESS) {
                std::cerr << "[WARNING] Failed to create timestamp query pool for frame "
                    << i << std::endl;
                // 清理已创建的查询池
                for (size_t j = 0; j < i; j++) {
                    vkDestroyQueryPool(mDevice->getLogicalDevice(),
                        mFrameData[j].timestampQueryPool,
                        nullptr);
                    mFrameData[j].timestampQueryPool = VK_NULL_HANDLE;
                }
                return false;
            }

            // 设置调试名称
            mDevice->setObjectName(reinterpret_cast<uint64_t>(frameData.timestampQueryPool),
                VK_OBJECT_TYPE_QUERY_POOL,
                ("Frame" + std::to_string(i) + "_TimestampQueryPool").c_str());
        }

        return true;
    }

    void FrameContext::cleanupFrameData() {
        if (!mDevice) {
            return;
        }

        for (auto& frameData : mFrameData) {
            // 销毁信号量
            if (frameData.imageAvailableSemaphore != VK_NULL_HANDLE) {
                mDevice->destroySemaphore(frameData.imageAvailableSemaphore);
                frameData.imageAvailableSemaphore = VK_NULL_HANDLE;
            }

            if (frameData.renderFinishedSemaphore != VK_NULL_HANDLE) {
                mDevice->destroySemaphore(frameData.renderFinishedSemaphore);
                frameData.renderFinishedSemaphore = VK_NULL_HANDLE;
            }

            // 销毁栅栏
            if (frameData.inFlightFence != VK_NULL_HANDLE) {
                mDevice->destroyFence(frameData.inFlightFence);
                frameData.inFlightFence = VK_NULL_HANDLE;
            }

            // 销毁时间戳查询池
            if (frameData.timestampQueryPool != VK_NULL_HANDLE) {
                vkDestroyQueryPool(mDevice->getLogicalDevice(),
                    frameData.timestampQueryPool,
                    nullptr);
                frameData.timestampQueryPool = VK_NULL_HANDLE;
            }

            // 销毁线程命令池
            if (frameData.threadCommandPool != VK_NULL_HANDLE) {
                mDevice->destroyCommandPool(frameData.threadCommandPool);
                frameData.threadCommandPool = VK_NULL_HANDLE;
            }

            // 命令缓冲区由命令池统一管理，不需要单独销毁
            frameData.commandBuffer = VK_NULL_HANDLE;
        }

        mFrameData.clear();
    }

    void FrameContext::beginTimestampQuery(const FrameInfo& frameInfo) {
        if (!mTimestampsEnabled || frameInfo.frameIndex >= mFrameData.size()) {
            return;
        }

        FrameData& frameData = mFrameData[frameInfo.frameIndex];
        if (frameData.timestampQueryPool == VK_NULL_HANDLE) {
            return;
        }

        // 重置查询池
        vkCmdResetQueryPool(frameInfo.commandBuffer, frameData.timestampQueryPool, 0, 2);

        // 写入开始时间戳
        vkCmdWriteTimestamp(frameInfo.commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            frameData.timestampQueryPool,
            0);  // 查询索引0
    }

    void FrameContext::endTimestampQuery(FrameInfo& frameInfo) {
        if (!mTimestampsEnabled || frameInfo.frameIndex >= mFrameData.size()) {
            return;
        }

        FrameData& frameData = mFrameData[frameInfo.frameIndex];
        if (frameData.timestampQueryPool == VK_NULL_HANDLE) {
            return;
        }

        // 写入结束时间戳
        vkCmdWriteTimestamp(frameInfo.commandBuffer,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            frameData.timestampQueryPool,
            1);  // 查询索引1
    }

    void FrameContext::updateStatistics(const FrameInfo& frameInfo) {
        if (frameInfo.frameIndex >= mFrameData.size()) {
            return;
        }

        FrameData& frameData = mFrameData[frameInfo.frameIndex];

        // 更新帧计数器
        mStatistics.totalFrames++;
        frameData.frameNumber = mStatistics.totalFrames;

        // 记录CPU时间
        if (frameInfo.cpuBeginTime > 0 && frameInfo.cpuEndTime > 0) {
            float cpuTime = nanosecondsToMilliseconds(frameInfo.cpuEndTime - frameInfo.cpuBeginTime);
            frameData.cpuTime = cpuTime;
            frameData.cpuBeginTime = frameInfo.cpuBeginTime;
            frameData.cpuEndTime = frameInfo.cpuEndTime;

            // 更新CPU时间统计
            mStatistics.averageCPUTime = (mStatistics.averageCPUTime * (mStatistics.totalFrames - 1) + cpuTime)
                / mStatistics.totalFrames;
            if (cpuTime > mStatistics.maxCPUTime) {
                mStatistics.maxCPUTime = cpuTime;
            }
        }

        // 更新成功/失败帧计数
        if (frameInfo.acquireResult == VK_SUCCESS &&
            (frameInfo.presentResult == VK_SUCCESS || frameInfo.presentResult == VK_SUBOPTIMAL_KHR)) {
            mStatistics.successfulFrames++;
        }
        else {
            mStatistics.failedFrames++;
        }

        // 更新重建计数
        if (frameInfo.needsRecreate) {
            mStatistics.recreateCount++;
        }

        // 更新GPU时间统计
        if (mTimestampsEnabled) {
            float gpuTime = getFrameGPUTime(frameInfo.frameIndex);
            frameData.gpuTime = gpuTime;

            if (gpuTime > 0) {
                mStatistics.averageGPUTime = (mStatistics.averageGPUTime * (mStatistics.totalFrames - 1) + gpuTime)
                    / mStatistics.totalFrames;
                if (gpuTime > mStatistics.maxGPUTime) {
                    mStatistics.maxGPUTime = gpuTime;
                }
            }
        }

        // 总帧时间（CPU + GPU）
        float frameTime = frameData.cpuTime + frameData.gpuTime;
        mStatistics.averageFrameTime = (mStatistics.averageFrameTime * (mStatistics.totalFrames - 1) + frameTime)
            / mStatistics.totalFrames;
        if (frameTime > mStatistics.maxFrameTime) {
            mStatistics.maxFrameTime = frameTime;
        }
    }

    void FrameContext::validateFrameInfo(const FrameInfo& frameInfo) const {
        if (frameInfo.frameIndex >= mFrameData.size()) {
            throw std::runtime_error("Invalid frame index");
        }

        const FrameData& frameData = mFrameData[frameInfo.frameIndex];

        if (frameInfo.commandBuffer != frameData.commandBuffer) {
            throw std::runtime_error("Command buffer mismatch");
        }

        if (frameInfo.imageAvailableSemaphore != frameData.imageAvailableSemaphore ||
            frameInfo.renderFinishedSemaphore != frameData.renderFinishedSemaphore ||
            frameInfo.inFlightFence != frameData.inFlightFence) {
            throw std::runtime_error("Synchronization objects mismatch");
        }
    }

    void FrameContext::createThreadCommandPool(FrameData& frameData) {
        auto queueIndices = mDevice->getQueueFamilyIndices();
        if (!queueIndices.graphicsFamily.has_value()) {
            return;
        }

        try {
            frameData.threadCommandPool = mDevice->createCommandPool(
                queueIndices.graphicsFamily.value(),
                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
            );
        }
        catch (const std::runtime_error& e) {
            std::cerr << "[WARNING] Failed to create thread command pool: " << e.what() << std::endl;
        }
    }

    void FrameContext::resetStatistics() {
        mStatistics = Statistics{};

        for (auto& frameData : mFrameData) {
            frameData.frameNumber = 0;
            frameData.cpuTime = 0.0f;
            frameData.gpuTime = 0.0f;
            frameData.cpuBeginTime = 0;
            frameData.cpuEndTime = 0;
        }
    }

    void FrameContext::setFrameDebugName(uint32_t frameIndex, const char* name) {
        if (frameIndex < mFrameData.size() && name) {
            mFrameData[frameIndex].debugName = name;
        }
    }

    void FrameContext::dumpFrameInfo() const {
        std::cout << "\n=== Frame Context Information ===" << std::endl;
        std::cout << "Frame Count: " << mConfig.frameCount << std::endl;
        std::cout << "Current Frame Index: " << mCurrentFrameIndex << std::endl;
        std::cout << "Frame In Progress: " << (mFrameInProgress ? "Yes" : "No") << std::endl;
        std::cout << "Needs Recreate: " << (mNeedsRecreate ? "Yes" : "No") << std::endl;
        std::cout << "Recreate Attempts: " << mRecreateAttempts << std::endl;
        std::cout << "Auto Recreate: " << (mConfig.autoRecreate ? "Yes" : "No") << std::endl;
        std::cout << "Timestamps Enabled: " << (mTimestampsEnabled ? "Yes" : "No") << std::endl;
        std::cout << "Timestamp Period: " << mTimestampPeriod << " ns" << std::endl;

        std::cout << "\n=== Frame Data ===" << std::endl;
        for (size_t i = 0; i < mFrameData.size(); i++) {
            const FrameData& frame = mFrameData[i];
            std::cout << "  Frame " << i << ":" << std::endl;
            std::cout << "    Command Buffer: " << (frame.commandBuffer ? "Valid" : "Invalid") << std::endl;
            std::cout << "    Image Available Semaphore: " << (frame.imageAvailableSemaphore ? "Valid" : "Invalid") << std::endl;
            std::cout << "    Render Finished Semaphore: " << (frame.renderFinishedSemaphore ? "Valid" : "Invalid") << std::endl;
            std::cout << "    In Flight Fence: " << (frame.inFlightFence ? "Valid" : "Invalid") << std::endl;
            if (!frame.debugName.empty()) {
                std::cout << "    Debug Name: " << frame.debugName << std::endl;
            }
        }
    }

    void FrameContext::printStatistics() const {
        std::cout << "\n=== Frame Context Statistics ===" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Total Frames: " << mStatistics.totalFrames << std::endl;
        std::cout << "Successful Frames: " << mStatistics.successfulFrames << std::endl;
        std::cout << "Failed Frames: " << mStatistics.failedFrames << std::endl;
        std::cout << "Recreate Count: " << mStatistics.recreateCount << std::endl;
        std::cout << "Average Frame Time: " << mStatistics.averageFrameTime << " ms" << std::endl;
        std::cout << "Max Frame Time: " << mStatistics.maxFrameTime << " ms" << std::endl;
        std::cout << "Average CPU Time: " << mStatistics.averageCPUTime << " ms" << std::endl;
        std::cout << "Max CPU Time: " << mStatistics.maxCPUTime << " ms" << std::endl;
        std::cout << "Average GPU Time: " << mStatistics.averageGPUTime << " ms" << std::endl;
        std::cout << "Max GPU Time: " << mStatistics.maxGPUTime << " ms" << std::endl;

        if (mStatistics.averageFrameTime > 0) {
            std::cout << "Average FPS: " << (1000.0 / mStatistics.averageFrameTime) << std::endl;
        }

        if (mStatistics.totalFrames > 0) {
            double successRate = (static_cast<double>(mStatistics.successfulFrames) / mStatistics.totalFrames) * 100.0;
            std::cout << "Success Rate: " << successRate << "%" << std::endl;
        }
    }

} // namespace StarryEngine