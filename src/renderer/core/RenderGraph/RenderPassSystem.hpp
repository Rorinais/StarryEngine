#pragma once
#include <iostream>
#include <memory>
#include <functional>
#include <vulkan/vulkan.h>
#include "RenderGraphTypes.hpp"

namespace StarryEngine {
    class CommandBuffer;
    class RenderContext;

    class RenderPass {
    public:
        using ExecuteCallback = std::function<void(CommandBuffer* cmdBuffer, RenderContext& context)>;

        using Ptr = std::shared_ptr<RenderPass>;
        RenderPass() = default;
        ~RenderPass() = default;

        void setName(const std::string& name) { mName = name; }

        // 资源使用声明 - 使用更清晰的命名
        void declareRead(ResourceHandle resource, VkPipelineStageFlags stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        void declareWrite(ResourceHandle resource, VkPipelineStageFlags stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        void declareReadWrite(ResourceHandle resource, VkPipelineStageFlags stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        // 带描述符绑定的资源使用
        void declareRead(ResourceHandle resource, uint32_t binding, VkDescriptorType type, VkPipelineStageFlags stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        // 设置执行逻辑 - 使用更清晰的命名
        void setExecutionLogic(ExecuteCallback callback) { mExecuteCallback = std::move(callback); }

        bool compile();
        void execute(CommandBuffer* cmdBuffer, RenderContext& context);

        // 调试方法
        void dumpDebugInfo() const;
        const std::string& getName() const { return mName; }
        const std::vector<ResourceUsage>& getResourceUsages() const { return mResourceUsages; }
        const ExecuteCallback& getExecuteCallback() const { return mExecuteCallback; } // 用于调试
        uint32_t getIndex() const { return mIndex; }
        void setIndex(uint32_t index) { mIndex = index; }

    private:
        bool createVulkanRenderPass();
        bool createFramebuffer();

    private:
        std::string mName;
        ExecuteCallback mExecuteCallback;
        std::vector<ResourceUsage> mResourceUsages;
        uint32_t mIndex = UINT32_MAX;

        // 编译时确定的资源依赖
        VkRenderPass mRenderPass = VK_NULL_HANDLE;
        VkFramebuffer mFramebuffer = VK_NULL_HANDLE;
        std::vector<VkClearValue> mClearValues;
    };

} // namespace StarryEngine