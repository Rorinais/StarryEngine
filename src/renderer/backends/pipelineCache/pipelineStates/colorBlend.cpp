#include "colorBlend.hpp"

namespace StarryEngine {

        ColorBlend& ColorBlend::addAttachment(const Config& config) {
            VkPipelineColorBlendAttachmentState attachment{};
            attachment.blendEnable = config.blendEnable;
            // 颜色混合参数
            attachment.srcColorBlendFactor = config.srcColorBlendFactor;
            attachment.dstColorBlendFactor = config.dstColorBlendFactor;
            attachment.colorBlendOp = config.colorBlendOp;
            // Alpha混合参数
            attachment.srcAlphaBlendFactor = config.srcAlphaBlendFactor;
            attachment.dstAlphaBlendFactor = config.dstAlphaBlendFactor;
            attachment.alphaBlendOp = config.alphaBlendOp;
            // 写入掩码
            attachment.colorWriteMask = config.colorWriteMask;
            mAttachments.push_back(attachment);
            return *this;
        }

        const VkPipelineColorBlendStateCreateInfo& ColorBlend::getCreateInfo() const {
            static VkPipelineColorBlendStateCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            info.logicOpEnable = VK_FALSE;
            info.logicOp = VK_LOGIC_OP_COPY;
            info.attachmentCount = static_cast<uint32_t>(mAttachments.size());
            info.pAttachments = mAttachments.data();
            info.blendConstants[0] = 0.0f;
            info.blendConstants[1] = 0.0f;
            info.blendConstants[2] = 0.0f;
            info.blendConstants[3] = 0.0f;
            return info;
        }
    
}