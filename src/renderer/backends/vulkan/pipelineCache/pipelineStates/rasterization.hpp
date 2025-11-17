#pragma once
#include <vulkan/vulkan.h>
namespace StarryEngine {

    class Rasterization {
    public:
        struct Config {
            VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
            VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
            VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE;
            float lineWidth = 1.0f;
            VkBool32 depthClampEnable = VK_FALSE;
            VkBool32 rasterizerDiscardEnable = VK_FALSE;
            VkBool32 depthBiasEnable = VK_FALSE;
        };

        Rasterization& setPolygonMode(VkPolygonMode mode);
        Rasterization& setCullMode(VkCullModeFlags cullMode);
        Rasterization& setFrontFace(VkFrontFace frontFace);
        Rasterization& setLineWidth(float width);
        Rasterization& enableDepthClamp(VkBool32 enable);
        Rasterization& enableDepthBias(VkBool32 enable);
        Rasterization& enableRasterizerDiscard(VkBool32 enable);

        const VkPipelineRasterizationStateCreateInfo& getCreateInfo() const;

    private:
        Config mConfig;
        VkPipelineRasterizationStateCreateInfo mCreateInfo{};
    };
}