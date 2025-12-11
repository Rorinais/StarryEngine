#include "DepthStencilComponent.hpp"

namespace StarryEngine {

    DepthStencilComponent::DepthStencilComponent(const std::string& name) {
        setName(name);
        reset();  // 使用reset初始化默认值
    }

    DepthStencilComponent& DepthStencilComponent::reset() {
        // 设置Vulkan默认的深度模板状态
        mCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthTestEnable = VK_TRUE,                     // 启用深度测试
            .depthWriteEnable = VK_TRUE,                    // 启用深度写入
            .depthCompareOp = VK_COMPARE_OP_LESS,           // 小于则通过（默认）
            .depthBoundsTestEnable = VK_FALSE,              // 禁用深度边界测试
            .stencilTestEnable = VK_FALSE,                  // 禁用模板测试
            .front = {
                .failOp = VK_STENCIL_OP_KEEP,              // 前向面失败操作：保持
                .passOp = VK_STENCIL_OP_KEEP,              // 前向面通过操作：保持
                .depthFailOp = VK_STENCIL_OP_KEEP,         // 前向面深度失败操作：保持
                .compareOp = VK_COMPARE_OP_ALWAYS,         // 前向面比较操作：总是通过
                .compareMask = 0xFF,                       // 前向面比较掩码
                .writeMask = 0xFF,                         // 前向面写入掩码
                .reference = 0                             // 前向面参考值
            },
            .back = {
                .failOp = VK_STENCIL_OP_KEEP,              // 后向面失败操作：保持
                .passOp = VK_STENCIL_OP_KEEP,              // 后向面通过操作：保持
                .depthFailOp = VK_STENCIL_OP_KEEP,         // 后向面深度失败操作：保持
                .compareOp = VK_COMPARE_OP_ALWAYS,         // 后向面比较操作：总是通过
                .compareMask = 0xFF,                       // 后向面比较掩码
                .writeMask = 0xFF,                         // 后向面写入掩码
                .reference = 0                             // 后向面参考值
            },
            .minDepthBounds = 0.0f,                        // 最小深度边界
            .maxDepthBounds = 1.0f                         // 最大深度边界
        };
        return *this;
    }

    DepthStencilComponent& DepthStencilComponent::enableDepthTest(VkBool32 enable) {
        mCreateInfo.depthTestEnable = enable;
        return *this;
    }

    DepthStencilComponent& DepthStencilComponent::enableDepthWrite(VkBool32 enable) {
        mCreateInfo.depthWriteEnable = enable;
        return *this;
    }

    DepthStencilComponent& DepthStencilComponent::setDepthCompareOp(VkCompareOp compareOp) {
        mCreateInfo.depthCompareOp = compareOp;
        return *this;
    }

    DepthStencilComponent& DepthStencilComponent::enableDepthBoundsTest(VkBool32 enable) {
        mCreateInfo.depthBoundsTestEnable = enable;
        return *this;
    }

    DepthStencilComponent& DepthStencilComponent::setDepthBounds(float minDepthBounds, float maxDepthBounds) {
        mCreateInfo.minDepthBounds = minDepthBounds;
        mCreateInfo.maxDepthBounds = maxDepthBounds;
        return *this;
    }

    DepthStencilComponent& DepthStencilComponent::enableStencilTest(VkBool32 enable) {
        mCreateInfo.stencilTestEnable = enable;
        return *this;
    }

    DepthStencilComponent& DepthStencilComponent::setFrontStencilOpState(
        VkStencilOp failOp,
        VkStencilOp passOp,
        VkStencilOp depthFailOp,
        VkCompareOp compareOp,
        uint32_t compareMask,
        uint32_t writeMask,
        uint32_t reference) {

        mCreateInfo.front = {
            .failOp = failOp,
            .passOp = passOp,
            .depthFailOp = depthFailOp,
            .compareOp = compareOp,
            .compareMask = compareMask,
            .writeMask = writeMask,
            .reference = reference
        };

        return *this;
    }

    DepthStencilComponent& DepthStencilComponent::setBackStencilOpState(
        VkStencilOp failOp,
        VkStencilOp passOp,
        VkStencilOp depthFailOp,
        VkCompareOp compareOp,
        uint32_t compareMask,
        uint32_t writeMask,
        uint32_t reference) {

        mCreateInfo.back = {
            .failOp = failOp,
            .passOp = passOp,
            .depthFailOp = depthFailOp,
            .compareOp = compareOp,
            .compareMask = compareMask,
            .writeMask = writeMask,
            .reference = reference
        };

        return *this;
    }

    DepthStencilComponent& DepthStencilComponent::setStencilOpState(
        VkStencilOp failOp,
        VkStencilOp passOp,
        VkStencilOp depthFailOp,
        VkCompareOp compareOp,
        uint32_t compareMask,
        uint32_t writeMask,
        uint32_t reference) {

        setFrontStencilOpState(failOp, passOp, depthFailOp, compareOp, compareMask, writeMask, reference);
        setBackStencilOpState(failOp, passOp, depthFailOp, compareOp, compareMask, writeMask, reference);
        return *this;
    }

    DepthStencilComponent& DepthStencilComponent::enableStandardDepthTest() {
        enableDepthTest(VK_TRUE);
        enableDepthWrite(VK_TRUE);
        setDepthCompareOp(VK_COMPARE_OP_LESS);
        enableDepthBoundsTest(VK_FALSE);
        return *this;
    }

    DepthStencilComponent& DepthStencilComponent::enableDepthTestOnly() {
        enableDepthTest(VK_TRUE);
        enableDepthWrite(VK_FALSE);
        setDepthCompareOp(VK_COMPARE_OP_LESS);
        return *this;
    }

    DepthStencilComponent& DepthStencilComponent::disableDepthTest() {
        enableDepthTest(VK_FALSE);
        enableDepthWrite(VK_FALSE);
        enableDepthBoundsTest(VK_FALSE);
        return *this;
    }

    DepthStencilComponent& DepthStencilComponent::enableStandardStencilTest() {
        enableStencilTest(VK_TRUE);
        setStencilOpState(
            VK_STENCIL_OP_KEEP,      // failOp
            VK_STENCIL_OP_REPLACE,   // passOp
            VK_STENCIL_OP_KEEP,      // depthFailOp
            VK_COMPARE_OP_ALWAYS,    // compareOp
            0xFF,                    // compareMask
            0xFF,                    // writeMask
            1                        // reference
        );
        return *this;
    }

    DepthStencilComponent& DepthStencilComponent::disableStencilTest() {
        enableStencilTest(VK_FALSE);
        return *this;
    }

    std::string DepthStencilComponent::getDescription() const {
        std::string desc = "Depth Stencil State: ";

        // 深度测试
        if (mCreateInfo.depthTestEnable) {
            desc += "DepthTest=ENABLED";
            desc += ", DepthWrite=" + std::string(mCreateInfo.depthWriteEnable ? "ENABLED" : "DISABLED");

            // 深度比较操作
            desc += ", DepthCompareOp=";
            switch (mCreateInfo.depthCompareOp) {
            case VK_COMPARE_OP_NEVER: desc += "NEVER"; break;
            case VK_COMPARE_OP_LESS: desc += "LESS"; break;
            case VK_COMPARE_OP_EQUAL: desc += "EQUAL"; break;
            case VK_COMPARE_OP_LESS_OR_EQUAL: desc += "LESS_OR_EQUAL"; break;
            case VK_COMPARE_OP_GREATER: desc += "GREATER"; break;
            case VK_COMPARE_OP_NOT_EQUAL: desc += "NOT_EQUAL"; break;
            case VK_COMPARE_OP_GREATER_OR_EQUAL: desc += "GREATER_OR_EQUAL"; break;
            case VK_COMPARE_OP_ALWAYS: desc += "ALWAYS"; break;
            default: desc += "UNKNOWN"; break;
            }

            if (mCreateInfo.depthBoundsTestEnable) {
                desc += ", DepthBounds=[" + std::to_string(mCreateInfo.minDepthBounds) +
                    ", " + std::to_string(mCreateInfo.maxDepthBounds) + "]";
            }
        }
        else {
            desc += "DepthTest=DISABLED";
        }

        // 模板测试
        if (mCreateInfo.stencilTestEnable) {
            desc += ", StencilTest=ENABLED";

            // 前向面模板状态
            desc += ", Front={";
            desc += "failOp=" + std::to_string(mCreateInfo.front.failOp);
            desc += ", passOp=" + std::to_string(mCreateInfo.front.passOp);
            desc += ", depthFailOp=" + std::to_string(mCreateInfo.front.depthFailOp);
            desc += ", compareOp=" + std::to_string(mCreateInfo.front.compareOp);
            desc += "}";

            // 后向面模板状态
            desc += ", Back={";
            desc += "failOp=" + std::to_string(mCreateInfo.back.failOp);
            desc += ", passOp=" + std::to_string(mCreateInfo.back.passOp);
            desc += ", depthFailOp=" + std::to_string(mCreateInfo.back.depthFailOp);
            desc += ", compareOp=" + std::to_string(mCreateInfo.back.compareOp);
            desc += "}";
        }
        else {
            desc += ", StencilTest=DISABLED";
        }

        return desc;
    }

    bool DepthStencilComponent::isValid() const {
        // 检查基础结构类型
        if (mCreateInfo.sType != VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO) {
            return false;
        }

        // 检查深度比较操作是否有效
        switch (mCreateInfo.depthCompareOp) {
        case VK_COMPARE_OP_NEVER:
        case VK_COMPARE_OP_LESS:
        case VK_COMPARE_OP_EQUAL:
        case VK_COMPARE_OP_LESS_OR_EQUAL:
        case VK_COMPARE_OP_GREATER:
        case VK_COMPARE_OP_NOT_EQUAL:
        case VK_COMPARE_OP_GREATER_OR_EQUAL:
        case VK_COMPARE_OP_ALWAYS:
            break;  // 有效的比较操作
        default:
            return false;  // 无效的比较操作
        }

        // 如果启用了深度边界测试，检查边界值是否有效
        if (mCreateInfo.depthBoundsTestEnable) {
            if (!areDepthBoundsValid()) {
                return false;
            }
        }

        // 检查模板操作是否有效
        if (mCreateInfo.stencilTestEnable) {
            // 检查前向面模板操作
            if (!isStencilOpValid(mCreateInfo.front.failOp) ||
                !isStencilOpValid(mCreateInfo.front.passOp) ||
                !isStencilOpValid(mCreateInfo.front.depthFailOp)) {
                return false;
            }

            // 检查前向面比较操作
            if (!isCompareOpValid(mCreateInfo.front.compareOp)) {
                return false;
            }

            // 检查后向面模板操作
            if (!isStencilOpValid(mCreateInfo.back.failOp) ||
                !isStencilOpValid(mCreateInfo.back.passOp) ||
                !isStencilOpValid(mCreateInfo.back.depthFailOp)) {
                return false;
            }

            // 检查后向面比较操作
            if (!isCompareOpValid(mCreateInfo.back.compareOp)) {
                return false;
            }
        }

        return true;
    }

    void DepthStencilComponent::updateCreateInfo() {
        // 确保结构体类型正确
        mCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        mCreateInfo.pNext = nullptr;
    }

    bool DepthStencilComponent::areDepthBoundsValid() const {
        // 深度边界应该在[0.0, 1.0]范围内，且最小值<=最大值
        return (mCreateInfo.minDepthBounds >= 0.0f &&
            mCreateInfo.maxDepthBounds <= 1.0f &&
            mCreateInfo.minDepthBounds <= mCreateInfo.maxDepthBounds);
    }

    // 辅助函数：检查模板操作是否有效
    bool DepthStencilComponent::isStencilOpValid(VkStencilOp op) const {
        switch (op) {
        case VK_STENCIL_OP_KEEP:
        case VK_STENCIL_OP_ZERO:
        case VK_STENCIL_OP_REPLACE:
        case VK_STENCIL_OP_INCREMENT_AND_CLAMP:
        case VK_STENCIL_OP_DECREMENT_AND_CLAMP:
        case VK_STENCIL_OP_INVERT:
        case VK_STENCIL_OP_INCREMENT_AND_WRAP:
        case VK_STENCIL_OP_DECREMENT_AND_WRAP:
            return true;
        default:
            return false;
        }
    }

    // 辅助函数：检查比较操作是否有效
    bool DepthStencilComponent::isCompareOpValid(VkCompareOp op) const {
        switch (op) {
        case VK_COMPARE_OP_NEVER:
        case VK_COMPARE_OP_LESS:
        case VK_COMPARE_OP_EQUAL:
        case VK_COMPARE_OP_LESS_OR_EQUAL:
        case VK_COMPARE_OP_GREATER:
        case VK_COMPARE_OP_NOT_EQUAL:
        case VK_COMPARE_OP_GREATER_OR_EQUAL:
        case VK_COMPARE_OP_ALWAYS:
            return true;
        default:
            return false;
        }
    }

} // namespace StarryEngine