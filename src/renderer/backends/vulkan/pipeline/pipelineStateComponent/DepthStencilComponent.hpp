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

    class DepthStencilComponent : public
        TypedPipelineComponent<DepthStencilComponent, PipelineComponentType::DEPTH_STENCIL> {
    public:
        DepthStencilComponent(const std::string& name);
        DepthStencilComponent& reset();

        // 深度测试相关配置
        DepthStencilComponent& enableDepthTest(VkBool32 enable);
        DepthStencilComponent& enableDepthWrite(VkBool32 enable);
        DepthStencilComponent& setDepthCompareOp(VkCompareOp compareOp);
        DepthStencilComponent& enableDepthBoundsTest(VkBool32 enable);
        DepthStencilComponent& setDepthBounds(float minDepthBounds, float maxDepthBounds);

        // 模板测试相关配置
        DepthStencilComponent& enableStencilTest(VkBool32 enable);

        // 前向面模板操作
        DepthStencilComponent& setFrontStencilOpState(
            VkStencilOp failOp,
            VkStencilOp passOp,
            VkStencilOp depthFailOp,
            VkCompareOp compareOp,
            uint32_t compareMask = 0xFF,
            uint32_t writeMask = 0xFF,
            uint32_t reference = 0);

        // 后向面模板操作
        DepthStencilComponent& setBackStencilOpState(
            VkStencilOp failOp,
            VkStencilOp passOp,
            VkStencilOp depthFailOp,
            VkCompareOp compareOp,
            uint32_t compareMask = 0xFF,
            uint32_t writeMask = 0xFF,
            uint32_t reference = 0);

        // 设置前后模板操作相同
        DepthStencilComponent& setStencilOpState(
            VkStencilOp failOp,
            VkStencilOp passOp,
            VkStencilOp depthFailOp,
            VkCompareOp compareOp,
            uint32_t compareMask = 0xFF,
            uint32_t writeMask = 0xFF,
            uint32_t reference = 0);

        // 便捷函数：常见深度测试配置
        DepthStencilComponent& enableStandardDepthTest();      // 启用标准深度测试（深度测试+写入）
        DepthStencilComponent& enableDepthTestOnly();          // 仅深度测试，不写入
        DepthStencilComponent& disableDepthTest();             // 禁用深度测试和写入

        // 便捷函数：常见模板测试配置
        DepthStencilComponent& enableStandardStencilTest();    // 启用标准模板测试
        DepthStencilComponent& disableStencilTest();           // 禁用模板测试

        void apply(VkGraphicsPipelineCreateInfo& pipelineInfo) override {
            updateCreateInfo();
            pipelineInfo.pDepthStencilState = &mCreateInfo;
        }

        std::string getDescription() const override;
        bool isValid() const override;

        // 获取当前深度测试状态
        VkBool32 getDepthTestEnable() const { return mCreateInfo.depthTestEnable; }
        VkBool32 getDepthWriteEnable() const { return mCreateInfo.depthWriteEnable; }
        VkCompareOp getDepthCompareOp() const { return mCreateInfo.depthCompareOp; }

        // 获取当前模板测试状态
        VkBool32 getStencilTestEnable() const { return mCreateInfo.stencilTestEnable; }
        const VkStencilOpState& getFrontStencilOpState() const { return mCreateInfo.front; }
        const VkStencilOpState& getBackStencilOpState() const { return mCreateInfo.back; }

    private:
        VkPipelineDepthStencilStateCreateInfo mCreateInfo{};

        void updateCreateInfo();

        // 辅助函数，用于检查深度边界是否有效
        bool areDepthBoundsValid() const;
        bool isStencilOpValid(VkStencilOp op) const;
        bool isCompareOpValid(VkCompareOp op) const;
    };

#endif // MYDEBUG
} // namespace StarryEngine