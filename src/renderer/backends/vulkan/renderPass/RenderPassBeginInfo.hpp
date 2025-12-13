#pragma once
#include<vulkan/vulkan.h>
#include<iostream>
#include<vector>
namespace StarryEngine{
    class RenderPassBeginInfo{
    public:
        RenderPassBeginInfo(VkClearValue color = {0,0,0,1.0f},VkClearValue depth = {1.0f,0},VkOffset2D offset = {0,0});
        ~RenderPassBeginInfo(){}

        RenderPassBeginInfo& reset();
        RenderPassBeginInfo& update(VkRenderPass&pass,VkFramebuffer&buffer,VkExtent2D extent);

        RenderPassBeginInfo& addClearColor(VkClearValue color);
        RenderPassBeginInfo& addClearDepth(VkClearValue depth);
        RenderPassBeginInfo& setExtent(VkExtent2D extent);
        RenderPassBeginInfo& setOffset(VkOffset2D offset);

        VkRenderPassBeginInfo& getRenderPassBeginInfo(){return passInfo;}
    private:
        VkRenderPassBeginInfo passInfo{};
        std::vector<VkClearValue> clearValues;
    };
}