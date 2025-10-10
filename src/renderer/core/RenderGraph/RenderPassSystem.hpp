#pragma once
#include <iostream>
#include <memory>
#include <functional>
#include <vulkan/vulkan.h>
#include "RenderGraphTypes.hpp"

namespace StarryEngine {

    // 前向声明
    class CommandBuffer;
    class RenderContext;

    // 命令缓冲执行回调函数
    using ExecuteCallback = std::function<void(CommandBuffer* cmdBuffer, RenderContext& context)>;

    struct RenderContext {
        CommandBuffer* commandBuffer;
        uint32_t frameIndex;
        // 资源访问方法将在执行器中实现
    };

    class RenderPass {
    private:
        std::string mName;
        ExecuteCallback mExecuteCallback;
        std::vector<ResourceUsage> mResourceUsages;
        uint32_t mIndex = 0;

        // 编译时确定的资源依赖
        VkRenderPass mRenderPass = VK_NULL_HANDLE;
        VkFramebuffer mFramebuffer = VK_NULL_HANDLE;
        std::vector<VkClearValue> mClearValues;

    public:
        using Ptr = std::shared_ptr<RenderPass>;
        RenderPass(std::string name) : mName(name) {}
        ~RenderPass() = default;

        // 资源使用声明
        void reads(ResourceHandle resource, VkPipelineStageFlags stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        void writes(ResourceHandle resource, VkPipelineStageFlags stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        void readWrite(ResourceHandle resource, VkPipelineStageFlags stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        // 带描述符绑定的资源使用
        void reads(ResourceHandle resource, uint32_t binding, VkDescriptorType type, VkPipelineStageFlags stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        void setExecuteCallback(ExecuteCallback callback) { mExecuteCallback = callback; }

        bool compile();
        void execute(CommandBuffer* cmdBuffer, RenderContext& context);

        const std::string& getName() const { return mName; }
        const std::vector<ResourceUsage>& getResourceUsages() const { return mResourceUsages; }
        uint32_t getIndex() const { return mIndex; }
        void setIndex(uint32_t index) { mIndex = index; }

    private:
        bool createVulkanRenderPass();
        bool createFramebuffer();
    };

} // namespace StarryEngine