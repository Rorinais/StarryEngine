#pragma once
#include "RHI_ENUMS.hpp"
#include <string>

namespace StarryEngine::RHI {

    // ==================== 命令缓冲区结构体 ====================

    /**
     * @brief 命令缓冲区描述结构体
     * @details 描述命令缓冲区的属性和行为
     */
    struct CommandBufferDesc {
        CommandBufferLevel level = CommandBufferLevel::Primary; ///< 命令缓冲区级别
        CommandBufferType type = CommandBufferType::Graphics;   ///< 命令缓冲区类型
        bool oneTimeSubmit = true;               ///< 是否为一次性提交
        bool simultaneousUse = false;            ///< 是否支持同时使用
        std::string debugName;                   ///< 调试名称

        bool operator==(const CommandBufferDesc& other) const {
            return level == other.level && type == other.type &&
                oneTimeSubmit == other.oneTimeSubmit &&
                simultaneousUse == other.simultaneousUse;
        }

        bool operator!=(const CommandBufferDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 命令池描述结构体
     * @details 描述命令池的配置
     */
    struct CommandPoolDesc {
        QueueType queueType = QueueType::Graphics; ///< 队列类型
        bool transient = false;                   ///< 短生命周期命令缓冲区
        bool resetCommandBuffer = true;           ///< 允许重置命令缓冲区
        bool protectedMemory = false;             ///< 使用受保护内存
        std::string debugName;                    ///< 调试名称

        bool operator==(const CommandPoolDesc& other) const {
            return queueType == other.queueType && transient == other.transient &&
                resetCommandBuffer == other.resetCommandBuffer &&
                protectedMemory == other.protectedMemory;
        }

        bool operator!=(const CommandPoolDesc& other) const {
            return !(*this == other);
        }
    };

} // namespace StarryEngine::RHI