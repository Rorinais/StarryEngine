#pragma once
#include "RHI_ENUMS.hpp"
#include "RHI_STRUCTS_BASE.hpp"
#include <functional>
#include <string>
#include <vector>
#include <array>

namespace StarryEngine::RHI {

    // ==================== 帧数据 ====================

    /**
     * @brief 帧数据结构体
     * @details 包含每帧的渲染状态和同步对象
     */
    struct FrameData {
        uint32_t frameIndex = 0;                  ///< 帧索引
        uint32_t imageIndex = 0;                  ///< 交换链图像索引
        CommandBufferHandle commandBuffer;        ///< 命令缓冲区句柄
        SemaphoreHandle imageAvailableSemaphore;  ///< 图像可用信号量
        SemaphoreHandle renderFinishedSemaphore;  ///< 渲染完成信号量
        FenceHandle inFlightFence;                ///< 飞行中栅栏
        float cpuTime = 0.0f;                     ///< CPU时间（毫秒）
        float gpuTime = 0.0f;                     ///< GPU时间（毫秒）
        void* userData = nullptr;                 ///< 用户数据

        bool operator==(const FrameData& other) const {
            return frameIndex == other.frameIndex && imageIndex == other.imageIndex &&
                commandBuffer == other.commandBuffer &&
                imageAvailableSemaphore == other.imageAvailableSemaphore &&
                renderFinishedSemaphore == other.renderFinishedSemaphore &&
                inFlightFence == other.inFlightFence;
        }

        bool operator!=(const FrameData& other) const {
            return !(*this == other);
        }
    };

    // ==================== 回调函数类型 ====================

    /**
     * @brief 帧回调函数类型
     * @details 每帧调用的回调函数
     */
    using FrameCallback = std::function<void(FrameData&)>;

    /**
     * @brief 窗口大小调整回调函数类型
     * @details 窗口大小改变时调用的回调函数
     */
    using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;

    /**
     * @brief 错误回调函数类型
     * @details 发生错误时调用的回调函数
     */
    using ErrorCallback = std::function<void(const std::string& error, bool fatal)>;

    /**
     * @brief 调试回调函数类型
     * @details 接收调试信息的回调函数
     */
    using DebugCallback = std::function<void(
        MessageSeverity severity,
        MessageSource source,
        const std::string& message)>;

} // namespace StarryEngine::RHI