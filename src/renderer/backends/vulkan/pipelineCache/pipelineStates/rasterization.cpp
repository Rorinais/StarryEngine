#include "rasterization.hpp"
namespace StarryEngine {
    Rasterization& Rasterization::setPolygonMode(VkPolygonMode mode) {
        mConfig.polygonMode = mode;
        return *this;
    }

    Rasterization& Rasterization::setCullMode(VkCullModeFlags cullMode) {
        mConfig.cullMode = cullMode;
        return *this;
    }

    Rasterization& Rasterization::setFrontFace(VkFrontFace frontFace) {
        mConfig.frontFace = frontFace;
        return *this;
    }

    Rasterization& Rasterization::setLineWidth(float width) {
        mConfig.lineWidth = width;
        return *this;
    }

    Rasterization& Rasterization::enableDepthClamp(VkBool32 enable) {
        mConfig.depthClampEnable = enable;
        return *this;
    }

    Rasterization& Rasterization::enableDepthBias(VkBool32 enable) {
        mConfig.depthBiasEnable = enable;
        return *this;
    }
    Rasterization& Rasterization::enableRasterizerDiscard(VkBool32 enable) {
        mConfig.rasterizerDiscardEnable = enable;
        return *this;
    }

    const VkPipelineRasterizationStateCreateInfo& Rasterization::getCreateInfo() const {
        static VkPipelineRasterizationStateCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        info.polygonMode = mConfig.polygonMode;
        info.cullMode = mConfig.cullMode;
        info.frontFace = mConfig.frontFace;
        info.lineWidth = mConfig.lineWidth;

        info.depthClampEnable = mConfig.depthClampEnable;
        info.rasterizerDiscardEnable = mConfig.rasterizerDiscardEnable;
        info.depthBiasEnable = mConfig.depthBiasEnable;
        return info;
    }
}