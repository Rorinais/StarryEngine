#pragma once
#include "RHI_ENUMS.hpp"
#include "RHI_STRUCTS_BASE.hpp"
#include <vector>
#include <array>

namespace StarryEngine::RHI {

    // ==================== 拷贝操作相关结构体 ====================

    /**
     * @brief 图像子资源范围结构体
     * @details 描述图像的特定子资源区域
     */
    struct ImageSubresourceRange {
        ImageAspect aspectMask = ImageAspect::Color;  ///< 图像切面掩码
        uint32_t baseMipLevel = 0;                   ///< 基础MIP层级
        uint32_t levelCount = 1;                     ///< MIP层级数量
        uint32_t baseArrayLayer = 0;                 ///< 基础数组层
        uint32_t layerCount = 1;                     ///< 数组层数量

        bool operator==(const ImageSubresourceRange& other) const {
            return aspectMask == other.aspectMask &&
                baseMipLevel == other.baseMipLevel &&
                levelCount == other.levelCount &&
                baseArrayLayer == other.baseArrayLayer &&
                layerCount == other.layerCount;
        }

        bool operator!=(const ImageSubresourceRange& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 缓冲区拷贝区域结构体
     * @details 描述缓冲区到缓冲区的拷贝区域
     */
    struct BufferCopyRegion {
        uint64_t srcOffset = 0;      ///< 源缓冲区偏移
        uint64_t dstOffset = 0;      ///< 目标缓冲区偏移
        uint64_t size = 0;           ///< 拷贝大小

        bool operator==(const BufferCopyRegion& other) const {
            return srcOffset == other.srcOffset &&
                dstOffset == other.dstOffset &&
                size == other.size;
        }

        bool operator!=(const BufferCopyRegion& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 图像拷贝区域结构体
     * @details 描述图像到图像的拷贝区域
     */
    struct ImageCopyRegion {
        ImageSubresourceRange srcSubresource;    ///< 源图像子资源
        Offset3D srcOffset;                      ///< 源图像偏移
        ImageSubresourceRange dstSubresource;    ///< 目标图像子资源
        Offset3D dstOffset;                      ///< 目标图像偏移
        Extent3D extent;                         ///< 拷贝范围

        bool operator==(const ImageCopyRegion& other) const {
            return srcSubresource == other.srcSubresource &&
                srcOffset == other.srcOffset &&
                dstSubresource == other.dstSubresource &&
                dstOffset == other.dstOffset &&
                extent == other.extent;
        }

        bool operator!=(const ImageCopyRegion& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 缓冲区到图像拷贝区域结构体
     * @details 描述缓冲区到图像的拷贝区域
     */
    struct BufferImageCopyRegion {
        uint64_t bufferOffset = 0;               ///< 缓冲区偏移
        uint32_t bufferRowLength = 0;            ///< 缓冲区行长度
        uint32_t bufferImageHeight = 0;          ///< 缓冲区图像高度
        ImageSubresourceRange imageSubresource;  ///< 图像子资源
        Offset3D imageOffset;                    ///< 图像偏移
        Extent3D imageExtent;                    ///< 图像范围

        bool operator==(const BufferImageCopyRegion& other) const {
            return bufferOffset == other.bufferOffset &&
                bufferRowLength == other.bufferRowLength &&
                bufferImageHeight == other.bufferImageHeight &&
                imageSubresource == other.imageSubresource &&
                imageOffset == other.imageOffset &&
                imageExtent == other.imageExtent;
        }

        bool operator!=(const BufferImageCopyRegion& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 图像位块传送区域结构体
     * @details 描述图像的位块传送操作区域
     */
    struct ImageBlitRegion {
        ImageSubresourceRange srcSubresource;        ///< 源图像子资源
        std::array<Offset3D, 2> srcOffsets;          ///< 源图像两个角点偏移
        ImageSubresourceRange dstSubresource;        ///< 目标图像子资源
        std::array<Offset3D, 2> dstOffsets;          ///< 目标图像两个角点偏移

        bool operator==(const ImageBlitRegion& other) const {
            return srcSubresource == other.srcSubresource &&
                srcOffsets == other.srcOffsets &&
                dstSubresource == other.dstSubresource &&
                dstOffsets == other.dstOffsets;
        }

        bool operator!=(const ImageBlitRegion& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 清除附件结构体
     * @details 描述要清除的附件信息
     */
    struct ClearAttachment {
        ImageAspect aspectMask = ImageAspect::Color;  ///< 图像切面掩码
        uint32_t colorAttachment = 0;                 ///< 颜色附件索引
        ClearValue clearValue;                        ///< 清除值

        bool operator==(const ClearAttachment& other) const {
            return aspectMask == other.aspectMask &&
                colorAttachment == other.colorAttachment &&
                clearValue == other.clearValue;
        }

        bool operator!=(const ClearAttachment& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 清除矩形结构体
     * @details 描述要清除的矩形区域
     */
    struct ClearRect {
        Rect2D rect;                       ///< 矩形区域
        uint32_t baseArrayLayer = 0;       ///< 基础数组层
        uint32_t layerCount = 1;           ///< 数组层数量

        bool operator==(const ClearRect& other) const {
            return rect == other.rect &&
                baseArrayLayer == other.baseArrayLayer &&
                layerCount == other.layerCount;
        }

        bool operator!=(const ClearRect& other) const {
            return !(*this == other);
        }
    };

} // namespace StarryEngine::RHI