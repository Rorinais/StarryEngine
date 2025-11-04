#pragma once
#include "VulkanCore/VulkanCore.hpp"
#include "WindowContext/WindowContext.hpp"
#include <vulkan/vulkan.h>

namespace StarryEngine {

    class IResourceManager;

    class IVulkanBackend {
    public:
        virtual ~IVulkanBackend() = default;

        // 初始化
        virtual bool initialize(VulkanCore::Ptr core, WindowContext::Ptr window) = 0;
        virtual void shutdown() = 0;

        // 帧管理
        virtual void beginFrame() = 0;
        virtual VkCommandBuffer getCommandBuffer() = 0;
        virtual void submitFrame() = 0;

        // 资源管理
        virtual void setResourceManager(IResourceManager* manager) = 0;

        // 事件处理
        virtual void onSwapchainRecreated() = 0;

        // 状态查询
        virtual uint32_t getCurrentFrameIndex() const = 0;
        virtual bool isFrameInProgress() const = 0;

        virtual VkDevice getDevice() const = 0;
    };

} // namespace StarryEngine