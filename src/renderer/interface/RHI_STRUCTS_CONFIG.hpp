#pragma once
#include "RHI_ENUMS.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <functional>

namespace StarryEngine::RHI {

    // ==================== 配置结构体 ====================

    /**
     * @brief RHI初始化配置结构体
     * @details 包含渲染API初始化所需的所有配置参数
     */
    struct RHIInitConfig {
        API api = API::Vulkan;                    ///< 使用的API
        FeatureLevel featureLevel = FeatureLevel::VK_1_2; ///< 功能级别
        bool enableDebug = true;                  ///< 启用调试
        bool enableGPUValidation = false;         ///< 启用GPU验证
        bool enableRenderDoc = false;             ///< 启用RenderDoc集成
        bool enableNsight = false;                ///< 启用Nsight集成
        bool enableAftermath = false;             ///< 启用Aftermath集成
        uint32_t frameBuffering = 2;              ///< 帧缓冲数量
        bool enableMultiThreading = true;         ///< 启用多线程
        uint32_t maxThreadCount = 4;              ///< 最大线程数
        bool enablePipelineCache = true;          ///< 启用管线缓存
        std::string pipelineCacheFile = "pipeline_cache.bin"; ///< 管线缓存文件
        bool enableShaderCache = true;            ///< 启用着色器缓存
        std::string shaderCacheDir = "shader_cache"; ///< 着色器缓存目录
        bool enableMemoryAllocator = true;        ///< 启用内存分配器
        bool enableDescriptorAllocator = true;    ///< 启用描述符分配器
        bool enableCommandAllocator = true;       ///< 启用命令分配器

        /// @brief 窗口配置
        struct Window {
            void* handle = nullptr;               ///< 窗口句柄
            uint32_t width = 1280;                ///< 窗口宽度
            uint32_t height = 720;                ///< 窗口高度
            std::string title = "StarryEngine Application"; ///< 窗口标题
            bool fullscreen = false;              ///< 是否全屏
            bool borderless = false;              ///< 是否无边框
            bool resizable = true;                ///< 是否可调整大小
            bool vsync = true;                    ///< 是否启用垂直同步
            uint32_t swapChainImages = 2;         ///< 交换链图像数量
            bool srgb = true;                     ///< 是否启用sRGB
            bool hdr = false;                     ///< 是否启用HDR
            float refreshRate = 60.0f;            ///< 刷新率
        } window;

        /// @brief 设备功能配置
        struct Features {
            bool geometryShader = false;          ///< 几何着色器
            bool tessellationShader = false;      ///< 细分着色器
            bool meshShader = false;              ///< 网格着色器
            bool taskShader = false;              ///< 任务着色器
            bool rayTracing = false;              ///< 光线追踪
            bool variableRateShading = false;     ///< 可变速率着色
            bool conservativeRasterization = false; ///< 保守光栅化
            bool samplerAnisotropy = true;        ///< 采样器各向异性
            bool textureCompression = true;       ///< 纹理压缩
            bool computeShader = true;            ///< 计算着色器
            bool shaderFloat64 = false;           ///< 64位浮点着色器
            bool shaderInt64 = false;             ///< 64位整型着色器
            bool shaderInt16 = false;             ///< 16位整型着色器
            bool shaderInt8 = false;              ///< 8位整型着色器
            bool shaderFloat16 = false;           ///< 16位浮点着色器
            bool shaderDemoteToHelper = false;    ///< 降级到辅助着色器
            bool shaderTerminateInvocation = false; ///< 终止调用
            bool subgroupOperations = false;      ///< 子组操作
            bool subgroupSizeControl = false;     ///< 子组大小控制
            bool computeFullSubgroups = false;    ///< 完整计算子组
            bool synchronization2 = true;         ///< 同步2
            bool dynamicRendering = true;         ///< 动态渲染
            bool shaderObject = false;            ///< 着色器对象
            bool descriptorBuffer = false;        ///< 描述符缓冲区
            bool bufferDeviceAddress = false;     ///< 缓冲区设备地址
        } features;

        bool operator==(const RHIInitConfig& other) const {
            return api == other.api && featureLevel == other.featureLevel &&
                enableDebug == other.enableDebug &&
                enableGPUValidation == other.enableGPUValidation &&
                enableRenderDoc == other.enableRenderDoc &&
                enableNsight == other.enableNsight &&
                enableAftermath == other.enableAftermath &&
                frameBuffering == other.frameBuffering &&
                enableMultiThreading == other.enableMultiThreading &&
                maxThreadCount == other.maxThreadCount &&
                enablePipelineCache == other.enablePipelineCache &&
                pipelineCacheFile == other.pipelineCacheFile &&
                enableShaderCache == other.enableShaderCache &&
                shaderCacheDir == other.shaderCacheDir &&
                enableMemoryAllocator == other.enableMemoryAllocator &&
                enableDescriptorAllocator == other.enableDescriptorAllocator &&
                enableCommandAllocator == other.enableCommandAllocator &&
                window.width == other.window.width && window.height == other.window.height &&
                window.title == other.window.title && window.fullscreen == other.window.fullscreen &&
                window.borderless == other.window.borderless && window.resizable == other.window.resizable &&
                window.vsync == other.window.vsync && window.swapChainImages == other.window.swapChainImages &&
                window.srgb == other.window.srgb && window.hdr == other.window.hdr &&
                window.refreshRate == other.window.refreshRate;
        }

        bool operator!=(const RHIInitConfig& other) const {
            return !(*this == other);
        }
    };

    // ==================== 句柄类型 ====================
    // 注意：以下句柄类型使用64位ID标识，支持跨API资源引用

    struct BufferHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const BufferHandle& other) const { return id == other.id; } constexpr bool operator!=(const BufferHandle& other) const { return id != other.id; } constexpr bool operator<(const BufferHandle& other) const { return id < other.id; } };
    struct TextureHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const TextureHandle& other) const { return id == other.id; } constexpr bool operator!=(const TextureHandle& other) const { return id != other.id; } constexpr bool operator<(const TextureHandle& other) const { return id < other.id; } };
    struct SamplerHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const SamplerHandle& other) const { return id == other.id; } constexpr bool operator!=(const SamplerHandle& other) const { return id != other.id; } };
    struct ShaderModuleHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const ShaderModuleHandle& other) const { return id == other.id; } constexpr bool operator!=(const ShaderModuleHandle& other) const { return id != other.id; } };
    struct PipelineLayoutHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const PipelineLayoutHandle& other) const { return id == other.id; } constexpr bool operator!=(const PipelineLayoutHandle& other) const { return id != other.id; } };
    struct PipelineHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const PipelineHandle& other) const { return id == other.id; } constexpr bool operator!=(const PipelineHandle& other) const { return id != other.id; } };
    struct RenderPassHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const RenderPassHandle& other) const { return id == other.id; } constexpr bool operator!=(const RenderPassHandle& other) const { return id != other.id; } };
    struct FramebufferHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const FramebufferHandle& other) const { return id == other.id; } constexpr bool operator!=(const FramebufferHandle& other) const { return id != other.id; } };
    struct CommandPoolHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const CommandPoolHandle& other) const { return id == other.id; } constexpr bool operator!=(const CommandPoolHandle& other) const { return id != other.id; } };
    struct CommandBufferHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const CommandBufferHandle& other) const { return id == other.id; } constexpr bool operator!=(const CommandBufferHandle& other) const { return id != other.id; } };
    struct FenceHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const FenceHandle& other) const { return id == other.id; } constexpr bool operator!=(const FenceHandle& other) const { return id != other.id; } };
    struct SemaphoreHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const SemaphoreHandle& other) const { return id == other.id; } constexpr bool operator!=(const SemaphoreHandle& other) const { return id != other.id; } };
    struct EventHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const EventHandle& other) const { return id == other.id; } constexpr bool operator!=(const EventHandle& other) const { return id != other.id; } };
    struct QueryPoolHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const QueryPoolHandle& other) const { return id == other.id; } constexpr bool operator!=(const QueryPoolHandle& other) const { return id != other.id; } };
    struct AccelerationStructureHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const AccelerationStructureHandle& other) const { return id == other.id; } constexpr bool operator!=(const AccelerationStructureHandle& other) const { return id != other.id; } };
    struct DescriptorSetLayoutHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const DescriptorSetLayoutHandle& other) const { return id == other.id; } constexpr bool operator!=(const DescriptorSetLayoutHandle& other) const { return id != other.id; } };
    struct DescriptorSetHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const DescriptorSetHandle& other) const { return id == other.id; } constexpr bool operator!=(const DescriptorSetHandle& other) const { return id != other.id; } };
    struct DescriptorPoolHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const DescriptorPoolHandle& other) const { return id == other.id; } constexpr bool operator!=(const DescriptorPoolHandle& other) const { return id != other.id; } };
    struct SwapChainHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const SwapChainHandle& other) const { return id == other.id; } constexpr bool operator!=(const SwapChainHandle& other) const { return id != other.id; } };
    struct QueueHandle { uint64_t id = 0; constexpr bool isValid() const { return id != 0; } constexpr bool operator==(const QueueHandle& other) const { return id == other.id; } constexpr bool operator!=(const QueueHandle& other) const { return id != other.id; } };

} // namespace StarryEngine::RHI