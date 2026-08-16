#pragma once

#include <interface/IRHI.hpp>
#include <interface/RHICommandEncoder.hpp>
#include <interface/RHIConfig.hpp>
#include <interface/RHIManager.hpp>
#include <interface/vulkan/VulkanInstance.hpp>
#include <interface/vulkan/VulkanDevice.hpp>
#include <interface/vulkan/VulkanSwapchain.hpp>
#include <interface/vulkan/VulkanFrameContext.hpp>
#include <interface/vulkan/VulkanConversion.hpp>

#include <vulkan/vulkan.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace StarryEngine{
    class ConfigConverter {
    public:
        static VulkanInstance::Config convertInstanceConfig(const RHI::RHIInitConfig& rhiConfig);
        static VulkanDevice::Config convertDeviceConfig(const RHI::RHIInitConfig& rhiConfig);
        static SwapChainConfig convertSwapChainConfig(const RHI::RHIInitConfig& rhiConfig);
        static VulkanFrameContext::Config convertFrameContextConfig(const RHI::RHIInitConfig& rhiConfig);

        static uint32_t convertFeatureLevel(RHI::FeatureLevel level);
        static RHI::MessageSeverity convertToRHISeverity(VkDebugUtilsMessageSeverityFlagBitsEXT vulkanSeverity);
        static RHI::MessageSource convertToRHISource(VkDebugUtilsMessageTypeFlagsEXT vulkanType);
        static const char* messageSourceToString(RHI::MessageSource source);
        static const char* messageSeverityToString(RHI::MessageSeverity severity);
    };

    class VulkanRHI: public RHI::IRHI {
    public:
        VulkanRHI() = default;
        ~VulkanRHI(){ clear(); }
        void clear();

        bool initialize(const RHI::RHIInitConfig& config)override;

        bool renderFrame(const std::function<void(RHI::RHICommandEncoder*, uint32_t imageIndex)>& drawFunc)override;

        bool recreateSwapChain(uint32_t width, uint32_t height)override;

        void setupFramebuffers(RHI::RenderPassHandle renderPass);

        void updateDescriptorSet(RHI::DescriptorSetHandle setHandle, uint32_t binding, uint32_t arrayElement, const RHI::DescriptorBufferInfo& bufferInfo);

        void updateDescriptorSet(RHI::DescriptorSetHandle set, uint32_t binding, uint32_t arrayElement, const RHI::DescriptorImageInfo& imageInfo);

        RHI::Format getDepthFormat() const override;

        uint32_t getWidth() const override { return mWidth; }
        uint32_t getHeight() const override { return mHeight; }

        std::shared_ptr<RHI::ResourceManager> getResourceManager() override { return mResourceManager; }

        std::shared_ptr<VulkanFrameContext> getFrameContext() override { return mFrameContext; }
  
        RHI::TextureHandle getDepthTexture() const override { return mDepthTextureHandle; }

        const std::vector<RHI::FramebufferHandle>& getFramebuffers() const override { return mFramebuffers; }

        std::unique_ptr<RHI::RHICommandEncoder> getCommandEncoder(VkCommandBuffer cmdBuf)const override;

        std::unique_ptr<RHI::RHICommandEncoder> allocateSecondaryCommandEncoder(uint32_t workerIndex) const override;
        void prepareParallelRecording(uint32_t workerCount) const override;

        void waitIdle() override {mDevice->waitIdle(); }

        void printDeviceInfo() override {
            mDevice->printDeviceInfo();
            mDevice->printQueueInfo();
            mSwapChain->printInfo();
        }

        void printResourceStatistics() override {
            mResourceManager->dumpStatistics();
        }

        void* getInstance() const override { return mDevice->getInstance()->getHandle(); }
        void* getPhysicalDevice() const override { return mDevice->getPhysicalDevice(); }
        void* getDevice()const override { return mDevice->getLogicalDevice(); }
        uint32_t getGraphicsQueueFamilyIndex()const override { return mDevice->getGraphicsQueueFamilyIndex(); }
        void* getGraphicsQueue()const override { return mDevice->getGraphicsQueue(); }
        RHI::Format getSwapChainImageFormat() const override { return func::VK_TO_RHI_Format(mSwapChain->getFormat()); }
        uint32_t getSwapChainImageCount() const override { return mSwapChain->getImageCount(); }
        void* getSwapChainImageView(uint32_t index) const override { return mSwapChain->getImageView(index); }

        // 帧槽位 / 在飞帧（per-slot 数据缓冲索引的依据）：
        // getCurrentFrameIndex() = VulkanFrameContext 的 0↔1 槽位（submitFrame 末尾翻转，一帧内稳定）。
        // 注意与 getCurrentImageIndex()（交换链图像索引，vkAcquireNextImage 返回）是两套独立索引。
        uint32_t getCurrentFrameIndex() const override {
            return mFrameContext ? mFrameContext->getCurrentFrameIndex() : 0;
        }
        uint32_t getCurrentImageIndex() const override {
            return mFrameContext ? mFrameContext->getCurrentFrameInfo().imageIndex : 0;
        }
        uint32_t getMaxFramesInFlight() const override {
            return mFrameContext ? mFrameContext->getFrameCount() : RHI::kMaxFramesInFlight;
        }

        // === 异步读回（帧序列导出）===
        // 槽位 = 持久命令缓冲 + fence；调用方（FrameCapture）自管轮换 + 背压，
        // 保证同一 slot 的 fence 信号后才再 beginAsyncReadback。
        bool beginAsyncReadback(uint32_t slot, RHI::RHITexture* src, RHI::RHIBuffer* dst,
                                const RHI::BufferImageCopyRegion& region) override;
        bool waitAsyncReadback(uint32_t slot, uint64_t timeoutNs) override;
        void releaseAsyncReadback(uint32_t slot) override;
        uint32_t asyncReadbackSlotCount() const override { return kReadbackSlotCount; }

    private:
        void destroyAsyncReadbackSlots();
        bool createDepthTexture();
        bool createFramebuffers();  
        void destroyFramebufferResources();

    private:
        VulkanInstance::Ptr mInstance;
        VulkanDevice::Ptr mDevice;
        VulkanSwapChain::Ptr mSwapChain;
        std::shared_ptr<VulkanFrameContext> mFrameContext;
        VkSurfaceKHR mSurface = VK_NULL_HANDLE;

        std::function<VkResult(VkSemaphore, VkFence, uint32_t&)> mAcquireFunc;
        std::function<VkResult(VkQueue, uint32_t, VkSemaphore)> mPresentFunc;
        uint32_t mWidth = 0, mHeight = 0;
        bool mFramebufferResized = false; 

        std::shared_ptr<RHI::ResourceManager> mResourceManager;

        // 异步读回槽位（initialize 创建，clear 释放）：持久命令缓冲 + fence。
        // 专用命令池（非 TRANSIENT）：persistent 缓冲需要 vkResetCommandBuffer 单发重置，
        // transient 池里的缓冲只能用 vkResetCommandPool 整池重置（VUID-vkResetCommandBuffer-00046）。
        // 必须与 FrameCapture::kSlots 一致（读回槽位环形缓存的并发上界 = 并发编码数）。
        static constexpr uint32_t kReadbackSlotCount = 8;
        VkCommandPool m_readbackPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_readbackCmd;
        std::vector<VkFence> m_readbackFence;
        // 每槽位是否 submit 过：从未 submit 的 fence 绝不能 vkWaitForFences（永不等信号 → 死锁）。
        // 无捕获/捕获帧数 < 槽位数时部分 fence 从未 submit，销毁时须跳过等待。
        std::vector<uint8_t> m_readbackSubmitted;
    private:
        RHI::RenderPassHandle mRenderPassHandle = RHI::RenderPassHandle::Null();
        RHI::TextureHandle mDepthTextureHandle = RHI::TextureHandle::Null();
        std::vector<RHI::FramebufferHandle> mFramebuffers;
        RHI::Format mDepthFormat = RHI::Format::Undefined;
    };
}
