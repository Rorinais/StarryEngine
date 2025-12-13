#include "RenderPassBeginInfo.hpp"

namespace StarryEngine{

    RenderPassBeginInfo::RenderPassBeginInfo(
            VkClearValue color,
            VkClearValue depth,
            VkOffset2D offset){
        clearValues.push_back(color);
        clearValues.push_back(depth);

        passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        passInfo.renderArea.offset = offset;
        passInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        passInfo.pClearValues = clearValues.data();
    }

    RenderPassBeginInfo& RenderPassBeginInfo::reset(){
        clearValues.clear();
        return *this;
    }

    RenderPassBeginInfo& RenderPassBeginInfo::update(VkRenderPass&pass,VkFramebuffer&buffer,VkExtent2D extent){
        passInfo.renderPass = pass;
        passInfo.framebuffer = buffer;
        passInfo.renderArea.extent = extent;
        passInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        passInfo.pClearValues = clearValues.data();
        return *this;
    }

    RenderPassBeginInfo& RenderPassBeginInfo::addClearColor(VkClearValue color){
        clearValues.push_back(color);
        return *this;
    }

    RenderPassBeginInfo& RenderPassBeginInfo::addClearDepth(VkClearValue depth){
        clearValues.push_back(depth);
        return *this;
    }

    RenderPassBeginInfo& RenderPassBeginInfo::setExtent(VkExtent2D extent){
        passInfo.renderArea.extent = extent;
        return *this;
    }

    RenderPassBeginInfo& RenderPassBeginInfo::setOffset(VkOffset2D offset){
        passInfo.renderArea.offset = offset;
        return *this;
    }
}