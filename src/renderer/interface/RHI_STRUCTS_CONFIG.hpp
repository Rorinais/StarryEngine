#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <functional>
#include <atomic>
#include <mutex>

#include "RHI_ENUMS.hpp"
#include "RHI_HANDLES_SYSTEM.hpp"
#include "RHI_STRUCTS_DESC.hpp"

namespace StarryEngine::RHI {
    // ==================== 可比较的函数包装器 ====================
    template<typename Signature>
    class ComparableFunction;

    template<typename R, typename... Args>
    class ComparableFunction<R(Args...)> {
    public:
        using FunctionType = std::function<R(Args...)>;

        // 线程安全的ID生成器
        static size_t getNextId() {
            static std::atomic<size_t> globalCounter{ 0 };
            return ++globalCounter;
        }

        // 默认构造函数
        ComparableFunction() = default;

        // nullptr_t 构造函数
        ComparableFunction(std::nullptr_t) : func_(nullptr), id_(0) {}

        // 复制构造函数
        ComparableFunction(const ComparableFunction& other)
            : func_(other.func_), id_(other.id_) {}

        // 移动构造函数
        ComparableFunction(ComparableFunction&& other) noexcept
            : func_(std::move(other.func_)), id_(other.id_) {
            other.id_ = 0;
        }

        // 从std::function构造
        ComparableFunction(const FunctionType& f) : func_(f), id_(getNextId()) {}

        // 从任何可调用对象构造
        template<typename F, typename = typename std::enable_if<
            !std::is_same<typename std::decay<F>::type, ComparableFunction>::value
        >::type>
        ComparableFunction(F&& f) : func_(std::forward<F>(f)), id_(getNextId()) {}

        // 赋值运算符
        ComparableFunction& operator=(const ComparableFunction& other) {
            if (this != &other) {
                func_ = other.func_;
                id_ = other.id_;
            }
            return *this;
        }

        ComparableFunction& operator=(ComparableFunction&& other) noexcept {
            if (this != &other) {
                func_ = std::move(other.func_);
                id_ = other.id_;
                other.id_ = 0;
            }
            return *this;
        }

        ComparableFunction& operator=(std::nullptr_t) {
            func_ = nullptr;
            id_ = 0;
            return *this;
        }

        // 调用操作符
        R operator()(Args... args) const {
            return func_(std::forward<Args>(args)...);
        }

        // 比较操作符
        bool operator==(const ComparableFunction& other) const {
            return id_ == other.id_;
        }

        bool operator!=(const ComparableFunction& other) const {
            return id_ != other.id_;
        }

        bool operator==(std::nullptr_t) const {
            return !static_cast<bool>(func_);
        }

        bool operator!=(std::nullptr_t) const {
            return static_cast<bool>(func_);
        }

        // 布尔转换
        explicit operator bool() const {
            return static_cast<bool>(func_);
        }

        // 获取ID，用于调试
        size_t getId() const { return id_; }

        // 获取底层函数引用
        const FunctionType& getFunction() const { return func_; }
        FunctionType& getFunction() { return func_; }

    private:
        FunctionType func_;
        size_t id_ = 0;
    };

    // ==================== 回调函数类型 ====================

    /**
     * @brief 帧回调函数类型
     * @details 每帧调用的回调函数
     */
    using FrameCallback = ComparableFunction<void(FrameData&)>;

    /**
     * @brief 窗口大小调整回调函数类型
     * @details 窗口大小改变时调用的回调函数
     */
    using ResizeCallback = ComparableFunction<void(uint32_t width, uint32_t height)>;

    /**
     * @brief 错误回调函数类型
     * @details 发生错误时调用的回调函数
     */
    using ErrorCallback = ComparableFunction<void(const std::string& error, bool fatal)>;

    /**
     * @brief 调试回调函数类型
     * @details 接收调试信息的回调函数
     */
    using DebugCallback = ComparableFunction<void(
        MessageSeverity severity,
        MessageSource source,
        const std::string& message)>;

    /**
     * @brief RHI初始化配置结构体
     * @details 包含渲染API初始化所需的所有配置参数
     */
    struct RHIInitConfig {
        // === 核心配置 ===
        API api = API::Vulkan;
        FeatureLevel featureLevel = FeatureLevel::VK_1_3;
        bool enableDebug = true;

        // === 窗口/显示配置 ===
        void* windowHandle = nullptr;
        uint32_t windowWidth = 1280;
        uint32_t windowHeight = 720;
        bool vsync = true;
        uint32_t swapChainImages = 2;
        bool srgb = true;
        bool hdr = false;

        // === 应用版本信息 ===
        Version appVersion = { 1, 0, 0 };
        Version engineVersion = { 1, 0, 0 };
        std::string appName = "StarryEngine App";
        std::string engineName = "StarryEngine";

        // === 调试配置 ===
        bool enableGPUValidation = false;
        bool enableRenderDoc = false;
        bool enableNsight = false;
        bool enableAftermath = false;
        DebugCallback debugCallback;

        // === 实例扩展和层 ===
        std::vector<std::string> requiredExtensions = {};
        std::vector<std::string> requiredLayers = { "VK_LAYER_KHRONOS_validation" };

        // === 设备配置 ===
        struct DeviceFeatures {
            bool samplerAnisotropy = true;
            bool geometryShader = false;
            bool tessellationShader = false;
            bool meshShader = false;
            bool rayTracing = false;
            bool computeShader = true;
            bool textureCompression = true;
            bool fillModeNonSolid = false;
            bool wideLines = false;
            bool synchronization = true;
            bool dynamicRendering = true;
        } deviceFeatures;

        std::vector<std::string> deviceExtensions = { "VK_KHR_swapchain" };
        float queuePriority = 1.0f;
        bool enableVMA = true;

        // === 交换链配置 ===
        enum class PresentMode {
            FIFO,       // 垂直同步
            MAILBOX,    // 无垂直同步（邮箱模式）
            IMMEDIATE   // 立即呈现
        };

        // 透明窗口：请求 swapchain alpha 合成（PRS/POST_MULTIPLIED），让桌面透过来
        bool requestTransparentSwapchain = false;

        PresentMode presentMode = PresentMode::FIFO;
        bool enableMailboxMode = false;
        bool enableImmediateMode = false;

        // === 帧上下文配置 ===
        uint32_t frameBuffering = 2;              // 双缓冲/三缓冲
        bool usePersistentCommandBuffers = true;
        bool enableTimestamps = false;
        bool allowCommandBufferReset = true;
        bool allowCommandPoolReset = true;
        uint32_t maxRecreateAttempts = 3;
        bool autoRecreateSwapChain = false;

        // === 多线程配置 ===
        bool enableMultiThreading = true;
        uint32_t maxThreadCount = 4;

        // === 缓存配置 ===
        bool enablePipelineCache = true;
        std::string pipelineCacheFile = "pipeline_cache.bin";
        bool enableShaderCache = true;
        std::string shaderCacheDir = "shader_cache";

        // === 内存配置 ===
        bool enableMemoryAllocator = true;
        bool enableDescriptorAllocator = true;
        bool enableCommandAllocator = true;

        // === 构建时默认配置 ===
        RHIInitConfig() {
#ifdef NDEBUG
            enableDebug = false;
            enableGPUValidation = false;
#endif
        }

        bool operator==(const RHIInitConfig& other) const {
            return api == other.api &&
                featureLevel == other.featureLevel &&
                enableDebug == other.enableDebug &&
                windowHandle == other.windowHandle &&
                windowWidth == other.windowWidth &&
                windowHeight == other.windowHeight &&
                vsync == other.vsync &&
                swapChainImages == other.swapChainImages &&
                srgb == other.srgb &&
                hdr == other.hdr &&
                appVersion == other.appVersion &&
                engineVersion == other.engineVersion &&
                appName == other.appName &&
                engineName == other.engineName &&
                enableGPUValidation == other.enableGPUValidation &&
                enableRenderDoc == other.enableRenderDoc &&
                enableNsight == other.enableNsight &&
                enableAftermath == other.enableAftermath &&
                requiredExtensions == other.requiredExtensions &&
                requiredLayers == other.requiredLayers &&
                deviceFeatures.samplerAnisotropy == other.deviceFeatures.samplerAnisotropy &&
                deviceFeatures.geometryShader == other.deviceFeatures.geometryShader &&
                deviceFeatures.tessellationShader == other.deviceFeatures.tessellationShader &&
                deviceFeatures.meshShader == other.deviceFeatures.meshShader &&
                deviceFeatures.rayTracing == other.deviceFeatures.rayTracing &&
                deviceFeatures.computeShader == other.deviceFeatures.computeShader &&
                deviceFeatures.textureCompression == other.deviceFeatures.textureCompression &&
                deviceFeatures.fillModeNonSolid == other.deviceFeatures.fillModeNonSolid &&
                deviceFeatures.wideLines == other.deviceFeatures.wideLines &&
                deviceFeatures.synchronization == other.deviceFeatures.synchronization &&
                deviceFeatures.dynamicRendering == other.deviceFeatures.dynamicRendering &&
                deviceExtensions == other.deviceExtensions &&
                queuePriority == other.queuePriority &&
                enableVMA == other.enableVMA &&
                presentMode == other.presentMode &&
                enableMailboxMode == other.enableMailboxMode &&
                enableImmediateMode == other.enableImmediateMode &&
                frameBuffering == other.frameBuffering &&
                usePersistentCommandBuffers == other.usePersistentCommandBuffers &&
                enableTimestamps == other.enableTimestamps &&
                allowCommandBufferReset == other.allowCommandBufferReset &&
                allowCommandPoolReset == other.allowCommandPoolReset &&
                maxRecreateAttempts == other.maxRecreateAttempts &&
                autoRecreateSwapChain == other.autoRecreateSwapChain &&
                enableMultiThreading == other.enableMultiThreading &&
                maxThreadCount == other.maxThreadCount &&
                enablePipelineCache == other.enablePipelineCache &&
                pipelineCacheFile == other.pipelineCacheFile &&
                enableShaderCache == other.enableShaderCache &&
                shaderCacheDir == other.shaderCacheDir &&
                enableMemoryAllocator == other.enableMemoryAllocator &&
                enableDescriptorAllocator == other.enableDescriptorAllocator &&
                enableCommandAllocator == other.enableCommandAllocator;
        }

        bool operator!=(const RHIInitConfig& other) const {
            return !(*this == other);
        }
    };
} // namespace StarryEngine::RHI