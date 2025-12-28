#pragma once
#include "RHI_ENUMS.hpp"
#include "RHI_STRUCTS_BASE.hpp"
#include <string>

namespace StarryEngine::RHI {

    // ==================== 查询结构体 ====================

    /**
     * @brief 查询池描述结构体
     * @details 描述查询池的配置
     */
    struct QueryPoolDesc {
        QueryType type = QueryType::Timestamp;      ///< 查询类型
        uint32_t count = 0;                         ///< 查询数量
        std::vector<PipelineStatistic> pipelineStatistics; ///< 管线统计类型
        std::string debugName;                      ///< 调试名称

        bool operator==(const QueryPoolDesc& other) const {
            return type == other.type && count == other.count &&
                pipelineStatistics == other.pipelineStatistics;
        }

        bool operator!=(const QueryPoolDesc& other) const {
            return !(*this == other);
        }
    };

    // ==================== 同步结构体 ====================

    /**
     * @brief 栅栏描述结构体
     * @details 描述GPU-CPU同步栅栏
     */
    struct FenceDesc {
        bool signaled = false;                      ///< 是否已发出信号
        std::string debugName;                      ///< 调试名称

        bool operator==(const FenceDesc& other) const {
            return signaled == other.signaled;
        }

        bool operator!=(const FenceDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 信号量描述结构体
     * @details 描述GPU-GPU同步信号量
     */
    struct SemaphoreDesc {
        std::string debugName;                      ///< 调试名称

        bool operator==(const SemaphoreDesc& other) const {
            return true;  // 所有信号量描述都一样
        }

        bool operator!=(const SemaphoreDesc& other) const {
            return false;
        }
    };

    /**
     * @brief 事件描述结构体
     * @details 描述GPU内部同步事件
     */
    struct EventDesc {
        std::string debugName;                      ///< 调试名称

        bool operator==(const EventDesc& other) const {
            return true;  // 所有事件描述都一样
        }

        bool operator!=(const EventDesc& other) const {
            return false;
        }
    };

    // ==================== 资源屏障结构体 ====================

    /**
     * @brief 缓冲区屏障结构体
     * @details 描述缓冲区访问同步屏障
     */
    struct BufferBarrier {
        void* buffer = nullptr;                     ///< 缓冲区句柄
        AccessFlag srcAccessMask = AccessFlag::None; ///< 源访问掩码
        AccessFlag dstAccessMask = AccessFlag::None; ///< 目标访问掩码
        uint64_t offset = 0;                        ///< 偏移量
        uint64_t size = 0;                          ///< 大小
        uint32_t srcQueueFamilyIndex = 0;           ///< 源队列族索引
        uint32_t dstQueueFamilyIndex = 0;           ///< 目标队列族索引

        bool operator==(const BufferBarrier& other) const {
            return buffer == other.buffer &&
                srcAccessMask == other.srcAccessMask &&
                dstAccessMask == other.dstAccessMask &&
                offset == other.offset && size == other.size &&
                srcQueueFamilyIndex == other.srcQueueFamilyIndex &&
                dstQueueFamilyIndex == other.dstQueueFamilyIndex;
        }

        bool operator!=(const BufferBarrier& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 图像屏障结构体
     * @details 描述图像访问同步屏障
     */
    struct ImageBarrier {
        void* image = nullptr;                      ///< 图像句柄
        ImageLayout oldLayout = ImageLayout::Undefined; ///< 旧布局
        ImageLayout newLayout = ImageLayout::Undefined; ///< 新布局
        AccessFlag srcAccessMask = AccessFlag::None; ///< 源访问掩码
        AccessFlag dstAccessMask = AccessFlag::None; ///< 目标访问掩码
        ImageAspect aspectMask = ImageAspect::Color; ///< 图像切面掩码
        uint32_t srcQueueFamilyIndex = 0;           ///< 源队列族索引
        uint32_t dstQueueFamilyIndex = 0;           ///< 目标队列族索引
        uint32_t baseMipLevel = 0;                  ///< 基础MIP层级
        uint32_t levelCount = 1;                    ///< 层级数量
        uint32_t baseArrayLayer = 0;                ///< 基础数组层
        uint32_t layerCount = 1;                    ///< 层数量

        bool operator==(const ImageBarrier& other) const {
            return image == other.image &&
                oldLayout == other.oldLayout && newLayout == other.newLayout &&
                srcAccessMask == other.srcAccessMask && dstAccessMask == other.dstAccessMask &&
                aspectMask == other.aspectMask &&
                srcQueueFamilyIndex == other.srcQueueFamilyIndex &&
                dstQueueFamilyIndex == other.dstQueueFamilyIndex &&
                baseMipLevel == other.baseMipLevel && levelCount == other.levelCount &&
                baseArrayLayer == other.baseArrayLayer && layerCount == other.layerCount;
        }

        bool operator!=(const ImageBarrier& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 内存屏障结构体
     * @details 描述内存访问同步屏障
     */
    struct MemoryBarrier {
        AccessFlag srcAccessMask = AccessFlag::None; ///< 源访问掩码
        AccessFlag dstAccessMask = AccessFlag::None; ///< 目标访问掩码

        bool operator==(const MemoryBarrier& other) const {
            return srcAccessMask == other.srcAccessMask && dstAccessMask == other.dstAccessMask;
        }

        bool operator!=(const MemoryBarrier& other) const {
            return !(*this == other);
        }
    };

} // namespace StarryEngine::RHI