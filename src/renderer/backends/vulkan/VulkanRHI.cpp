#include "VulkanRHI.hpp"
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <algorithm>

namespace StarryEngine::RHI {

    // ==================== 构造函数/析构函数 ====================
    VulkanRHI::VulkanRHI() {
        // 初始化统计
        mStatistics.frameStartTime = std::chrono::high_resolution_clock::now();
    }

    VulkanRHI::~VulkanRHI() {
        if (mInitialized) {
            shutdown();
        }
    }

    // ==================== 初始化/清理 ====================
    bool VulkanRHI::initialize(const RHIInitConfig& config) {
        if (mInitialized) {
            return true;
        }

        mConfig = config;

        try {
            // 1. 创建 Vulkan 实例
            Instance::Config instanceConfig;
            instanceConfig.appName = config.appName;
            instanceConfig.appVersion = config.appVersion;
            instanceConfig.engineName = config.engineName;
            instanceConfig.engineVersion = config.engineVersion;
            instanceConfig.enableValidationLayers = config.enableValidation;

            mInstance = Instance::create(instanceConfig);
            if (!mInstance) {
                throw std::runtime_error("Failed to create Vulkan instance");
            }

            // 2. 创建设备
            Device::Config deviceConfig;
            deviceConfig.enableValidation = config.enableValidation;
            deviceConfig.extensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                VK_KHR_MAINTENANCE1_EXTENSION_NAME
            };

            // 创建表面
            VkSurfaceKHR surface = VK_NULL_HANDLE;
            if (config.nativeWindowHandle) {
                // 根据平台创建表面
#ifdef _WIN32
                VkWin32SurfaceCreateInfoKHR createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
                createInfo.hwnd = static_cast<HWND>(config.nativeWindowHandle);
                createInfo.hinstance = GetModuleHandle(nullptr);
                if (vkCreateWin32SurfaceKHR(mInstance->getVkInstance(), &createInfo, nullptr, &surface) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create window surface");
                }
#elif defined(__linux__)
    // Linux 平台实现
#endif
            }

            mDevice = Device::create(mInstance, surface, deviceConfig);
            if (!mDevice) {
                throw std::runtime_error("Failed to create Vulkan device");
            }

            // 3. 创建交换链
            SwapChainConfig swapChainConfig;
            swapChainConfig.width = config.initialWidth;
            swapChainConfig.height = config.initialHeight;
            swapChainConfig.vsync = config.vsyncEnabled;

            mSwapChain = SwapChain::create(mDevice, swapChainConfig);
            if (!mSwapChain) {
                throw std::runtime_error("Failed to create swap chain");
            }

            // 4. 创建帧上下文
            FrameContext::Config frameConfig;
            frameConfig.maxFramesInFlight = config.maxFramesInFlight;

            mFrameContext = FrameContext::create(mDevice, mSwapChain, frameConfig);
            if (!mFrameContext) {
                throw std::runtime_error("Failed to create frame context");
            }

            // 5. 创建同步对象
            createSyncObjects();

            // 6. 初始化队列
            auto queueHandles = mDevice->getQueueHandles();
            mGraphicsQueue = new RHIQueue(); // 简化实现，实际需要完整实现
            mPresentQueue = new RHIQueue();

            mInitialized = true;
            return true;

        }
        catch (const std::exception& e) {
            if (mErrorCallback) {
                mErrorCallback(e.what());
            }
            return false;
        }
    }

    void VulkanRHI::shutdown() {
        if (!mInitialized) {
            return;
        }

        waitIdle();

        // 清理同步对象
        cleanupSyncObjects();

        // 清理资源
        for (auto& buffer : mBuffers) {
            destroyBufferInternal(buffer.get());
        }
        mBuffers.clear();

        for (auto& texture : mTextures) {
            destroyTextureInternal(texture.get());
        }
        mTextures.clear();

        for (auto& sampler : mSamplers) {
            destroySamplerInternal(sampler.get());
        }
        mSamplers.clear();

        // 清理组件
        mFrameContext.reset();
        mSwapChain.reset();
        mDevice.reset();
        mInstance.reset();

        delete mGraphicsQueue;
        delete mPresentQueue;

        mInitialized = false;
    }

    // ==================== 帧管理 ====================
    bool VulkanRHI::beginFrame() {
        if (!mInitialized) {
            return false;
        }

        // 等待上一帧完成
        vkWaitForFences(mDevice->getLogicalDevice(), 1, &mInFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(mDevice->getLogicalDevice(), 1, &mInFlightFence);

        // 获取下一张交换链图像
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(
            mDevice->getLogicalDevice(),
            mSwapChain->getSwapchain(),
            UINT64_MAX,
            mImageAvailableSemaphore,
            VK_NULL_HANDLE,
            &imageIndex
        );

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            // 交换链需要重建
            mSwapChain->recreate(mDevice);
            return false;
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to acquire swap chain image");
        }

        // 更新帧上下文
        mFrameContext->beginFrame(imageIndex);

        // 更新统计
        auto currentTime = std::chrono::high_resolution_clock::now();
        mStatistics.frameTime = std::chrono::duration<double, std::milli>(
            currentTime - mStatistics.frameStartTime).count();
        mStatistics.frameStartTime = currentTime;

        mStatistics.frameTimeHistory.push_back(mStatistics.frameTime);
        if (mStatistics.frameTimeHistory.size() > 60) {
            mStatistics.frameTimeHistory.pop_front();
        }

        // 计算 FPS
        if (!mStatistics.frameTimeHistory.empty()) {
            double totalTime = 0;
            for (double time : mStatistics.frameTimeHistory) {
                totalTime += time;
            }
            mStatistics.fps = static_cast<uint32_t>(1000.0 / (totalTime / mStatistics.frameTimeHistory.size()));
        }

        return true;
    }

    void VulkanRHI::endFrame() {
        if (!mInitialized) {
            return;
        }

        mFrameContext->endFrame();
    }

    void VulkanRHI::present() {
        if (!mInitialized) {
            return;
        }

        // 呈现提交
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &mRenderFinishedSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &mSwapChain->getSwapchain();

        uint32_t imageIndex = mFrameContext->getCurrentImageIndex();
        presentInfo.pImageIndices = &imageIndex;

        VkResult result = vkQueuePresentKHR(mDevice->getPresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            mSwapChain->recreate(mDevice);
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swap chain image");
        }

        // 递增帧索引
        mFrameContext->advanceFrame();
    }

    void VulkanRHI::waitIdle() {
        if (mDevice) {
            vkDeviceWaitIdle(mDevice->getLogicalDevice());
        }
    }

    void VulkanRHI::waitForGPU() {
        waitIdle();
    }

    // ==================== 缓冲区创建 ====================
    RHIBuffer* VulkanRHI::createBuffer(const BufferDesc& desc) {
        return createBufferInternal(desc);
    }

    VulkanRHI::VulkanBuffer* VulkanRHI::createBufferInternal(const BufferDesc& desc) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = desc.size;
        bufferInfo.usage = convertBufferUsage(desc.usage);
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = convertMemoryTypeToVMA(desc.memoryType);

        if (desc.cpuAccess & CPUAccessFlag::Read) {
            allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        }
        if (desc.cpuAccess & CPUAccessFlag::Write) {
            allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        }

        auto buffer = std::make_unique<VulkanBuffer>();
        buffer->desc = desc;

        VkResult result = vmaCreateBuffer(
            mDevice->getAllocator(),
            &bufferInfo,
            &allocInfo,
            &buffer->buffer,
            &buffer->allocation,
            &buffer->allocationSize
        );

        if (result != VK_SUCCESS) {
            return nullptr;
        }

        // 更新内存统计
        mStatistics.totalAllocatedMemory += buffer->allocationSize;
        mStatistics.peakMemoryUsage = std::max(
            mStatistics.peakMemoryUsage,
            mStatistics.totalAllocatedMemory
        );

        // 如果需要初始数据，则映射并填充
        if (desc.initialData && desc.initialDataSize > 0) {
            void* mappedData;
            vmaMapMemory(mDevice->getAllocator(), buffer->allocation, &mappedData);
            memcpy(mappedData, desc.initialData, desc.initialDataSize);
            vmaUnmapMemory(mDevice->getAllocator(), buffer->allocation);
        }

        // 设置调试名称
        if (!desc.name.empty() && mDebugEnabled) {
            mDevice->setBufferName(buffer->buffer, desc.name.c_str());
        }

        auto ptr = buffer.get();
        mBuffers.push_back(std::move(buffer));
        return ptr;
    }

    void VulkanRHI::destroyBuffer(RHIBuffer* buffer) {
        if (auto vulkanBuffer = dynamic_cast<VulkanBuffer*>(buffer)) {
            destroyBufferInternal(vulkanBuffer);
        }
    }

    void VulkanRHI::destroyBufferInternal(VulkanBuffer* buffer) {
        if (buffer && buffer->buffer != VK_NULL_HANDLE) {
            // 等待设备空闲，确保缓冲区不再使用
            waitIdle();

            // 如果已映射，取消映射
            if (buffer->mappedData) {
                vmaUnmapMemory(mDevice->getAllocator(), buffer->allocation);
                buffer->mappedData = nullptr;
            }

            // 销毁缓冲区
            vmaDestroyBuffer(mDevice->getAllocator(), buffer->buffer, buffer->allocation);

            // 更新内存统计
            mStatistics.totalAllocatedMemory -= buffer->allocationSize;

            // 从容器中移除
            auto it = std::find_if(mBuffers.begin(), mBuffers.end(),
                [buffer](const auto& ptr) { return ptr.get() == buffer; });
            if (it != mBuffers.end()) {
                mBuffers.erase(it);
            }
        }
    }

    // ==================== VulkanBuffer 成员函数实现 ====================
    void* VulkanRHI::VulkanBuffer::map(uint64_t offset, uint64_t size) {
        if (mappedData) {
            return static_cast<uint8_t*>(mappedData) + offset;
        }

        VmaAllocator allocator = VulkanRHI::getInstance()->getDevice()->getAllocator();
        vmaMapMemory(allocator, allocation, &mappedData);
        return static_cast<uint8_t*>(mappedData) + offset;
    }

    void VulkanRHI::VulkanBuffer::unmap() {
        if (mappedData) {
            VmaAllocator allocator = VulkanRHI::getInstance()->getDevice()->getAllocator();
            vmaUnmapMemory(allocator, allocation);
            mappedData = nullptr;
        }
    }

    void VulkanRHI::VulkanBuffer::update(const void* data, uint64_t size, uint64_t offset) {
        if (!data || size == 0) {
            return;
        }

        size = (size == 0) ? desc.size - offset : size;
        size = std::min(size, desc.size - offset);

        void* dst = map(offset, size);
        memcpy(dst, data, size);
        unmap();
    }

    void VulkanRHI::VulkanBuffer::flush(uint64_t offset, uint64_t size) {
        VmaAllocator allocator = VulkanRHI::getInstance()->getDevice()->getAllocator();
        vmaFlushAllocation(allocator, allocation, offset, size);
    }

    void VulkanRHI::VulkanBuffer::invalidate(uint64_t offset, uint64_t size) {
        VmaAllocator allocator = VulkanRHI::getInstance()->getDevice()->getAllocator();
        vmaInvalidateAllocation(allocator, allocation, offset, size);
    }

    void* VulkanRHI::VulkanBuffer::createView(Format format, uint64_t offset, uint64_t size) {
        // 创建缓冲区视图
        VkBufferViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        viewInfo.buffer = buffer;
        viewInfo.format = VulkanRHI::getInstance()->convertFormat(format);
        viewInfo.offset = offset;
        viewInfo.range = (size == 0) ? desc.size - offset : size;

        VkBufferView bufferView;
        VkDevice device = VulkanRHI::getInstance()->getDevice()->getLogicalDevice();
        if (vkCreateBufferView(device, &viewInfo, nullptr, &bufferView) != VK_SUCCESS) {
            return nullptr;
        }

        return reinterpret_cast<void*>(bufferView);
    }

    void VulkanRHI::VulkanBuffer::destroyView(void* view) {
        VkBufferView bufferView = reinterpret_cast<VkBufferView>(view);
        VkDevice device = VulkanRHI::getInstance()->getDevice()->getLogicalDevice();
        vkDestroyBufferView(device, bufferView, nullptr);
    }

    void VulkanRHI::VulkanBuffer::transitionState(AccessFlag newAccess, PipelineStage newStage) {
        // 实现资源屏障
        // 简化实现，实际需要命令缓冲区
    }

    // ==================== 纹理创建 ====================
    RHITexture* VulkanRHI::createTexture(const TextureDesc& desc) {
        return createTextureInternal(desc);
    }

    VulkanRHI::VulkanTexture* VulkanRHI::createTextureInternal(const TextureDesc& desc) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = convertImageType(desc.type);
        imageInfo.extent.width = desc.extent.width;
        imageInfo.extent.height = desc.extent.height;
        imageInfo.extent.depth = desc.extent.depth;
        imageInfo.mipLevels = desc.mipLevels;
        imageInfo.arrayLayers = desc.arrayLayers;
        imageInfo.format = convertFormat(desc.format);
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = convertImageUsage(desc);
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = convertSampleCount(desc.sampleCount);

        // 如果生成mipmaps，需要适当的转移操作
        if (desc.generateMips) {
            imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        auto texture = std::make_unique<VulkanTexture>();
        texture->desc = desc;

        VkResult result = vmaCreateImage(
            mDevice->getAllocator(),
            &imageInfo,
            &allocInfo,
            &texture->image,
            &texture->allocation,
            &texture->allocationSize
        );

        if (result != VK_SUCCESS) {
            return nullptr;
        }

        // 创建默认图像视图
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = texture->image;
        viewInfo.viewType = convertImageViewType(desc.type, desc.arrayLayers);
        viewInfo.format = convertFormat(desc.format);
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        if (isDepthFormat(desc.format)) {
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (hasStencilComponent(desc.format)) {
                viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }

        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = desc.mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = desc.arrayLayers;

        if (vkCreateImageView(mDevice->getLogicalDevice(), &viewInfo, nullptr, &texture->defaultView) != VK_SUCCESS) {
            vmaDestroyImage(mDevice->getAllocator(), texture->image, texture->allocation);
            return nullptr;
        }

        // 更新内存统计
        mStatistics.totalAllocatedMemory += texture->allocationSize;
        mStatistics.peakMemoryUsage = std::max(
            mStatistics.peakMemoryUsage,
            mStatistics.totalAllocatedMemory
        );

        // 设置调试名称
        if (!desc.name.empty() && mDebugEnabled) {
            mDevice->setImageName(texture->image, desc.name.c_str());
        }

        // 如果需要初始数据，上传数据
        if (desc.initialData) {
            // 创建临时缓冲区上传数据
            // 简化实现，实际需要完整的传输操作
        }

        auto ptr = texture.get();
        mTextures.push_back(std::move(texture));
        return ptr;
    }

    void VulkanRHI::destroyTexture(RHITexture* texture) {
        if (auto vulkanTexture = dynamic_cast<VulkanTexture*>(texture)) {
            destroyTextureInternal(vulkanTexture);
        }
    }

    void VulkanRHI::destroyTextureInternal(VulkanTexture* texture) {
        if (texture && texture->image != VK_NULL_HANDLE) {
            waitIdle();

            VkDevice device = mDevice->getLogicalDevice();

            // 销毁额外视图
            for (VkImageView view : texture->additionalViews) {
                vkDestroyImageView(device, view, nullptr);
            }
            texture->additionalViews.clear();

            // 销毁默认视图
            if (texture->defaultView != VK_NULL_HANDLE) {
                vkDestroyImageView(device, texture->defaultView, nullptr);
            }

            // 销毁图像
            vmaDestroyImage(mDevice->getAllocator(), texture->image, texture->allocation);

            // 更新内存统计
            mStatistics.totalAllocatedMemory -= texture->allocationSize;

            // 从容器中移除
            auto it = std::find_if(mTextures.begin(), mTextures.end(),
                [texture](const auto& ptr) { return ptr.get() == texture; });
            if (it != mTextures.end()) {
                mTextures.erase(it);
            }
        }
    }

    // ==================== VulkanTexture 成员函数实现 ====================
    ImageLayout VulkanRHI::VulkanTexture::getCurrentLayout() const {
        // 简化实现，实际需要跟踪布局状态
        return ImageLayout::Undefined;
    }

    void* VulkanRHI::VulkanTexture::createView(ImageAspect aspect, uint32_t baseMipLevel,
        uint32_t levelCount, uint32_t baseArrayLayer,
        uint32_t layerCount) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VulkanRHI::getInstance()->convertImageViewType(desc.type, desc.arrayLayers);
        viewInfo.format = VulkanRHI::getInstance()->convertFormat(desc.format);

        // 设置 aspect mask
        VkImageAspectFlags aspectMask = 0;
        if (aspect & ImageAspect::Color) aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
        if (aspect & ImageAspect::Depth) aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if (aspect & ImageAspect::Stencil) aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

        viewInfo.subresourceRange.aspectMask = aspectMask;
        viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
        viewInfo.subresourceRange.levelCount = levelCount;
        viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
        viewInfo.subresourceRange.layerCount = layerCount;

        VkImageView imageView;
        VkDevice device = VulkanRHI::getInstance()->getDevice()->getLogicalDevice();
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            return nullptr;
        }

        additionalViews.insert(imageView);
        return reinterpret_cast<void*>(imageView);
    }

    void VulkanRHI::VulkanTexture::destroyView(void* view) {
        VkImageView imageView = reinterpret_cast<VkImageView>(view);
        VkDevice device = VulkanRHI::getInstance()->getDevice()->getLogicalDevice();
        vkDestroyImageView(device, imageView, nullptr);
        additionalViews.erase(imageView);
    }

    void VulkanRHI::VulkanTexture::generateMipmaps() {
        // 实现 mipmap 生成
        auto device = VulkanRHI::getInstance()->getDevice();
        VkCommandBuffer commandBuffer = device->beginSingleTimeCommands(device->getGraphicsCommandPool());

        // 生成 mipmap 的转移操作
        // ...

        device->endSingleTimeCommands(device->getGraphicsCommandPool(), commandBuffer);
    }

    void VulkanRHI::VulkanTexture::transitionLayout(ImageLayout newLayout, PipelineStage srcStage,
        PipelineStage dstStage, AccessFlag srcAccess,
        AccessFlag dstAccess, uint32_t baseMipLevel,
        uint32_t levelCount, uint32_t baseArrayLayer,
        uint32_t layerCount) {
        // 实现布局转移
        auto device = VulkanRHI::getInstance()->getDevice();
        VkCommandBuffer commandBuffer = device->beginSingleTimeCommands(device->getGraphicsCommandPool());

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VulkanRHI::getInstance()->convertImageLayout(getCurrentLayout());
        barrier.newLayout = VulkanRHI::getInstance()->convertImageLayout(newLayout);
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;

        // 设置 aspect mask
        VkFormat format = VulkanRHI::getInstance()->convertFormat(desc.format);
        if (VulkanRHI::getInstance()->isDepthFormat(desc.format)) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (VulkanRHI::getInstance()->hasStencilComponent(desc.format)) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        barrier.subresourceRange.baseMipLevel = baseMipLevel;
        barrier.subresourceRange.levelCount = levelCount;
        barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
        barrier.subresourceRange.layerCount = layerCount;
        barrier.srcAccessMask = VulkanRHI::getInstance()->convertAccessFlags(srcAccess);
        barrier.dstAccessMask = VulkanRHI::getInstance()->convertAccessFlags(dstAccess);

        VkPipelineStageFlags sourceStage = VulkanRHI::getInstance()->convertPipelineStage(srcStage);
        VkPipelineStageFlags destinationStage = VulkanRHI::getInstance()->convertPipelineStage(dstStage);

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        device->endSingleTimeCommands(device->getGraphicsCommandPool(), commandBuffer);
    }

    // ==================== 类型转换函数 ====================
    VkFormat VulkanRHI::convertFormat(Format format) const {
        switch (format) {
        case Format::R8_UNORM: return VK_FORMAT_R8_UNORM;
        case Format::R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case Format::B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
        case Format::R32G32B32A32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::D32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
        case Format::D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
        case Format::D32_SFLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        default: return VK_FORMAT_UNDEFINED;
        }
    }

    VkBufferUsageFlags VulkanRHI::convertBufferUsage(BufferUsage usage) const {
        VkBufferUsageFlags flags = 0;

        if (usage & BufferUsage::Vertex) flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (usage & BufferUsage::Index) flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (usage & BufferUsage::Uniform) flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (usage & BufferUsage::Storage) flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (usage & BufferUsage::Indirect) flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        if (usage & BufferUsage::TransferSrc) flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if (usage & BufferUsage::TransferDst) flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        return flags;
    }

    VkImageUsageFlags VulkanRHI::convertImageUsage(const TextureDesc& desc) const {
        VkImageUsageFlags usage = 0;

        if (desc.usage & TextureUsage::Sampled) usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if (desc.usage & TextureUsage::Storage) usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        if (desc.usage & TextureUsage::ColorAttachment) usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (desc.usage & TextureUsage::DepthStencilAttachment) usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (desc.usage & TextureUsage::TransferSrc) usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (desc.usage & TextureUsage::TransferDst) usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        return usage;
    }

    VkImageType VulkanRHI::convertImageType(TextureType type) const {
        switch (type) {
        case TextureType::Texture1D: return VK_IMAGE_TYPE_1D;
        case TextureType::Texture2D: return VK_IMAGE_TYPE_2D;
        case TextureType::Texture3D: return VK_IMAGE_TYPE_3D;
        default: return VK_IMAGE_TYPE_2D;
        }
    }

    VkImageViewType VulkanRHI::convertImageViewType(TextureType type, uint32_t arrayLayers) const {
        switch (type) {
        case TextureType::Texture1D:
            return (arrayLayers > 1) ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        case TextureType::Texture2D:
            return (arrayLayers > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        case TextureType::Texture3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        case TextureType::TextureCube:
            return (arrayLayers > 6) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
        default:
            return VK_IMAGE_VIEW_TYPE_2D;
        }
    }

    // ==================== 同步对象管理 ====================
    void VulkanRHI::createSyncObjects() {
        VkDevice device = mDevice->getLogicalDevice();

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 初始状态为已发出

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &mImageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &mRenderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &mInFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects");
        }
    }

    void VulkanRHI::cleanupSyncObjects() {
        VkDevice device = mDevice->getLogicalDevice();

        if (mImageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, mImageAvailableSemaphore, nullptr);
            mImageAvailableSemaphore = VK_NULL_HANDLE;
        }

        if (mRenderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, mRenderFinishedSemaphore, nullptr);
            mRenderFinishedSemaphore = VK_NULL_HANDLE;
        }

        if (mInFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(device, mInFlightFence, nullptr);
            mInFlightFence = VK_NULL_HANDLE;
        }
    }

    // ==================== 统计信息 ====================
    uint64_t VulkanRHI::getTotalAllocatedMemory() const {
        return mStatistics.totalAllocatedMemory;
    }

    uint64_t VulkanRHI::getPeakMemoryUsage() const {
        return mStatistics.peakMemoryUsage;
    }

    uint64_t VulkanRHI::getDrawCallCount() const {
        return mStatistics.drawCallCount;
    }

    uint64_t VulkanRHI::getTriangleCount() const {
        return mStatistics.triangleCount;
    }

    double VulkanRHI::getFrameTime() const {
        return mStatistics.frameTime;
    }

    uint32_t VulkanRHI::getFPS() const {
        return mStatistics.fps;
    }

    // ==================== 辅助函数 ====================
    bool VulkanRHI::isDepthFormat(Format format) const {
        switch (format) {
        case Format::D32_SFLOAT:
        case Format::D24_UNORM_S8_UINT:
        case Format::D32_SFLOAT_S8_UINT:
            return true;
        default:
            return false;
        }
    }

    bool VulkanRHI::hasStencilComponent(Format format) const {
        switch (format) {
        case Format::D24_UNORM_S8_UINT:
        case Format::D32_SFLOAT_S8_UINT:
            return true;
        default:
            return false;
        }
    }

    VmaAllocationCreateFlags VulkanRHI::convertMemoryTypeToVMA(MemoryType type) const {
        switch (type) {
        case MemoryType::DeviceLocal:
            return 0; // VMA 会自动选择设备本地内存
        case MemoryType::HostVisible:
            return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT;
        case MemoryType::HostCoherent:
            return VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT;
        default:
            return 0;
        }
    }

    // ==================== 单例访问（简化实现） ====================
    VulkanRHI* VulkanRHI::getInstance() {
        static VulkanRHI instance;
        return &instance;
    }

} // namespace StarryEngine::RHI