#pragma once
#include "RenderGraphTypes.hpp"
#include <vulkan/vulkan.h>
#include <functional>
#include <memory>

namespace StarryEngine {

    class IResourceManager;

    class IRenderGraph {
    public:
        virtual ~IRenderGraph() = default;

        // 初始化
        virtual bool initialize(VkDevice device, VmaAllocator allocator) = 0;
        virtual void shutdown() = 0;

        // 构建接口
        virtual RenderPassHandle addPass(const std::string& name,
            std::function<void(RenderPass&)> setupCallback) = 0;
        virtual ResourceHandle createResource(const std::string& name,
            const ResourceDescription& desc) = 0;
        virtual bool importResource(ResourceHandle handle, VkImage image,
            VkImageView view, ResourceState initialState) = 0;

        // 编译和执行
        virtual bool compile() = 0;
        virtual void execute(VkCommandBuffer cmd, uint32_t frameIndex) = 0;

        // 资源管理
        virtual void setResourceManager(IResourceManager* manager) = 0;

        // 帧管理
        virtual void beginFrame(uint32_t frameIndex) = 0;
        virtual void endFrame(uint32_t frameIndex) = 0;
    };

} // namespace StarryEngine