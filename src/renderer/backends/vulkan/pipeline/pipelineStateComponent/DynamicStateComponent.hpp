#pragma once
#include "../interface/TypedPipelineComponent.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <stdexcept>
#include <set>
#include <unordered_map>

namespace StarryEngine {
#define MYDEBUG
#ifdef MYDEBUG
    class DynamicStateComponent : public
        TypedPipelineComponent<DynamicStateComponent, PipelineComponentType::DYNAMIC_STATE> {
    public:
        DynamicStateComponent(const std::string& name);
        DynamicStateComponent& reset();

        // 添加动态状态
        DynamicStateComponent& addDynamicState(VkDynamicState state);
        DynamicStateComponent& addDynamicStates(const std::vector<VkDynamicState>& states);

        // 移除动态状态
        DynamicStateComponent& removeDynamicState(VkDynamicState state);
        DynamicStateComponent& clearDynamicStates();

        // 检查动态状态是否存在
        bool hasDynamicState(VkDynamicState state) const;

        // 设置动态状态
        DynamicStateComponent& setDynamicStates(const std::vector<VkDynamicState>& states);

        // 便捷函数：添加常见动态状态组合
        DynamicStateComponent& addViewportScissorStates();    // 视口和裁剪
        DynamicStateComponent& addLineWidthState();           // 线宽
        DynamicStateComponent& addDepthStencilStates();       // 深度和模板状态
        DynamicStateComponent& addColorBlendStates();         // 颜色混合
        DynamicStateComponent& addVertexInputState();         // 顶点输入（需要扩展）

        void apply(VkGraphicsPipelineCreateInfo& pipelineInfo) override {
            updateCreateInfo();
            pipelineInfo.pDynamicState = &mCreateInfo;
        }

        std::string getDescription() const override;
        bool isValid() const override;

        // 获取动态状态列表
        const std::vector<VkDynamicState>& getDynamicStates() const { return mDynamicStates; }
        uint32_t getDynamicStateCount() const { return static_cast<uint32_t>(mDynamicStates.size()); }

    private:
        VkPipelineDynamicStateCreateInfo mCreateInfo{};
        std::vector<VkDynamicState> mDynamicStates;

        void updateCreateInfo();
    };
#endif // MYDEBUG
} // namespace StarryEngine