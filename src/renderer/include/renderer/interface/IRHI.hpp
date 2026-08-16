#pragma once

#include<interface/RHIEnums.hpp>
#include<interface/RHIHandles.hpp>
#include<interface/RHIStructs.hpp>
#include<interface/IRHIResources.hpp>
#include<interface/RHICommandEncoder.hpp>
#include<interface/RHIConfig.hpp>
#include<interface/RHIManager.hpp>

#include <vulkan/vulkan.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace StarryEngine {
    class VulkanFrameContext;   // 前向声明：getFrameContext 返回后端帧上下文
}

namespace StarryEngine::RHI {
    inline constexpr uint32_t kMaxFramesInFlight = 2;

    class IRHI {
    public:
        virtual ~IRHI() = default;

        // 初始化（VulkanRHI 已实现：实例→设备→交换链→帧上下文）
        virtual bool initialize(const RHI::RHIInitConfig& config) = 0;

        // 每帧渲染
        virtual bool renderFrame(const std::function<void(RHI::RHICommandEncoder*, uint32_t imageIndex)>& drawFunc) = 0;

        // 重建交换链（窗口大小改变时）
        virtual bool recreateSwapChain(uint32_t width, uint32_t height) = 0;

        // 获取资源管理器（用于创建缓冲、纹理等）
        virtual std::shared_ptr<RHI::ResourceManager> getResourceManager() = 0;

        // 获取帧上下文（用于同步、统计）
        virtual std::shared_ptr<VulkanFrameContext> getFrameContext() = 0;

        // 获取深度纹理句柄（如果需要）
        virtual RHI::TextureHandle getDepthTexture() const = 0;

        // 获取帧缓冲句柄列表（如果需要）
        virtual const std::vector<RHI::FramebufferHandle>& getFramebuffers() const = 0;

        virtual std::unique_ptr<RHI::RHICommandEncoder> getCommandEncoder(VkCommandBuffer cmdBuf) const = 0;
        virtual std::unique_ptr<RHI::RHICommandEncoder> allocateSecondaryCommandEncoder(uint32_t workerIndex) const = 0;
        virtual void prepareParallelRecording(uint32_t workerCount) const = 0;

        virtual uint32_t getWidth() const = 0;
        virtual uint32_t getHeight() const = 0;

        // 等待设备空闲
        virtual void waitIdle() = 0;

        virtual void printDeviceInfo() = 0;
        virtual void printResourceStatistics() = 0;

        virtual RHI::Format getDepthFormat() const = 0;
        virtual void* getInstance()  const = 0;
        virtual void* getPhysicalDevice()  const = 0;
        virtual void* getDevice() const = 0;
        virtual uint32_t getGraphicsQueueFamilyIndex() const = 0;
        virtual void* getGraphicsQueue() const = 0;
        virtual RHI::Format getSwapChainImageFormat()  const = 0;
        virtual uint32_t getSwapChainImageCount()  const = 0;
        virtual void* getSwapChainImageView(uint32_t index) const = 0;

        // 帧槽位 / 在飞帧（per-slot 数据缓冲索引的依据）：
        virtual uint32_t getCurrentFrameIndex() const = 0;
        virtual uint32_t getCurrentImageIndex() const = 0;
        virtual uint32_t getMaxFramesInFlight() const = 0;

        // === 异步读回（帧序列导出）===
        virtual bool beginAsyncReadback(uint32_t slot, RHI::RHITexture* src, RHI::RHIBuffer* dst,const RHI::BufferImageCopyRegion& region) = 0;
        virtual bool waitAsyncReadback(uint32_t slot, uint64_t timeoutNs) = 0;
        virtual void releaseAsyncReadback(uint32_t slot) = 0;
        virtual uint32_t asyncReadbackSlotCount() const = 0;
    };

} // namespace StarryEngine::RHI
