#include "RasterizationComponent.hpp"

namespace StarryEngine {

    RasterizationComponent::RasterizationComponent(const std::string& name) {
        setName(name);
        reset();  
    }

    RasterizationComponent& RasterizationComponent::reset() {
        // 设置Vulkan默认的栅格化状态
        mCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthClampEnable = VK_FALSE,              // 禁用深度截断
            .rasterizerDiscardEnable = VK_FALSE,       // 不禁用光栅化
            .polygonMode = VK_POLYGON_MODE_FILL,       // 填充模式
            .cullMode = VK_CULL_MODE_BACK_BIT,         // 剔除背面
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, // 逆时针为正面
            .depthBiasEnable = VK_FALSE,               // 禁用深度偏移
            .depthBiasConstantFactor = 0.0f,           // 深度偏移常量因子
            .depthBiasClamp = 0.0f,                    // 深度偏移钳制值
            .depthBiasSlopeFactor = 0.0f,              // 深度偏移斜率因子
            .lineWidth = 1.0f                          // 线宽
        };
        return *this;
    }

    RasterizationComponent& RasterizationComponent::enableDepthClamp(VkBool32 enable) {
        mCreateInfo.depthClampEnable = enable;
        return *this;
    }

    RasterizationComponent& RasterizationComponent::enableRasterizerDiscard(VkBool32 enable) {
        mCreateInfo.rasterizerDiscardEnable = enable;
        return *this;
    }

    RasterizationComponent& RasterizationComponent::setPolygonMode(VkPolygonMode mode) {
        mCreateInfo.polygonMode = mode;
        return *this;
    }

    RasterizationComponent& RasterizationComponent::setCullMode(VkCullModeFlags cullMode) {
        mCreateInfo.cullMode = cullMode;
        return *this;
    }

    RasterizationComponent& RasterizationComponent::setFrontFace(VkFrontFace frontFace) {
        mCreateInfo.frontFace = frontFace;
        return *this;
    }

    RasterizationComponent& RasterizationComponent::enableDepthBias(VkBool32 enable) {
        mCreateInfo.depthBiasEnable = enable;
        return *this;
    }

    RasterizationComponent& RasterizationComponent::setDepthBiasConstantFactor(float factor) {
        mCreateInfo.depthBiasConstantFactor = factor;
        return *this;
    }

    RasterizationComponent& RasterizationComponent::setDepthBiasClamp(float clamp) {
        mCreateInfo.depthBiasClamp = clamp;
        return *this;
    }

    RasterizationComponent& RasterizationComponent::setDepthBiasSlopeFactor(float factor) {
        mCreateInfo.depthBiasSlopeFactor = factor;
        return *this;
    }

    RasterizationComponent& RasterizationComponent::setLineWidth(float width) {
        mCreateInfo.lineWidth = width;
        return *this;
    }

    std::string RasterizationComponent::getDescription() const {
        std::string desc = "Rasterization State: ";

        // 多边形模式
        desc += "PolygonMode=";
        switch (mCreateInfo.polygonMode) {
        case VK_POLYGON_MODE_FILL: desc += "FILL"; break;
        case VK_POLYGON_MODE_LINE: desc += "LINE"; break;
        case VK_POLYGON_MODE_POINT: desc += "POINT"; break;
        default: desc += "UNKNOWN"; break;
        }

        // 剔除模式
        desc += ", CullMode=";
        if (mCreateInfo.cullMode & VK_CULL_MODE_FRONT_BIT) desc += "FRONT";
        if (mCreateInfo.cullMode & VK_CULL_MODE_BACK_BIT) {
            if (mCreateInfo.cullMode & VK_CULL_MODE_FRONT_BIT) desc += "_AND_";
            desc += "BACK";
        }
        if (mCreateInfo.cullMode == VK_CULL_MODE_NONE) desc += "NONE";

        // 正面方向
        desc += ", FrontFace=";
        desc += (mCreateInfo.frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE) ? "CCW" : "CW";

        // 深度截断
        if (mCreateInfo.depthClampEnable) desc += ", DepthClamp=ENABLED";

        // 深度偏移
        if (mCreateInfo.depthBiasEnable) {
            desc += ", DepthBias=[" + std::to_string(mCreateInfo.depthBiasConstantFactor) +
                ", " + std::to_string(mCreateInfo.depthBiasClamp) +
                ", " + std::to_string(mCreateInfo.depthBiasSlopeFactor) + "]";
        }

        // 线宽
        desc += ", LineWidth=" + std::to_string(mCreateInfo.lineWidth);

        return desc;
    }

    bool RasterizationComponent::isValid() const {
        // 检查基础结构类型
        if (mCreateInfo.sType != VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO) {
            return false;
        }

        // 检查多边形模式是否有效
        if (mCreateInfo.polygonMode != VK_POLYGON_MODE_FILL &&
            mCreateInfo.polygonMode != VK_POLYGON_MODE_LINE &&
            mCreateInfo.polygonMode != VK_POLYGON_MODE_POINT) {
            return false;
        }

        // 检查剔除模式是否有效
        VkCullModeFlags validCullModes = VK_CULL_MODE_NONE | VK_CULL_MODE_FRONT_BIT |
            VK_CULL_MODE_BACK_BIT | VK_CULL_MODE_FRONT_AND_BACK;
        if ((mCreateInfo.cullMode & ~validCullModes) != 0) {
            return false;
        }

        // 检查正面方向是否有效
        if (mCreateInfo.frontFace != VK_FRONT_FACE_COUNTER_CLOCKWISE &&
            mCreateInfo.frontFace != VK_FRONT_FACE_CLOCKWISE) {
            return false;
        }

        // 检查线宽是否合法（通常要求lineWidth >= 1.0，但具体取决于硬件支持）
        if (mCreateInfo.lineWidth <= 0.0f) {
            return false;
        }

        // 如果启用了深度偏移，检查参数是否合法
        if (mCreateInfo.depthBiasEnable) {
            // 深度偏移钳制值可以是0.0或正数
            if (mCreateInfo.depthBiasClamp < 0.0f) {
                return false;
            }
        }

        return true;
    }
} // namespace StarryEngine