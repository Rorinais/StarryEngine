#include "NewPipelineBuilder.hpp"
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <sstream>

namespace StarryEngine {

    // 辅助函数：获取组件类型名称
    const char* GetComponentTypeName(PipelineComponentType type) {
        static const std::unordered_map<PipelineComponentType, const char*> typeNames = {
            {PipelineComponentType::VERTEX_INPUT, "VertexInput"},
            {PipelineComponentType::INPUT_ASSEMBLY, "InputAssembly"},
            {PipelineComponentType::VIEWPORT_STATE, "ViewportState"},
            {PipelineComponentType::RASTERIZATION, "Rasterization"},
            {PipelineComponentType::MULTISAMPLE, "Multisample"},
            {PipelineComponentType::DEPTH_STENCIL, "DepthStencil"},
            {PipelineComponentType::COLOR_BLEND, "ColorBlend"},
            {PipelineComponentType::DYNAMIC_STATE, "DynamicState"},
            {PipelineComponentType::SHADER_STAGE, "ShaderStage"}
        };

        auto it = typeNames.find(type);
        return it != typeNames.end() ? it->second : "Unknown";
    }

    PipelineBuilder::PipelineBuilder(VkDevice device, std::shared_ptr<ComponentRegistry> registry)
        : mDevice(device), mRegistry(registry) {
    }

    PipelineBuilder& PipelineBuilder::addComponent(PipelineComponentType type, const std::string& name) {
        mSelections.emplace_back(type, name);
        return *this;
    }

    PipelineBuilder& PipelineBuilder::addComponents(const std::vector<ComponentSelection>& selections) {
        mSelections.insert(mSelections.end(), selections.begin(), selections.end());
        return *this;
    }

    PipelineBuilder& PipelineBuilder::clearSelections() {
        mSelections.clear();
        return *this;
    }

    VkPipeline PipelineBuilder::buildGraphicsPipeline(
        VkPipelineLayout pipelineLayout,
        VkRenderPass renderPass,
        uint32_t subpass,
        bool validate) {

        if (validate && !validateSelections()) {
            throw std::runtime_error("Pipeline component validation failed");
        }

        std::vector<std::string> warnings;
        auto components = collectComponents(warnings);

        // 输出警告
        if (!warnings.empty()) {
            std::cout << "Pipeline build warnings:" << std::endl;
            for (const auto& warning : warnings) {
                std::cout << "  ⚠ " << warning << std::endl;
            }
        }

        // 创建管线创建信息
        VkGraphicsPipelineCreateInfo pipelineInfo = createPipelineCreateInfo(
            components, pipelineLayout, renderPass, subpass);

        // 创建管线
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkResult result = vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1,
            &pipelineInfo, nullptr, &pipeline);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline: VkResult = " +
                std::to_string(result));
        }

        std::cout << "Successfully created graphics pipeline with "
            << components.size() << " components" << std::endl;

        return pipeline;
    }

    VkPipeline PipelineBuilder::buildFromPreset(
        const std::string& presetName,
        VkPipelineLayout pipelineLayout,
        VkRenderPass renderPass,
        uint32_t subpass) {

        // 从预设获取组件选择
        auto selections = getPresetSelections(presetName);
        if (selections.empty()) {
            throw std::runtime_error("Preset not found: " + presetName);
        }

        clearSelections();
        addComponents(selections);

        return buildGraphicsPipeline(pipelineLayout, renderPass, subpass, true);
    }

    bool PipelineBuilder::validateSelections() const {
        // 检查是否有重复的组件类型
        std::unordered_set<PipelineComponentType> seenTypes;
        for (const auto& selection : mSelections) {
            if (!seenTypes.insert(selection.type).second) {
                std::cout << "Error: Duplicate component type: "
                    << GetComponentTypeName(selection.type) << std::endl;
                return false;
            }
        }

        // 检查必需的组件类型
        static const std::vector<PipelineComponentType> requiredTypes = {
            PipelineComponentType::SHADER_STAGE,
            PipelineComponentType::VIEWPORT_STATE,
            PipelineComponentType::RASTERIZATION,
            PipelineComponentType::MULTISAMPLE,
            PipelineComponentType::DEPTH_STENCIL,
            PipelineComponentType::COLOR_BLEND
        };

        bool allRequired = true;
        for (auto type : requiredTypes) {
            if (seenTypes.find(type) == seenTypes.end()) {
                // 尝试获取默认组件
                auto defaultComponent = mRegistry->getDefaultComponent(type);
                if (!defaultComponent) {
                    std::cout << "Error: Missing required component type: "
                        << GetComponentTypeName(type) << " and no default available" << std::endl;
                    allRequired = false;
                }
                else {
                    std::cout << "Warning: Missing required component type: "
                        << GetComponentTypeName(type) << ", will use default" << std::endl;
                }
            }
        }

        return allRequired;
    }

    std::vector<ComponentSelection> PipelineBuilder::getPresetSelections(const std::string& presetName) const {
        static const std::unordered_map<std::string, std::vector<ComponentSelection>> presets = {
            {"opaque", {
                {PipelineComponentType::SHADER_STAGE, ""},
                {PipelineComponentType::VERTEX_INPUT, ""},
                {PipelineComponentType::INPUT_ASSEMBLY, ""},
                {PipelineComponentType::VIEWPORT_STATE, ""},
                {PipelineComponentType::DYNAMIC_STATE, ""},
                {PipelineComponentType::RASTERIZATION, ""},
                {PipelineComponentType::MULTISAMPLE, ""},
                {PipelineComponentType::DEPTH_STENCIL, ""},
                {PipelineComponentType::COLOR_BLEND, ""}
            }},
            {"transparent", {
                {PipelineComponentType::SHADER_STAGE, ""},
                {PipelineComponentType::VERTEX_INPUT, ""},
                {PipelineComponentType::INPUT_ASSEMBLY, ""},
                {PipelineComponentType::VIEWPORT_STATE, ""},
                {PipelineComponentType::DYNAMIC_STATE, ""},
                {PipelineComponentType::RASTERIZATION, ""},
                {PipelineComponentType::MULTISAMPLE, ""},
                {PipelineComponentType::DEPTH_STENCIL, "test_only"},
                {PipelineComponentType::COLOR_BLEND, "alpha"}
            }},
            {"wireframe", {
                {PipelineComponentType::SHADER_STAGE, ""},
                {PipelineComponentType::VERTEX_INPUT, ""},
                {PipelineComponentType::INPUT_ASSEMBLY, ""},
                {PipelineComponentType::VIEWPORT_STATE, ""},
                {PipelineComponentType::DYNAMIC_STATE, ""},
                {PipelineComponentType::RASTERIZATION, "wireframe"},
                {PipelineComponentType::MULTISAMPLE, ""},
                {PipelineComponentType::DEPTH_STENCIL, ""},
                {PipelineComponentType::COLOR_BLEND, ""}
            }}
        };

        auto it = presets.find(presetName);
        if (it != presets.end()) {
            return it->second;
        }
        return {};
    }

    std::unordered_map<PipelineComponentType,
        std::shared_ptr<IPipelineStateComponent>>
        PipelineBuilder::collectComponents(std::vector<std::string>& warnings) const {
        std::unordered_map<PipelineComponentType,
            std::shared_ptr<IPipelineStateComponent>> components;

        for (const auto& selection : mSelections) {
            std::shared_ptr<IPipelineStateComponent> component;

            if (!selection.name.empty()) {
                // 尝试获取指定名称的组件
                component = mRegistry->getComponent(selection.type, selection.name);

                if (!component) {
                    // 组件未找到，使用默认
                    warnings.push_back("Component not found: " +
                        std::string(GetComponentTypeName(selection.type)) +
                        "[" + selection.name + "], using default");
                    component = mRegistry->getDefaultComponent(selection.type);
                }
            }
            else {
                // 使用默认组件
                component = mRegistry->getDefaultComponent(selection.type);
            }

            if (component) {
                // 检查组件是否有效
                if (!component->isValid()) {
                    throw std::runtime_error("Component is invalid: " +
                        std::string(GetComponentTypeName(selection.type)) +
                        "[" + (selection.name.empty() ? "default" : selection.name) + "]");
                }
                components[selection.type] = component;
            }
            else {
                throw std::runtime_error("No component available for type: " +
                    std::string(GetComponentTypeName(selection.type)));
            }
        }

        return components;
    }

    VkGraphicsPipelineCreateInfo PipelineBuilder::createPipelineCreateInfo(
        const std::unordered_map<PipelineComponentType,
        std::shared_ptr<IPipelineStateComponent>>&components,
        VkPipelineLayout pipelineLayout,
        VkRenderPass renderPass,
        uint32_t subpass) const {

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = subpass;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        // 应用所有组件
        for (const auto& [type, component] : components) {
            component->apply(pipelineInfo);
        }

        return pipelineInfo;
    }

    std::string PipelineBuilder::getPipelineCreateInfo() const {
        std::stringstream ss;
        ss << "Pipeline Create Info:\n";
        ss << "  Component Count: " << mSelections.size() << "\n";

        for (const auto& selection : mSelections) {
            ss << "  - " << GetComponentTypeName(selection.type);
            if (!selection.name.empty()) {
                ss << " [" << selection.name << "]";
            }
            ss << "\n";
        }

        return ss.str();
    }

} // namespace StarryEngine