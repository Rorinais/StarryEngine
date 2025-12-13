#pragma once
#include "../interface/TypedPipelineComponent.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <stdexcept>
#include <set>
#include <unordered_map>

namespace StarryEngine {
    class RasterizationComponent : public
        TypedPipelineComponent<RasterizationComponent, PipelineComponentType::RASTERIZATION> {
    public:
        RasterizationComponent(const std::string& name);
        RasterizationComponent& reset();

        RasterizationComponent& enableDepthClamp(VkBool32 enable);
        RasterizationComponent& enableRasterizerDiscard(VkBool32 enable);
        RasterizationComponent& setPolygonMode(VkPolygonMode mode);
        RasterizationComponent& setCullMode(VkCullModeFlags cullMode);
        RasterizationComponent& setFrontFace(VkFrontFace frontFace);
        RasterizationComponent& enableDepthBias(VkBool32 enable);
		RasterizationComponent& setDepthBiasConstantFactor(float factor);
		RasterizationComponent& setDepthBiasClamp(float clamp);
		RasterizationComponent& setDepthBiasSlopeFactor(float factor);
        RasterizationComponent& setLineWidth(float width);

        void apply(VkGraphicsPipelineCreateInfo& pipelineInfo) override {
			pipelineInfo.pRasterizationState = &mCreateInfo;
        }

        std::string getDescription() const override;
        bool isValid() const override;

    private:
        VkPipelineRasterizationStateCreateInfo mCreateInfo{};

        void updateCreateInfo() {}
    };
} // namespace StarryEngine