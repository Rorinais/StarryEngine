#include <interface/vulkan/VulkanDevice.hpp>
#include <interface/vulkan/VulkanFrameContext.hpp>
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

    VulkanFrameContext::VulkanFrameContext(std::shared_ptr<VulkanDevice> device, const Config& config)
        : mDevice(device), mConfig(config) {

        if (!mDevice) {
            throw std::runtime_error("Device must be valid");
        }
    }

    VulkanFrameContext::~VulkanFrameContext() {
        cleanup();
    }

    bool VulkanFrameContext::initialize(uint32_t graphicsQueueFamilyIndex) {
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

    void VulkanFrameContext::cleanup() {
        if (!mDevice) {
            return;
        }

        mDevice->waitIdle();
        cleanupFrameData();
        cleanupPerImageSemaphores();
        mDevice->destroyCommandPool(mMainCommandPool);
        for (auto& slotPools : mWorkerCommandPools) {
            for (VkCommandPool pool : slotPools) {
                if (pool != VK_NULL_HANDLE) mDevice->destroyCommandPool(pool);
            }
        }
        mWorkerCommandPools.clear();
    }

    void VulkanFrameContext::initializePerImageSemaphores(uint32_t swapChainImageCount) {
        if (!mDevice) return;

        // 先清理旧的
        cleanupPerImageSemaphores();

        mSwapChainImageCount = swapChainImageCount;
        mPerImageRenderFinishedSemaphores.resize(swapChainImageCount);

        for (uint32_t i = 0; i < swapChainImageCount; i++) {
            mPerImageRenderFinishedSemaphores[i] = mDevice->createSemaphore();
            std::string name = "Image" + std::to_string(i) + "_RenderFinishedSemaphore";
            mDevice->setObjectName(
                reinterpret_cast<uint64_t>(mPerImageRenderFinishedSemaphores[i]),
                VK_OBJECT_TYPE_SEMAPHORE,
                name.c_str());
        }
    }

    void VulkanFrameContext::cleanupPerImageSemaphores() {
        if (!mDevice) return;
        for (auto& sem : mPerImageRenderFinishedSemaphores) {
            mDevice->destroySemaphore(sem);
        }
        mPerImageRenderFinishedSemaphores.clear();
        mSwapChainImageCount = 0;
    }

    // ==================== 帧循环接口 ====================

    VulkanFrameContext::FrameInfo VulkanFrameContext::beginFrame(const AcquireImageFunc& acquireFunc) {
        if (mFrameInProgress) {
            throw std::runtime_error("Frame already in progress");
        }

        // 等待当前槽位上一轮帧完成（frameCount=2 → 等待 2 帧前提交的帧）
        if (!waitForFrame(mCurrentFrameIndex)) {
            throw std::runtime_error("Failed to wait for previous frame");
        }

        resetWorkerCommandPools(mCurrentFrameIndex);

        if (mFrameData[mCurrentFrameIndex].hasSubmittedFirstFrame) {
            updateStatistics(mCurrentFrameIndex);
        }

        FrameInfo frameInfo;
        frameInfo.frameIndex = mCurrentFrameIndex; 
        FrameData& frameData = mFrameData[mCurrentFrameIndex];

        // 重置栅栏
        resetFrame(mCurrentFrameIndex);

        // 获取图像（通过回调）；离线无呈现模式：首次真实 acquire 后固定复用
        uint32_t imageIndex = 0;
        VkResult acquireResult = VK_SUCCESS;
        if (m_skipPresent && m_fixedImageAcquired) {
            imageIndex = m_fixedImageIndex;   // 复用已获取的图像（不 acquire → 不依赖 present 归还）
        } else {
            acquireResult = acquireFunc(frameData.imageAvailableSemaphore,
                VK_NULL_HANDLE,
                imageIndex);
            if (m_skipPresent) {
                m_fixedImageIndex = imageIndex;
                m_fixedImageAcquired = true;
            }
        }

        frameInfo.acquireResult = acquireResult;

        // 处理获取结果
        if (!handleAcquireResult(acquireResult, frameInfo)) {
            // VK_TIMEOUT / VK_NOT_READY 是瞬时错误，不需要重建 swap chain
            if (acquireResult != VK_TIMEOUT && acquireResult != VK_NOT_READY) {
                frameInfo.needsRecreate = true;
            }
            return frameInfo;
        }

        frameInfo.imageIndex = imageIndex;
        frameInfo.commandBuffer = frameData.commandBuffer;
        frameInfo.imageAvailableSemaphore = frameData.imageAvailableSemaphore;
        frameInfo.renderFinishedSemaphore = (imageIndex < mPerImageRenderFinishedSemaphores.size())
            ? mPerImageRenderFinishedSemaphores[imageIndex]
            : VK_NULL_HANDLE;
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

    void VulkanFrameContext::endFrame(FrameInfo& frameInfo) {
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

        // 记录 CPU 结束时间
        frameInfo.cpuEndTime = getCurrentTimeNanoseconds();

        if (frameInfo.frameIndex < mFrameData.size()) {
            FrameData& frameData = mFrameData[frameInfo.frameIndex];
            float cpuTime = nanosecondsToMilliseconds(frameInfo.cpuEndTime - frameInfo.cpuBeginTime);
            frameData.cpuTime = cpuTime;
            frameData.cpuBeginTime = frameInfo.cpuBeginTime;
            frameData.cpuEndTime = frameInfo.cpuEndTime;
        }

        mFrameInProgress = false;
    }

    VkResult VulkanFrameContext::submitFrame(FrameInfo& frameInfo,
        VkQueue graphicsQueue,
        const PresentImageFunc& presentFunc) {

        if (!graphicsQueue) {
            throw std::runtime_error("Invalid graphics queue");
        }

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
        submitInfo.waitSemaphoreCount = m_skipPresent ? 0u : 1u;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &frameInfo.commandBuffer;
        submitInfo.signalSemaphoreCount = m_skipPresent ? 0u : 1u;
        submitInfo.pSignalSemaphores = signalSemaphores;

        // 提交命令缓冲区
        VkResult submitResult = vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameInfo.inFlightFence);

        if (submitResult != VK_SUCCESS) {
            std::cerr << "[ERROR] Failed to submit command buffer: " << submitResult << std::endl;
            return submitResult;
        }

        // 该槽位完成一次完整 submit：此后读它的时间戳池才是安全的
        mFrameData[frameInfo.frameIndex].hasSubmittedFirstFrame = true;

        if (m_skipPresent) {
            // 离线无呈现：不调用 vkQueuePresentKHR（present 会卡住后续读回拷贝的排队）
            m_lastFrameIndex = mCurrentFrameIndex;
            mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mConfig.frameCount;
            return VK_SUCCESS;
        }

        // 呈现图像（通过回调）
        VkResult presentResult = presentFunc(graphicsQueue, frameInfo.imageIndex, frameInfo.renderFinishedSemaphore);
        frameInfo.presentResult = presentResult;

        // 处理呈现结果
        handlePresentResult(presentResult, frameInfo);

        // 如果启用了自动重建并且需要重建，尝试重建
        if (mConfig.autoRecreate && frameInfo.needsRecreate && mRecreateCallback) {
            std::cout << "[INFO] Auto-recreating swap chain..." << std::endl;
        }

        // 更新帧索引
        m_lastFrameIndex = mCurrentFrameIndex;
        mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mConfig.frameCount;
        return presentResult;
    }

    VulkanFrameContext::FrameInfo VulkanFrameContext::processFrame(const AcquireImageFunc& acquireFunc,
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

    bool VulkanFrameContext::recreateSwapChain(uint32_t width, uint32_t height) {
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

    void VulkanFrameContext::handleRecreateResult(bool success) {
        if (success) {
            mNeedsRecreate = false;
            mRecreateAttempts = 0;
        }
        else {
            mNeedsRecreate = true; 
        }
    }

    void VulkanFrameContext::resetAllFrames() {
        if (!mDevice) return;

        // 确保所有 GPU 工作完成
        mDevice->waitIdle();

        // 等待所有栅栏完成，但不要重置它们
        for (auto& frameData : mFrameData) {
            if (frameData.inFlightFence != VK_NULL_HANDLE) {
                vkWaitForFences(mDevice->getLogicalDevice(), 1, &frameData.inFlightFence, VK_TRUE, UINT64_MAX);
            }
        }

        mCurrentFrameIndex = 0;
        mFrameInProgress = false;
        mNeedsRecreate = false;
        mRecreateAttempts = 0;
    }

    // ==================== 重建处理辅助函数 ====================

    bool VulkanFrameContext::handleAcquireResult(VkResult result, FrameInfo& frameInfo) {
        switch (result) {
        case VK_SUCCESS:
            return true;

        case VK_SUBOPTIMAL_KHR:
            // 图像有效，照常渲染。窗口 resize 回调会触发重建，不阻塞帧循环
            std::cout << "[INFO] Swap chain is suboptimal (acquire)" << std::endl;
            return true;

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

    bool VulkanFrameContext::handlePresentResult(VkResult result, FrameInfo& frameInfo) {
        switch (result) {
        case VK_SUCCESS:
            return true;

        case VK_SUBOPTIMAL_KHR:
            std::cout << "[INFO] Swap chain is suboptimal (present)" << std::endl;
            return true;

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

    bool VulkanFrameContext::shouldRecreate(VkResult result) const {
        return result == VK_ERROR_OUT_OF_DATE_KHR ||
            result == VK_ERROR_SURFACE_LOST_KHR ||
            result == VK_SUBOPTIMAL_KHR;
    }

    void VulkanFrameContext::notifyRecreateNeeded(FrameInfo& frameInfo, const std::string& reason) {
        std::cout << "[INFO] " << reason << std::endl;
        frameInfo.needsRecreate = true;
        frameInfo.imageIndex = UINT32_MAX; 
        mNeedsRecreate = true;
    }

    // ==================== 资源访问 ====================

    VulkanFrameContext::FrameInfo VulkanFrameContext::getCurrentFrameInfo() const {
        FrameInfo frameInfo;
        frameInfo.frameIndex = mCurrentFrameIndex;

        if (mCurrentFrameIndex < mFrameData.size()) {
            const FrameData& frameData = mFrameData[mCurrentFrameIndex];
            frameInfo.commandBuffer = frameData.commandBuffer;
            frameInfo.imageAvailableSemaphore = frameData.imageAvailableSemaphore;
            frameInfo.inFlightFence = frameData.inFlightFence;
        }

        return frameInfo;
    }

    VkCommandBuffer VulkanFrameContext::getCurrentCommandBuffer() const {
        if (mCurrentFrameIndex < mFrameData.size()) {
            return mFrameData[mCurrentFrameIndex].commandBuffer;
        }
        return VK_NULL_HANDLE;
    }

    // ==================== 状态查询 ====================

    bool VulkanFrameContext::waitForFrame(uint32_t frameIndex, uint64_t timeout) {
        if (frameIndex >= mFrameData.size()) {
            return false;
        }

        VkFence fence = mFrameData[frameIndex].inFlightFence;
        if (fence == VK_NULL_HANDLE) {
            return true;
        }

        VkResult result = vkWaitForFences(mDevice->getLogicalDevice(), 1, &fence, VK_TRUE, timeout);
        if (result != VK_SUCCESS) {
            std::cerr << "[ERROR] waitForFrame failed for frame " << frameIndex
                << " with result: " << result << std::endl;
            return false;
        }
        return true;
    }

    void VulkanFrameContext::resetFrame(uint32_t frameIndex) {
        if (frameIndex >= mFrameData.size()) {
            return;
        }

        VkFence fence = mFrameData[frameIndex].inFlightFence;
        if (fence != VK_NULL_HANDLE) {
            vkResetFences(mDevice->getLogicalDevice(), 1, &fence);
        }
    }

    // ==================== 多线程支持 ====================

    VkCommandBuffer VulkanFrameContext::allocateThreadCommandBuffer(uint32_t threadIndex, VkCommandBufferLevel level) {
        if (threadIndex >= mFrameData.size()) {
            return VK_NULL_HANDLE;
        }

        FrameData& frameData = mFrameData[threadIndex];

        // 创建线程命令池
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

    void VulkanFrameContext::freeThreadCommandBuffers(uint32_t threadIndex) {
        if (threadIndex >= mFrameData.size()) {
            return;
        }

        FrameData& frameData = mFrameData[threadIndex];
        if (frameData.threadCommandPool != VK_NULL_HANDLE) {
            vkResetCommandPool(mDevice->getLogicalDevice(),
                frameData.threadCommandPool,
                VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
        }
    }

    VkCommandPool VulkanFrameContext::getThreadCommandPool(uint32_t threadIndex) {
        if (threadIndex < mFrameData.size()) {
            return mFrameData[threadIndex].threadCommandPool;
        }
        return VK_NULL_HANDLE;
    }

    // ==================== 并行命令录制（per-worker 命令池） ====================

    VkCommandPool VulkanFrameContext::getOrCreateWorkerCommandPool(uint32_t frameSlot, uint32_t workerIndex) {
        // 调用方保证：reserveWorkerCommandPools 已在主线程预建好 [slot][worker] 骨架；
        // 每个 worker 只触碰自己的索引，故这里无并发写同一元素的问题。
        if (mWorkerCommandPools.empty() || frameSlot >= mWorkerCommandPools.size()) {
            mWorkerCommandPools.resize(frameSlot + 1);
        }
        auto& slotPools = mWorkerCommandPools[frameSlot];
        if (workerIndex >= slotPools.size()) {
            slotPools.resize(workerIndex + 1, VK_NULL_HANDLE);
        }
        if (slotPools[workerIndex] == VK_NULL_HANDLE) {
            auto queueIndices = mDevice->getQueueFamilyIndices();
            slotPools[workerIndex] = mDevice->createCommandPool(
                queueIndices.graphicsFamily.value(),
                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        }
        return slotPools[workerIndex];
    }

    VkCommandBuffer VulkanFrameContext::allocateWorkerCommandBuffer(uint32_t frameSlot, uint32_t workerIndex, VkCommandBufferLevel level) {
        VkCommandPool pool = getOrCreateWorkerCommandPool(frameSlot, workerIndex);
        if (pool == VK_NULL_HANDLE) return VK_NULL_HANDLE;
        try {
            return mDevice->allocateCommandBuffer(pool, level);
        }
        catch (const std::runtime_error& e) {
            std::cerr << "[ERROR] Failed to allocate worker command buffer: " << e.what() << std::endl;
            return VK_NULL_HANDLE;
        }
    }

    void VulkanFrameContext::resetWorkerCommandPools(uint32_t frameSlot) {
        if (frameSlot >= mWorkerCommandPools.size()) return;   // 未启用并行录制：零开销
        for (VkCommandPool pool : mWorkerCommandPools[frameSlot]) {
            if (pool != VK_NULL_HANDLE) {
                vkResetCommandPool(mDevice->getLogicalDevice(), pool, 0);
            }
        }
    }

    void VulkanFrameContext::reserveWorkerCommandPools(uint32_t workerCount) {
        if (workerCount == 0) return;
        // 主线程一次性建好 [frameCount][workerCount] 骨架；worker 只在录制时读自己的池，
        // 不再触发 vector resize（避免并发 resize 数据竞争）。
        if (mWorkerCommandPools.size() < mConfig.frameCount) {
            mWorkerCommandPools.resize(mConfig.frameCount);
        }
        for (auto& slotPools : mWorkerCommandPools) {
            if (slotPools.size() < workerCount) {
                slotPools.resize(workerCount, VK_NULL_HANDLE);
            }
        }
    }

    // ==================== 时间戳查询 ====================

    void VulkanFrameContext::enableTimestamps(bool enable) {
        if (enable && !mTimestampsEnabled) {
            mTimestampsEnabled = createTimestampQueries();
        }
        else if (!enable && mTimestampsEnabled) {
            for (auto& frameData : mFrameData) {
                mDevice->destroyQueryPool(frameData.timestampQueryPool);
            }
            mTimestampsEnabled = false;
        }
    }

    std::pair<uint64_t, uint64_t> VulkanFrameContext::getFrameGPUTimestamps(uint32_t frameIndex) const {
        if (!mTimestampsEnabled || frameIndex >= mFrameData.size()) {
            return { 0, 0 };
        }

        const FrameData& frameData = mFrameData[frameIndex];
        if (frameData.timestampQueryPool == VK_NULL_HANDLE) {
            return { 0, 0 };
        }

        uint64_t timestamps[2] = { 0 };
        // 非阻塞读：调用点（beginFrame）已 waitForFrame 该槽位的 fence，查询必然就绪。
        // 不再用 WAIT_BIT——它会在帧中途插隐式全管线同步，Wayland/NVIDIA 上死锁 acquire。
        VkResult result = vkGetQueryPoolResults(
            mDevice->getLogicalDevice(),
            frameData.timestampQueryPool,
            0, 2,
            sizeof(timestamps),
            timestamps,
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT
        );

        if (result == VK_SUCCESS) {
            return { timestamps[0], timestamps[1] };
        }

        return { 0, 0 };
    }

    float VulkanFrameContext::getFrameGPUTime(uint32_t frameIndex) const {
        auto [beginTime, endTime] = getFrameGPUTimestamps(frameIndex);
        if (beginTime == 0 || endTime == 0) {
            return 0.0f;
        }

        uint64_t diff = endTime - beginTime;
        return static_cast<float>(diff) * mTimestampPeriod / 1'000'000.0f;
    }

    // ==================== 内部创建函数 ====================

    bool VulkanFrameContext::createSyncObjects() {
        mFrameData.resize(mConfig.frameCount);

        for (size_t i = 0; i < mFrameData.size(); i++) {
            FrameData& frameData = mFrameData[i];

            try {
                // 创建信号量（imageAvailable 按 frame 索引，renderFinished 按 image 索引）
                frameData.imageAvailableSemaphore = mDevice->createSemaphore();

                // 创建栅栏
                frameData.inFlightFence = mDevice->createFence(VK_FENCE_CREATE_SIGNALED_BIT);

                // 设置调试名称
                std::string semaphoreName = "Frame" + std::to_string(i) + "_ImageAvailableSemaphore";
                mDevice->setObjectName(reinterpret_cast<uint64_t>(frameData.imageAvailableSemaphore),
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

    bool VulkanFrameContext::createCommandBuffers() {
        for (size_t i = 0; i < mFrameData.size(); i++) {
            FrameData& frameData = mFrameData[i];

            try {
                // 分配命令缓冲区
                frameData.commandBuffer = mDevice->allocateCommandBuffer(mMainCommandPool,VK_COMMAND_BUFFER_LEVEL_PRIMARY);

                // 设置调试名称
                mDevice->setObjectName(reinterpret_cast<uint64_t>(frameData.commandBuffer),VK_OBJECT_TYPE_COMMAND_BUFFER,("Frame" + std::to_string(i) + "_CommandBuffer").c_str());

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

    bool VulkanFrameContext::createTimestampQueries() {
        if (!mConfig.enableTimestamps) {
            return false;
        }

        VkQueryPoolCreateInfo queryPoolInfo = {};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = 2;

        for (size_t i = 0; i < mFrameData.size(); i++) {
            FrameData& frameData = mFrameData[i];

            VkResult result = vkCreateQueryPool(
                mDevice->getLogicalDevice(),
                &queryPoolInfo,
                nullptr,
                &frameData.timestampQueryPool
            );

            if (result != VK_SUCCESS) {
                std::cerr << "[WARNING] Failed to create timestamp query pool for frame "<< i << std::endl;
                // 清理已创建的查询池
                for (size_t j = 0; j < i; j++) {
					mDevice->destroyQueryPool(mFrameData[j].timestampQueryPool);
                }
                return false;
            }

            // 设置调试名称
            mDevice->setObjectName(reinterpret_cast<uint64_t>(frameData.timestampQueryPool),VK_OBJECT_TYPE_QUERY_POOL,("Frame" + std::to_string(i) + "_TimestampQueryPool").c_str());
        }

        return true;
    }

    void VulkanFrameContext::cleanupFrameData() {
        if (!mDevice) {
            return;
        }

        for (auto& frameData : mFrameData) {
            mDevice->destroySemaphore(frameData.imageAvailableSemaphore);
            mDevice->destroyFence(frameData.inFlightFence);
			mDevice->destroyQueryPool(frameData.timestampQueryPool);
            mDevice->destroyCommandPool(frameData.threadCommandPool);
        }
        mFrameData.clear();
    }

    void VulkanFrameContext::beginTimestampQuery(const FrameInfo& frameInfo) {
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
            0);  
    }

    void VulkanFrameContext::endTimestampQuery(FrameInfo& frameInfo) {
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
            1);  
    }

    void VulkanFrameContext::updateStatistics(uint32_t completedFrameIndex) {
        if (completedFrameIndex >= mFrameData.size()) return;

        FrameData& frameData = mFrameData[completedFrameIndex];

        // 1. 递增总帧数并保存当前计数值
        mStatistics.totalFrames++;
        uint64_t total = mStatistics.totalFrames;  

        // 记录该帧的序号
        frameData.frameNumber = total;

        // 2. CPU 时间统计
        float cpuTime = frameData.cpuTime;
        if (cpuTime > 0) {
            // 更新平均 CPU 时间（使用新的总帧数 total）
            mStatistics.averageCPUTime = (mStatistics.averageCPUTime * (total - 1) + cpuTime) / total;
            if (cpuTime > mStatistics.maxCPUTime) {
                mStatistics.maxCPUTime = cpuTime;
            }
        }

        // 3. GPU 时间统计（如果启用）
        if (mTimestampsEnabled) {
            float gpuTime = getFrameGPUTime(completedFrameIndex);
            frameData.gpuTime = gpuTime;

            if (gpuTime > 0) {
                mStatistics.averageGPUTime = (mStatistics.averageGPUTime * (total - 1) + gpuTime) / total;
                if (gpuTime > mStatistics.maxGPUTime) {
                    mStatistics.maxGPUTime = gpuTime;
                }
            }
        }

        // 4. 总帧时间（CPU + GPU）
        float frameTime = frameData.cpuTime + frameData.gpuTime;
        mStatistics.averageFrameTime = (mStatistics.averageFrameTime * (total - 1) + frameTime) / total;
        if (frameTime > mStatistics.maxFrameTime) {
            mStatistics.maxFrameTime = frameTime;
        }
    }


    void VulkanFrameContext::validateFrameInfo(const FrameInfo& frameInfo) const {
        if (frameInfo.frameIndex >= mFrameData.size()) {
            throw std::runtime_error("Invalid frame index");
        }

        const FrameData& frameData = mFrameData[frameInfo.frameIndex];

        if (frameInfo.commandBuffer != frameData.commandBuffer) {
            throw std::runtime_error("Command buffer mismatch");
        }

        if (frameInfo.imageAvailableSemaphore != frameData.imageAvailableSemaphore ||
            frameInfo.inFlightFence != frameData.inFlightFence) {
            throw std::runtime_error("Synchronization objects mismatch");
        }
    }

    void VulkanFrameContext::createThreadCommandPool(FrameData& frameData) {
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

    void VulkanFrameContext::resetStatistics() {
        mStatistics = Statistics{};

        for (auto& frameData : mFrameData) {
            frameData.frameNumber = 0;
            frameData.cpuTime = 0.0f;
            frameData.gpuTime = 0.0f;
            frameData.cpuBeginTime = 0;
            frameData.cpuEndTime = 0;
        }
    }

    void VulkanFrameContext::setFrameDebugName(uint32_t frameIndex, const char* name) {
        if (frameIndex < mFrameData.size() && name) {
            mFrameData[frameIndex].debugName = name;
        }
    }

    void VulkanFrameContext::dumpFrameInfo() const {
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
            std::cout << "    In Flight Fence: " << (frame.inFlightFence ? "Valid" : "Invalid") << std::endl;
            if (!frame.debugName.empty()) {
                std::cout << "    Debug Name: " << frame.debugName << std::endl;
            }
        }

        std::cout << "\n=== Per-Image Render Finished Semaphores ===" << std::endl;
        for (size_t i = 0; i < mPerImageRenderFinishedSemaphores.size(); i++) {
            std::cout << "  Image " << i << ": "
                      << (mPerImageRenderFinishedSemaphores[i] ? "Valid" : "Invalid") << std::endl;
        }
    }

    void VulkanFrameContext::printStatistics() const {
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