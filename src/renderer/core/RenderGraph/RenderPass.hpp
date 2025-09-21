#pragma once
#include <iostream>
#include <memory>
#include <functional>
#include <vulkan/vulkan.h>
#include "../VulkanCore/VulkanCore.hpp"
#include "../WindowContext/WindowContext.hpp"
#include "../FrameContext/FrameContext.hpp"
#include "RenderGraphTypes.hpp"

// 前向声明
class CommandPool;
class CommandBuffer;
class ShaderProgram;

namespace StarryEngine {
    //先实现简单的renderGraph，可以将一个pass当成一个节点
	//每个pass有自己的资源依赖和执行逻辑
	//依赖资源可以有多个，输出资源也可以有多个，所以可以用vector存储
    //资源可以定义一个资源引用的结构体，
	//我们在外部是通过rendererGraph交互的，所以renderGraph会管理所有的资源
 
    //命令缓冲执行回调函数
    using ExecuteCallback = std::function<void(CommandBuffer::Ptr cmdBuffer, class RenderContext& context)>;

    struct RenderContext {
		CommandBuffer::Ptr commandBuffer;
		uint32_t frameIndex;
		//ResourceRegistry::Ptr resourceRegistry; // 资源注册表指针
		//DescriptorAllocator::Ptr descriptorAllocator; // 描述符集注册表指针

        VkImage getImage(ResourceHandle handle)const {
            // 从资源注册表中获取VkImage
            return VK_NULL_HANDLE; 
		}

        VkBuffer getBuffer(ResourceHandle handle)const {
            // 从资源注册表中获取VkBuffer
            return VK_NULL_HANDLE;
        }

        VkImageView getImageView(ResourceHandle handle)const {
            // 从资源注册表中获取VkImageView
            return VK_NULL_HANDLE;
		}

        VkDescriptorSet getDescriptorSet(ResourceHandle handle)const {
            // 从资源注册表中获取VkDescriptorSet
            return VK_NULL_HANDLE;
        }

	};


	//资源使用类型
    struct PassResourceUsage{
		ResourceHandle resource;
		VkPipelineStageFlags stageFlags; // 资源使用的管线阶段
		VkAccessFlags accessFlags; // 资源的访问权限
		VkImageLayout layout; // 资源的布局
		VkDescriptorType descriptorType; // 描述符类型

		bool isWrite; // 是否是写操作

        uint32_t binding = 0; // 绑定点
		uint32_t descriptorSet = 0; // 描述符集索引
    };

    class RenderPass {
    private:
        std::string mName;
		ExecuteCallback mExecuteCallback;//命令缓冲执行回调函数
		std::vector<PassResourceUsage> mResourceUsages;
        
		uint32_t mIndex = 0; // 渲染通道索引

		//编译时确定的资源依赖
        VkRenderPass mRenderPass = VK_NULL_HANDLE;
		VkFramebuffer mFramebuffer = VK_NULL_HANDLE;
		std::vector<VkClearValue> mClearValues; // 清除值

    public:
        using Ptr = std::shared_ptr<RenderPass>;
        RenderPass(std::string name) :mName(name){}
         ~RenderPass() = default;
		
         // 资源使用声明
         void reads(ResourceHandle resource, VkPipelineStageFlags stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
         void writes(ResourceHandle resource, VkPipelineStageFlags stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
         void readWrite(ResourceHandle resource, VkPipelineStageFlags stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

         // 带描述符绑定的资源使用
         void reads(ResourceHandle resource, uint32_t binding, VkDescriptorType type,VkPipelineStageFlags stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

		 void setExecuteCallback(ExecuteCallback callback) { mExecuteCallback = callback; }

		 bool compile();

		 void execute(CommandBuffer::Ptr cmdBuffer,RenderContext& context);

         const std::string& getName() const { return mName; }
         const std::vector<PassResourceUsage>& getResourceUsages() const { return mResourceUsages; }
         uint32_t getIndex() const { return mIndex; }
         void setIndex(uint32_t index) { mIndex = index; }
         
    private:
        bool createVulkanRenderPass();
		bool createFramebuffer();
    };
}
