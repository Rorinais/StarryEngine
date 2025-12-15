#pragma once
#include <vulkan/vulkan.h>
#include "../backends/vulkan/vulkanCore/VulkanCore.hpp"
#include "../backends/vulkan/windowContext/WindowContext.hpp"

namespace StarryEngine{
    class IBackend {
    public:
        virtual ~IBackend() = default;

        // 初始化
        virtual bool initialize(VulkanCore::Ptr core, WindowContext::Ptr window) = 0;
        virtual void shutdown() = 0;

        // 帧管理
        virtual void beginFrame() = 0;
        virtual VkCommandBuffer getCommandBuffer() = 0;
        virtual void submitFrame() = 0;

        // 事件处理
        virtual void onSwapchainRecreated() = 0;

        // 状态查询
        virtual uint32_t getCurrentFrameIndex() const = 0;
        virtual uint32_t getCurrentImageIndex() const = 0;

        virtual bool isFrameInProgress() const = 0;
    };

}