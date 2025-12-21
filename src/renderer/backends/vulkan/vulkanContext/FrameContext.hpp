#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <functional>
#include <string>

namespace StarryEngine {
    class Device;

    // 回调函数类型定义
    using AcquireImageFunc = std::function<VkResult(VkSemaphore, VkFence, uint32_t&)>;
    using PresentImageFunc = std::function<VkResult(VkQueue, uint32_t, VkSemaphore)>;
    using RecreateSwapChainFunc = std::function<bool(uint32_t, uint32_t)>;

    class FrameContext {
    public:
        struct Config {
            uint32_t frameCount = 2;  // 双缓冲或三缓冲
            bool usePersistentCommandBuffers = true;
            bool enableTimestamps = false;
            VkCommandBufferResetFlags commandBufferResetFlags = VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT;

            // 命令池配置
            VkCommandPoolCreateFlags commandPoolFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            // 重建配置
            uint32_t maxRecreateAttempts = 3;
            bool autoRecreate = false;  // 是否自动重建交换链
        };

        struct FrameInfo {
            uint32_t frameIndex = 0;
            uint32_t imageIndex = 0;
            VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
            VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
            VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
            VkFence inFlightFence = VK_NULL_HANDLE;

            // 重建状态
            bool needsRecreate = false;
            VkResult acquireResult = VK_SUCCESS;
            VkResult presentResult = VK_SUCCESS;

            // 时间戳
            uint64_t cpuBeginTime = 0;
            uint64_t cpuEndTime = 0;

            // 用户数据指针
            void* userData = nullptr;
        };

        struct Statistics {
            uint64_t totalFrames = 0;
            uint64_t successfulFrames = 0;
            uint64_t failedFrames = 0;
            uint64_t recreateCount = 0;
            double averageFrameTime = 0.0;
            double maxFrameTime = 0.0;
            double averageGPUTime = 0.0;
            double maxGPUTime = 0.0;
            double averageCPUTime = 0.0;
            double maxCPUTime = 0.0;
        };

        using Ptr = std::shared_ptr<FrameContext>;

        static Ptr create(std::shared_ptr<Device> device, const Config& config = Config()) {
            return std::make_shared<FrameContext>(device, config);
        }

        FrameContext(std::shared_ptr<Device> device, const Config& config = Config());
        ~FrameContext();

        // 禁用拷贝和赋值
        FrameContext(const FrameContext&) = delete;
        FrameContext& operator=(const FrameContext&) = delete;

        // === 生命周期管理 ===
        bool initialize(uint32_t graphicsQueueFamilyIndex);
        void cleanup();

        // === 回调设置 ===
        void setRecreateCallback(const RecreateSwapChainFunc& callback) {
            mRecreateCallback = callback;
        }

        // === 帧循环接口 ===
        FrameInfo beginFrame(const AcquireImageFunc& acquireFunc);
        void endFrame(FrameInfo& frameInfo);
        VkResult submitFrame(FrameInfo& frameInfo,
            VkQueue graphicsQueue,
            const PresentImageFunc& presentFunc);

        // 简化接口：一次性执行完整帧
        FrameInfo processFrame(const AcquireImageFunc& acquireFunc,
            VkQueue graphicsQueue,
            const PresentImageFunc& presentFunc);

        // === 资源访问 ===
        uint32_t getCurrentFrameIndex() const { return mCurrentFrameIndex; }
        FrameInfo getCurrentFrameInfo() const;
        VkCommandBuffer getCurrentCommandBuffer() const;
        VkCommandPool getCommandPool() const { return mMainCommandPool; }

        // === 状态查询 ===
        bool isFrameInProgress() const { return mFrameInProgress; }
        bool isRecreationNeeded() const { return mNeedsRecreate; }
        bool waitForFrame(uint32_t frameIndex, uint64_t timeout = UINT64_MAX);
        void resetFrame(uint32_t frameIndex);

        // === 重建处理 ===
        bool recreateSwapChain(uint32_t width, uint32_t height);
        void handleRecreateResult(bool success);
        void setAutoRecreate(bool autoRecreate) { mConfig.autoRecreate = autoRecreate; }

        // === 时间戳查询 ===
        void enableTimestamps(bool enable);
        std::pair<uint64_t, uint64_t> getFrameGPUTimestamps(uint32_t frameIndex) const;
        float getFrameGPUTime(uint32_t frameIndex) const;

        // === 统计信息 ===
        const Statistics& getStatistics() const { return mStatistics; }
        void resetStatistics();
        uint64_t getTotalFramesRendered() const { return mStatistics.totalFrames; }
        double getAverageFPS() const {
            return mStatistics.averageFrameTime > 0 ? 1000.0 / mStatistics.averageFrameTime : 0.0;
        }

        // === 调试 ===
        void setFrameDebugName(uint32_t frameIndex, const char* name);
        void dumpFrameInfo() const;
        void printStatistics() const;

    private:
        struct FrameData {
            VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
            VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
            VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
            VkFence inFlightFence = VK_NULL_HANDLE;

            // 时间戳查询
            VkQueryPool timestampQueryPool = VK_NULL_HANDLE;

            // 调试信息
            std::string debugName;

            // 统计信息
            uint64_t frameNumber = 0;
            float cpuTime = 0.0f;
            float gpuTime = 0.0f;
            uint64_t cpuBeginTime = 0;
            uint64_t cpuEndTime = 0;

            // 线程命令池（用于多线程录制）
            VkCommandPool threadCommandPool = VK_NULL_HANDLE;
        };

        std::shared_ptr<Device> mDevice;
        Config mConfig;

        // 回调函数
        RecreateSwapChainFunc mRecreateCallback;

        // 帧数据
        std::vector<FrameData> mFrameData;
        uint32_t mCurrentFrameIndex = 0;
        bool mFrameInProgress = false;
        bool mNeedsRecreate = false;
        uint32_t mRecreateAttempts = 0;

        // 主命令池
        VkCommandPool mMainCommandPool = VK_NULL_HANDLE;

        // 统计信息
        Statistics mStatistics;

        // 时间戳支持
        bool mTimestampsEnabled = false;
        float mTimestampPeriod = 1.0f;

    private:
        // 内部创建函数
        bool createSyncObjects();
        bool createCommandBuffers();
        bool createTimestampQueries();
        void cleanupFrameData();

        // 时间戳辅助函数
        void beginTimestampQuery(const FrameInfo& frameInfo);
        void endTimestampQuery(FrameInfo& frameInfo);

        // 统计更新
        void updateStatistics(const FrameInfo& frameInfo);

        // 验证
        void validateFrameInfo(const FrameInfo& frameInfo) const;

        // 多线程支持
        void createThreadCommandPool(FrameData& frameData);

        // 重建处理辅助函数
        bool handleAcquireResult(VkResult result, FrameInfo& frameInfo);
        bool handlePresentResult(VkResult result, FrameInfo& frameInfo);
        bool shouldRecreate(VkResult result) const;
        void notifyRecreateNeeded(FrameInfo& frameInfo, const std::string& reason);
    };

} // namespace StarryEngine