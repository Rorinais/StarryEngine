#pragma once
#include "../interface/TypedPipelineComponent.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <stdexcept>
#include <set>
#include <unordered_map>

namespace StarryEngine {
    class ColorBlendComponent : public
        TypedPipelineComponent<ColorBlendComponent, PipelineComponentType::COLOR_BLEND> {
    public:
        ColorBlendComponent(const std::string& name);
        ColorBlendComponent& reset();

        // 全局颜色混合状态
        ColorBlendComponent& enableLogicOp(VkBool32 enable);
        ColorBlendComponent& setLogicOp(VkLogicOp logicOp);
        ColorBlendComponent& setBlendConstants(float r, float g, float b, float a);

        // 添加颜色附件混合状态
        ColorBlendComponent& addAttachmentState(const VkPipelineColorBlendAttachmentState& attachment);

        // 便捷函数：创建标准混合状态
        ColorBlendComponent& addAttachmentState(
            VkBool32 blendEnable,
            VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            VkBlendOp colorBlendOp = VK_BLEND_OP_ADD,
            VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD,
            VkColorComponentFlags colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT);

        // 批量设置附件状态
        ColorBlendComponent& setAttachmentStates(const std::vector<VkPipelineColorBlendAttachmentState>& attachments);

        // 便捷函数：常见混合配置
        ColorBlendComponent& addNoBlendingAttachment();           // 无混合
        ColorBlendComponent& addAlphaBlendingAttachment();        // Alpha混合
        ColorBlendComponent& addAdditiveBlendingAttachment();     // 加法混合
        ColorBlendComponent& addMultiplicativeBlendingAttachment(); // 乘法混合

        // 设置所有附件为相同状态
        ColorBlendComponent& setAllAttachmentsSame(bool same);

        void apply(VkGraphicsPipelineCreateInfo& pipelineInfo) override {
            updateCreateInfo();
            pipelineInfo.pColorBlendState = &mCreateInfo;
        }

        std::string getDescription() const override;
        bool isValid() const override;

        // 获取颜色混合信息
        const std::vector<VkPipelineColorBlendAttachmentState>& getAttachmentStates() const { return mAttachmentStates; }
        uint32_t getAttachmentCount() const { return static_cast<uint32_t>(mAttachmentStates.size()); }
        const float* getBlendConstants() const { return mCreateInfo.blendConstants; }
        VkBool32 getLogicOpEnable() const { return mCreateInfo.logicOpEnable; }
        VkLogicOp getLogicOp() const { return mCreateInfo.logicOp; }

    private:
        VkPipelineColorBlendStateCreateInfo mCreateInfo{};
        std::vector<VkPipelineColorBlendAttachmentState> mAttachmentStates;
        bool mAllAttachmentsSame = false;  // 是否所有附件使用相同状态

        void updateCreateInfo();

        // 辅助函数声明
        bool isColorBlendAttachmentValid(const VkPipelineColorBlendAttachmentState& state) const;
        bool isBlendFactorValid(VkBlendFactor factor) const;
        bool isBlendOpValid(VkBlendOp op) const;
    };

} // namespace StarryEngine