#pragma once
#include "RHI_ENUMS.hpp"
#include "RHI_STRUCTS_BASE.hpp"
#include <vector>
#include <string>

namespace StarryEngine::RHI {

    // ==================== 渲染通道结构体 ====================

    /**
     * @brief 附件描述结构体
     * @details 描述渲染通道中的单个附件
     */
    struct AttachmentDesc {
        Format format = Format::Undefined;         ///< 附件格式
        uint32_t sampleCount = 1;                  ///< 采样数
        AttachmentLoadOp loadOp = AttachmentLoadOp::DontCare; ///< 加载操作
        AttachmentStoreOp storeOp = AttachmentStoreOp::DontCare; ///< 存储操作
        AttachmentLoadOp stencilLoadOp = AttachmentLoadOp::DontCare; ///< 模板加载操作
        AttachmentStoreOp stencilStoreOp = AttachmentStoreOp::DontCare; ///< 模板存储操作
        ImageLayout initialLayout = ImageLayout::Undefined; ///< 初始布局
        ImageLayout finalLayout = ImageLayout::Undefined;   ///< 最终布局

        bool operator==(const AttachmentDesc& other) const {
            return format == other.format && sampleCount == other.sampleCount &&
                loadOp == other.loadOp && storeOp == other.storeOp &&
                stencilLoadOp == other.stencilLoadOp && stencilStoreOp == other.stencilStoreOp &&
                initialLayout == other.initialLayout && finalLayout == other.finalLayout;
        }

        bool operator!=(const AttachmentDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 附件引用结构体
     * @details 引用渲染通道中的特定附件
     */
    struct AttachmentReference {
        uint32_t attachment = 0;                     ///< 附件索引
        ImageLayout layout = ImageLayout::Undefined; ///< 附件布局

        bool operator==(const AttachmentReference& other) const {
            return attachment == other.attachment && layout == other.layout;
        }

        bool operator!=(const AttachmentReference& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 子通道描述结构体
     * @details 描述渲染通道中的一个子通道
     */
    struct SubpassDesc {
        std::vector<AttachmentReference> inputAttachments;       ///< 输入附件
        std::vector<AttachmentReference> colorAttachments;       ///< 颜色附件
        std::vector<AttachmentReference> resolveAttachments;     ///< 解析附件
        AttachmentReference depthStencilAttachment;              ///< 深度模板附件
        std::vector<uint32_t> preserveAttachments;               ///< 保留附件

        bool operator==(const SubpassDesc& other) const {
            return inputAttachments == other.inputAttachments &&
                colorAttachments == other.colorAttachments &&
                resolveAttachments == other.resolveAttachments &&
                depthStencilAttachment == other.depthStencilAttachment &&
                preserveAttachments == other.preserveAttachments;
        }

        bool operator!=(const SubpassDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 子通道依赖结构体
     * @details 描述子通道之间的执行依赖关系
     */
    struct SubpassDependency {
        uint32_t srcSubpass = 0;                     ///< 源子通道索引
        uint32_t dstSubpass = 0;                     ///< 目标子通道索引
        PipelineStage srcStageMask = PipelineStage::TopOfPipe; ///< 源阶段掩码
        PipelineStage dstStageMask = PipelineStage::BottomOfPipe; ///< 目标阶段掩码
        AccessFlag srcAccessMask = AccessFlag::None; ///< 源访问掩码
        AccessFlag dstAccessMask = AccessFlag::None; ///< 目标访问掩码
        bool byRegion = false;                       ///< 是否为区域依赖

        bool operator==(const SubpassDependency& other) const {
            return srcSubpass == other.srcSubpass && dstSubpass == other.dstSubpass &&
                srcStageMask == other.srcStageMask && dstStageMask == other.dstStageMask &&
                srcAccessMask == other.srcAccessMask && dstAccessMask == other.dstAccessMask &&
                byRegion == other.byRegion;
        }

        bool operator!=(const SubpassDependency& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 渲染通道描述结构体
     * @details 描述完整的渲染通道配置
     */
    struct RenderPassDesc {
        std::vector<AttachmentDesc> attachments;    ///< 附件列表
        std::vector<SubpassDesc> subpasses;         ///< 子通道列表
        std::vector<SubpassDependency> dependencies; ///< 依赖关系
        std::string debugName;                      ///< 调试名称

        bool operator==(const RenderPassDesc& other) const {
            return attachments == other.attachments &&
                subpasses == other.subpasses &&
                dependencies == other.dependencies;
        }

        bool operator!=(const RenderPassDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 帧缓冲描述结构体
     * @details 描述帧缓冲的配置
     */
    struct FramebufferDesc {
        void* renderPass = nullptr;                 ///< 渲染通道句柄
        std::vector<void*> attachments;             ///< 附件句柄列表
        Extent2D extent;                            ///< 尺寸
        uint32_t layers = 1;                        ///< 层数
        std::string debugName;                      ///< 调试名称

        bool operator==(const FramebufferDesc& other) const {
            return extent == other.extent && layers == other.layers;
        }

        bool operator!=(const FramebufferDesc& other) const {
            return !(*this == other);
        }
    };

} // namespace StarryEngine::RHI