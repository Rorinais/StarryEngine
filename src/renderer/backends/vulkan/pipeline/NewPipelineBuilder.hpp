#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "./pipelineStateComponent/ComponentRegistry.hpp"
#include "./pipelineStateComponent/ColorBlendComponent.hpp"
#include "./pipelineStateComponent/DepthStencilComponent.hpp"
#include "./pipelineStateComponent/DynamicStateComponent.hpp"
#include "./pipelineStateComponent/InputAssemblyComponent.hpp"
#include "./pipelineStateComponent/MultiSampleComponent.hpp"
#include "./pipelineStateComponent/RasterizationComponent.hpp"
#include "./pipelineStateComponent/ShaderStageComponent.hpp"
#include "./pipelineStateComponent/VertexInputComponent.hpp"
#include "./pipelineStateComponent/ViewportComponent.hpp"

namespace StarryEngine {

    struct ComponentSelection {
        PipelineComponentType type;
        std::string name;  // 如果为空，使用默认

        ComponentSelection(PipelineComponentType t, const std::string& n = "")
            : type(t), name(n) {
        }
    };

    class PipelineBuilder {
    public:
        PipelineBuilder(VkDevice device, std::shared_ptr<ComponentRegistry> registry);

        // 添加组件选择
        PipelineBuilder& addComponent(PipelineComponentType type, const std::string& name = "");

        // 批量添加组件选择
        PipelineBuilder& addComponents(const std::vector<ComponentSelection>& selections);

        // 清空所有选择
        PipelineBuilder& clearSelections();

        // 构建图形管线
        VkPipeline buildGraphicsPipeline(
            VkPipelineLayout pipelineLayout,
            VkRenderPass renderPass,
            uint32_t subpass = 0,
            bool validate = true);

        // 从预定义组合构建
        VkPipeline buildFromPreset(
            const std::string& presetName,
            VkPipelineLayout pipelineLayout,
            VkRenderPass renderPass,
            uint32_t subpass = 0);

        // 验证组件选择
        bool validateSelections() const;

        // 获取当前选择
        const std::vector<ComponentSelection>& getSelections() const { return mSelections; }

        // 获取管线创建信息（用于调试）
        std::string getPipelineCreateInfo() const;

    private:
        VkDevice mDevice;
        std::shared_ptr<ComponentRegistry> mRegistry;
        std::vector<ComponentSelection> mSelections;

        // 从预设获取组件选择
        std::vector<ComponentSelection> getPresetSelections(const std::string& presetName) const;

        // 收集所有组件
        std::unordered_map<PipelineComponentType,
            std::shared_ptr<IPipelineStateComponent>> collectComponents(std::vector<std::string>& warnings) const;

        // 创建管线创建信息结构
        VkGraphicsPipelineCreateInfo createPipelineCreateInfo(
            const std::unordered_map<PipelineComponentType,
            std::shared_ptr<IPipelineStateComponent>>&components,
            VkPipelineLayout pipelineLayout,
            VkRenderPass renderPass,
            uint32_t subpass) const;
    };

} // namespace StarryEngine