#include "ColorBlendComponent.hpp"
#include<iostream>
namespace StarryEngine {

    ColorBlendComponent::ColorBlendComponent(const std::string& name) {
        setName(name);
        reset();  // 使用reset初始化默认值
    }
    ColorBlendComponent& ColorBlendComponent::reset() {
        // 设置Vulkan默认的颜色混合状态
        mCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .logicOpEnable = VK_FALSE,                     // 禁用逻辑操作
            .logicOp = VK_LOGIC_OP_COPY,                   // 逻辑操作：拷贝
            .attachmentCount = 0,                          // 附件数量
            .pAttachments = nullptr,                       // 附件状态
            .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}     // 混合常量
        };

        mAttachmentStates.clear();
        mAllAttachmentsSame = false;

        updateCreateInfo();  // 确保调用这个！
        return *this;
    }

    ColorBlendComponent& ColorBlendComponent::enableLogicOp(VkBool32 enable) {
        mCreateInfo.logicOpEnable = enable;
        return *this;
    }

    ColorBlendComponent& ColorBlendComponent::setLogicOp(VkLogicOp logicOp) {
        mCreateInfo.logicOp = logicOp;
        return *this;
    }

    ColorBlendComponent& ColorBlendComponent::setBlendConstants(float r, float g, float b, float a) {
        mCreateInfo.blendConstants[0] = r;
        mCreateInfo.blendConstants[1] = g;
        mCreateInfo.blendConstants[2] = b;
        mCreateInfo.blendConstants[3] = a;
        return *this;
    }

    ColorBlendComponent& ColorBlendComponent::addAttachmentState(const VkPipelineColorBlendAttachmentState& attachment) {
        mAttachmentStates.push_back(attachment);
        updateCreateInfo();  // 确保调用这个！
        return *this;
    }

    ColorBlendComponent& ColorBlendComponent::addAttachmentState(
        VkBool32 blendEnable,
        VkBlendFactor srcColorBlendFactor,
        VkBlendFactor dstColorBlendFactor,
        VkBlendOp colorBlendOp,
        VkBlendFactor srcAlphaBlendFactor,
        VkBlendFactor dstAlphaBlendFactor,
        VkBlendOp alphaBlendOp,
        VkColorComponentFlags colorWriteMask) {

        VkPipelineColorBlendAttachmentState attachment = {
            .blendEnable = blendEnable,
            .srcColorBlendFactor = srcColorBlendFactor,
            .dstColorBlendFactor = dstColorBlendFactor,
            .colorBlendOp = colorBlendOp,
            .srcAlphaBlendFactor = srcAlphaBlendFactor,
            .dstAlphaBlendFactor = dstAlphaBlendFactor,
            .alphaBlendOp = alphaBlendOp,
            .colorWriteMask = colorWriteMask
        };

        return addAttachmentState(attachment);
    }

    ColorBlendComponent& ColorBlendComponent::setAttachmentStates(const std::vector<VkPipelineColorBlendAttachmentState>& attachments) {
        mAttachmentStates = attachments;
        return *this;
    }

    ColorBlendComponent& ColorBlendComponent::addNoBlendingAttachment() {
        // 无混合：直接覆盖，不进行混合
        return addAttachmentState(
            VK_FALSE,                           // blendEnable
            VK_BLEND_FACTOR_ONE,                // srcColorBlendFactor (不使用)
            VK_BLEND_FACTOR_ZERO,               // dstColorBlendFactor (不使用)
            VK_BLEND_OP_ADD,                    // colorBlendOp (不使用)
            VK_BLEND_FACTOR_ONE,                // srcAlphaBlendFactor (不使用)
            VK_BLEND_FACTOR_ZERO,               // dstAlphaBlendFactor (不使用)
            VK_BLEND_OP_ADD,                    // alphaBlendOp (不使用)
            VK_COLOR_COMPONENT_R_BIT |          // colorWriteMask
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT
        );
    }

    ColorBlendComponent& ColorBlendComponent::addAlphaBlendingAttachment() {
        // Alpha混合：常用于透明度混合
        return addAttachmentState(
            VK_TRUE,                            // blendEnable
            VK_BLEND_FACTOR_SRC_ALPHA,          // srcColorBlendFactor
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,// dstColorBlendFactor
            VK_BLEND_OP_ADD,                    // colorBlendOp
            VK_BLEND_FACTOR_ONE,                // srcAlphaBlendFactor
            VK_BLEND_FACTOR_ZERO,               // dstAlphaBlendFactor
            VK_BLEND_OP_ADD,                    // alphaBlendOp
            VK_COLOR_COMPONENT_R_BIT |          // colorWriteMask
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT
        );
    }

    ColorBlendComponent& ColorBlendComponent::addAdditiveBlendingAttachment() {
        // 加法混合：常用于发光效果
        return addAttachmentState(
            VK_TRUE,                            // blendEnable
            VK_BLEND_FACTOR_ONE,                // srcColorBlendFactor
            VK_BLEND_FACTOR_ONE,                // dstColorBlendFactor
            VK_BLEND_OP_ADD,                    // colorBlendOp
            VK_BLEND_FACTOR_ONE,                // srcAlphaBlendFactor
            VK_BLEND_FACTOR_ONE,                // dstAlphaBlendFactor
            VK_BLEND_OP_ADD,                    // alphaBlendOp
            VK_COLOR_COMPONENT_R_BIT |          // colorWriteMask
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT
        );
    }

    ColorBlendComponent& ColorBlendComponent::addMultiplicativeBlendingAttachment() {
        // 乘法混合：常用于阴影效果
        return addAttachmentState(
            VK_TRUE,                            // blendEnable
            VK_BLEND_FACTOR_DST_COLOR,          // srcColorBlendFactor
            VK_BLEND_FACTOR_ZERO,               // dstColorBlendFactor
            VK_BLEND_OP_ADD,                    // colorBlendOp
            VK_BLEND_FACTOR_DST_ALPHA,          // srcAlphaBlendFactor
            VK_BLEND_FACTOR_ZERO,               // dstAlphaBlendFactor
            VK_BLEND_OP_ADD,                    // alphaBlendOp
            VK_COLOR_COMPONENT_R_BIT |          // colorWriteMask
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT
        );
    }

    ColorBlendComponent& ColorBlendComponent::setAllAttachmentsSame(bool same) {
        mAllAttachmentsSame = same;
        return *this;
    }

    std::string ColorBlendComponent::getDescription() const {
        std::string desc = "Color Blend State: ";

        // 逻辑操作
        if (mCreateInfo.logicOpEnable) {
            desc += "LogicOp=ENABLED, Op=";
            switch (mCreateInfo.logicOp) {
            case VK_LOGIC_OP_CLEAR: desc += "CLEAR"; break;
            case VK_LOGIC_OP_AND: desc += "AND"; break;
            case VK_LOGIC_OP_AND_REVERSE: desc += "AND_REVERSE"; break;
            case VK_LOGIC_OP_COPY: desc += "COPY"; break;
            case VK_LOGIC_OP_AND_INVERTED: desc += "AND_INVERTED"; break;
            case VK_LOGIC_OP_NO_OP: desc += "NO_OP"; break;
            case VK_LOGIC_OP_XOR: desc += "XOR"; break;
            case VK_LOGIC_OP_OR: desc += "OR"; break;
            case VK_LOGIC_OP_NOR: desc += "NOR"; break;
            case VK_LOGIC_OP_EQUIVALENT: desc += "EQUIVALENT"; break;
            case VK_LOGIC_OP_INVERT: desc += "INVERT"; break;
            case VK_LOGIC_OP_OR_REVERSE: desc += "OR_REVERSE"; break;
            case VK_LOGIC_OP_COPY_INVERTED: desc += "COPY_INVERTED"; break;
            case VK_LOGIC_OP_OR_INVERTED: desc += "OR_INVERTED"; break;
            case VK_LOGIC_OP_NAND: desc += "NAND"; break;
            case VK_LOGIC_OP_SET: desc += "SET"; break;
            default: desc += "UNKNOWN"; break;
            }
        }
        else {
            desc += "LogicOp=DISABLED";
        }

        // 混合常量
        desc += ", BlendConstants=[" +
            std::to_string(mCreateInfo.blendConstants[0]) + ", " +
            std::to_string(mCreateInfo.blendConstants[1]) + ", " +
            std::to_string(mCreateInfo.blendConstants[2]) + ", " +
            std::to_string(mCreateInfo.blendConstants[3]) + "]";

        // 附件状态
        desc += ", Attachments=" + std::to_string(mAttachmentStates.size());
        if (mAllAttachmentsSame) {
            desc += " (all same)";
        }

        // 如果附件数量较少，可以显示详细信息
        if (!mAttachmentStates.empty() && mAttachmentStates.size() <= 4) {
            for (size_t i = 0; i < mAttachmentStates.size(); ++i) {
                const auto& state = mAttachmentStates[i];
                desc += "\n  Attachment " + std::to_string(i) + ": ";
                desc += state.blendEnable ? "Blend=ENABLED" : "Blend=DISABLED";
                if (state.blendEnable) {
                    desc += ", SrcColor=" + std::to_string(state.srcColorBlendFactor);
                    desc += ", DstColor=" + std::to_string(state.dstColorBlendFactor);
                    desc += ", ColorOp=" + std::to_string(state.colorBlendOp);
                    desc += ", SrcAlpha=" + std::to_string(state.srcAlphaBlendFactor);
                    desc += ", DstAlpha=" + std::to_string(state.dstAlphaBlendFactor);
                    desc += ", AlphaOp=" + std::to_string(state.alphaBlendOp);
                }
                desc += ", WriteMask=0x" + std::to_string(state.colorWriteMask);
            }
        }

        return desc;
    }

    bool ColorBlendComponent::isValid() const {
        // 检查基础结构类型
        if (mCreateInfo.sType != VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO) {
            std::cout << "FAIL: Invalid structure type: " << mCreateInfo.sType
                << " (expected: " << VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO << ")" << std::endl;
            return false;
        }

        // 检查逻辑操作是否有效
        if (mCreateInfo.logicOpEnable) {
            switch (mCreateInfo.logicOp) {
            case VK_LOGIC_OP_CLEAR:
            case VK_LOGIC_OP_AND:
            case VK_LOGIC_OP_AND_REVERSE:
            case VK_LOGIC_OP_COPY:
            case VK_LOGIC_OP_AND_INVERTED:
            case VK_LOGIC_OP_NO_OP:
            case VK_LOGIC_OP_XOR:
            case VK_LOGIC_OP_OR:
            case VK_LOGIC_OP_NOR:
            case VK_LOGIC_OP_EQUIVALENT:
            case VK_LOGIC_OP_INVERT:
            case VK_LOGIC_OP_OR_REVERSE:
            case VK_LOGIC_OP_COPY_INVERTED:
            case VK_LOGIC_OP_OR_INVERTED:
            case VK_LOGIC_OP_NAND:
            case VK_LOGIC_OP_SET:
                std::cout << "PASS: Logic operation is valid" << std::endl;
                break;  // 有效的逻辑操作
            default:
                std::cout << "FAIL: Invalid logic operation: " << static_cast<int>(mCreateInfo.logicOp) << std::endl;
                return false;  // 无效的逻辑操作
            }
        }
        else {
            std::cout << "PASS: Logic operation is disabled" << std::endl;
        }

        // 检查混合常量是否在有效范围内
        bool blendConstantsValid = true;
        for (int i = 0; i < 4; ++i) {
            if (mCreateInfo.blendConstants[i] < 0.0f || mCreateInfo.blendConstants[i] > 1.0f) {
                std::cout << "FAIL: Blend constant[" << i << "] = " << mCreateInfo.blendConstants[i]
                    << " is out of range [0.0, 1.0]" << std::endl;
                blendConstantsValid = false;
            }
        }
        if (blendConstantsValid) {
            std::cout << "PASS: All blend constants are in valid range" << std::endl;
        }
        else {
            return false;
        }

        // 检查附件状态的有效性
        for (size_t i = 0; i < mAttachmentStates.size(); ++i) {
            const auto& state = mAttachmentStates[i];
            std::cout << "  Attachment " << i << ": blendEnable=" << state.blendEnable
                << ", colorWriteMask=0x" << std::hex << state.colorWriteMask << std::dec << std::endl;

            if (!isColorBlendAttachmentValid(state)) {
                std::cout << "FAIL: Attachment " << i << " is invalid" << std::endl;
                return false;
            }
            std::cout << "  PASS: Attachment " << i << " is valid" << std::endl;
        }

        if (mAttachmentStates.empty()) {
            if (mCreateInfo.attachmentCount != 0 || mCreateInfo.pAttachments != nullptr) {
                std::cout << "FAIL: Empty attachment states but create info has non-zero count or non-null pointer" << std::endl;
                return false;
            }
            std::cout << "PASS: Empty attachment states handled correctly" << std::endl;
        }
        else {
            if (mCreateInfo.attachmentCount != mAttachmentStates.size() ||
                mCreateInfo.pAttachments != mAttachmentStates.data()) {
                std::cout << "FAIL: Attachment count or pointer mismatch!" << std::endl;
                std::cout << "  Expected count: " << mAttachmentStates.size()
                    << ", Actual count: " << mCreateInfo.attachmentCount << std::endl;
                std::cout << "  Expected pointer: " << (void*)mAttachmentStates.data()
                    << ", Actual pointer: " << (void*)mCreateInfo.pAttachments << std::endl;
                return false;
            }
            std::cout << "PASS: Attachment count and pointer are consistent" << std::endl;
        }
        return true;
    }

    void ColorBlendComponent::updateCreateInfo() {
        // 确保结构体类型正确
        mCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        mCreateInfo.pNext = nullptr;

        // 更新附件状态指针和数量
        mCreateInfo.attachmentCount = static_cast<uint32_t>(mAttachmentStates.size());
        mCreateInfo.pAttachments = mAttachmentStates.empty() ? nullptr : mAttachmentStates.data();
    }

    // 辅助函数：检查颜色混合附件状态是否有效
    bool ColorBlendComponent::isColorBlendAttachmentValid(const VkPipelineColorBlendAttachmentState& state) const {
        // 检查混合因子是否有效
        if (!isBlendFactorValid(state.srcColorBlendFactor) ||
            !isBlendFactorValid(state.dstColorBlendFactor) ||
            !isBlendFactorValid(state.srcAlphaBlendFactor) ||
            !isBlendFactorValid(state.dstAlphaBlendFactor)) {
            return false;
        }

        // 检查混合操作是否有效
        if (!isBlendOpValid(state.colorBlendOp) ||
            !isBlendOpValid(state.alphaBlendOp)) {
            return false;
        }

        // 检查颜色写入掩码是否有效
        VkColorComponentFlags validMask = VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        if ((state.colorWriteMask & ~validMask) != 0) {
            return false;
        }

        return true;
    }

    // 辅助函数：检查混合因子是否有效
    bool ColorBlendComponent::isBlendFactorValid(VkBlendFactor factor) const {
        switch (factor) {
        case VK_BLEND_FACTOR_ZERO:
        case VK_BLEND_FACTOR_ONE:
        case VK_BLEND_FACTOR_SRC_COLOR:
        case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
        case VK_BLEND_FACTOR_DST_COLOR:
        case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
        case VK_BLEND_FACTOR_SRC_ALPHA:
        case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
        case VK_BLEND_FACTOR_DST_ALPHA:
        case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
        case VK_BLEND_FACTOR_CONSTANT_COLOR:
        case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
        case VK_BLEND_FACTOR_CONSTANT_ALPHA:
        case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
        case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
        case VK_BLEND_FACTOR_SRC1_COLOR:
        case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
        case VK_BLEND_FACTOR_SRC1_ALPHA:
        case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
            return true;
        default:
            return false;
        }
    }

    // 辅助函数：检查混合操作是否有效
    bool ColorBlendComponent::isBlendOpValid(VkBlendOp op) const {
        switch (op) {
        case VK_BLEND_OP_ADD:
        case VK_BLEND_OP_SUBTRACT:
        case VK_BLEND_OP_REVERSE_SUBTRACT:
        case VK_BLEND_OP_MIN:
        case VK_BLEND_OP_MAX:
            return true;
            // 检查扩展的混合操作
#ifdef VK_EXT_blend_operation_advanced
        case VK_BLEND_OP_ZERO_EXT:
        case VK_BLEND_OP_SRC_EXT:
        case VK_BLEND_OP_DST_EXT:
        case VK_BLEND_OP_SRC_OVER_EXT:
        case VK_BLEND_OP_DST_OVER_EXT:
        case VK_BLEND_OP_SRC_IN_EXT:
        case VK_BLEND_OP_DST_IN_EXT:
        case VK_BLEND_OP_SRC_OUT_EXT:
        case VK_BLEND_OP_DST_OUT_EXT:
        case VK_BLEND_OP_SRC_ATOP_EXT:
        case VK_BLEND_OP_DST_ATOP_EXT:
        case VK_BLEND_OP_XOR_EXT:
        case VK_BLEND_OP_MULTIPLY_EXT:
        case VK_BLEND_OP_SCREEN_EXT:
        case VK_BLEND_OP_OVERLAY_EXT:
        case VK_BLEND_OP_DARKEN_EXT:
        case VK_BLEND_OP_LIGHTEN_EXT:
        case VK_BLEND_OP_COLORDODGE_EXT:
        case VK_BLEND_OP_COLORBURN_EXT:
        case VK_BLEND_OP_HARDLIGHT_EXT:
        case VK_BLEND_OP_SOFTLIGHT_EXT:
        case VK_BLEND_OP_DIFFERENCE_EXT:
        case VK_BLEND_OP_EXCLUSION_EXT:
        case VK_BLEND_OP_INVERT_EXT:
        case VK_BLEND_OP_INVERT_RGB_EXT:
        case VK_BLEND_OP_LINEARDODGE_EXT:
        case VK_BLEND_OP_LINEARBURN_EXT:
        case VK_BLEND_OP_VIVIDLIGHT_EXT:
        case VK_BLEND_OP_LINEARLIGHT_EXT:
        case VK_BLEND_OP_PINLIGHT_EXT:
        case VK_BLEND_OP_HARDMIX_EXT:
        case VK_BLEND_OP_HSL_HUE_EXT:
        case VK_BLEND_OP_HSL_SATURATION_EXT:
        case VK_BLEND_OP_HSL_COLOR_EXT:
        case VK_BLEND_OP_HSL_LUMINOSITY_EXT:
        case VK_BLEND_OP_PLUS_EXT:
        case VK_BLEND_OP_PLUS_CLAMPED_EXT:
        case VK_BLEND_OP_PLUS_CLAMPED_ALPHA_EXT:
        case VK_BLEND_OP_PLUS_DARKER_EXT:
        case VK_BLEND_OP_MINUS_EXT:
        case VK_BLEND_OP_MINUS_CLAMPED_EXT:
        case VK_BLEND_OP_CONTRAST_EXT:
        case VK_BLEND_OP_INVERT_OVG_EXT:
        case VK_BLEND_OP_RED_EXT:
        case VK_BLEND_OP_GREEN_EXT:
        case VK_BLEND_OP_BLUE_EXT:
            // 这些操作需要 VK_EXT_blend_operation_advanced 扩展
            return true;
#endif
        default:
            return false;
        }
    }

} // namespace StarryEngine