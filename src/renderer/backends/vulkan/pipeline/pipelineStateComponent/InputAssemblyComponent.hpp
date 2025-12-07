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
    class InputAssemblyComponent : public
        TypedPipelineComponent<InputAssemblyComponent, PipelineComponentType::INPUT_ASSEMBLY> {
    public:
        InputAssemblyComponent(const std::string& name);
        InputAssemblyComponent& reset();

        InputAssemblyComponent& setTopology(VkPrimitiveTopology topology);
        InputAssemblyComponent& enablePrimitiveRestart(VkBool32 enable);

        // 便捷函数，用于设置常见的拓扑类型
        InputAssemblyComponent& setTriangleList();
        InputAssemblyComponent& setTriangleStrip();
        InputAssemblyComponent& setLineList();
        InputAssemblyComponent& setLineStrip();
        InputAssemblyComponent& setPointList();

        void apply(VkGraphicsPipelineCreateInfo& pipelineInfo) override {
            updateCreateInfo();
            pipelineInfo.pInputAssemblyState = &mCreateInfo;
        }

        std::string getDescription() const override;
        bool isValid() const override;

        // 获取当前拓扑和重启状态
        VkPrimitiveTopology getTopology() const { return mCreateInfo.topology; }
        VkBool32 getPrimitiveRestartEnable() const { return mCreateInfo.primitiveRestartEnable; }

    private:
        VkPipelineInputAssemblyStateCreateInfo mCreateInfo{};

        void updateCreateInfo(){}
    };
#endif // MYDEBUG
} // namespace StarryEngine