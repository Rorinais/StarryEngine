#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <deque>
#include "VulkanRHI.hpp"

#ifdef _WIN32
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#elif defined(__APPLE__)
#include <vulkan/vulkan_macos.h>
#endif

namespace StarryEngine::RHI {
    VulkanBackend::VulkanBackend() = default;

    VulkanBackend::~VulkanBackend() {
        shutdown();
    }

    bool VulkanBackend::initialize(void* windowHandle, uint32_t width, uint32_t height) {
        if (mInitialized) {
            return true;
        }

        try {
            mConfig.nativeWindowHandle = windowHandle;
            mConfig.initialWidth = width;
            mConfig.initialHeight = height;

            // 1. 创建实例
            mInstance = Instance::create(mConfig.instanceConfig);
            if (!mInstance || !mInstance->getHandle()) {
                return false;
            }

            // 2. 创建表面
#ifdef _WIN32
            VkWin32SurfaceCreateInfoKHR surfaceInfo{};
            surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            surfaceInfo.hwnd = static_cast<HWND>(windowHandle);
            surfaceInfo.hinstance = GetModuleHandle(nullptr);

            if (vkCreateWin32SurfaceKHR(mInstance->getHandle(), &surfaceInfo, nullptr, &mSurface) != VK_SUCCESS) {
                return false;
            }
#endif

            // 3. 创建设备 - 使用你的Device组件
            mDevice = Device::create(mInstance, mSurface, mConfig.deviceConfig);
            if (!mDevice || !mDevice->getLogicalDevice()) {
                return false;
            }

            // 4. 初始化VMA - 使用你的Device的VMA功能
            if (!mDevice->initializeVMA()) {
                return false;
            }

            // 5. 创建交换链 - 使用你的SwapChain组件
            mSwapChain = SwapChain::create(mDevice, mSurface, mConfig.swapChainConfig);
            if (!mSwapChain || !mSwapChain->getHandle()) {
                return false;
            }

            // 6. 创建帧上下文 - 使用你的FrameContext组件
            auto queueFamilyIndices = mDevice->getQueueFamilyIndices();
            if (!queueFamilyIndices.graphicsFamily.has_value()) {
                return false;
            }

            mFrameContext = FrameContext::create(mDevice, mConfig.frameContextConfig);
            if (!mFrameContext->initialize(queueFamilyIndices.graphicsFamily.value())) {
                return false;
            }

            // 7. 创建传输命令池
            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
            poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

            if (vkCreateCommandPool(mDevice->getLogicalDevice(), &poolInfo, nullptr, &mTransferCommandPool) != VK_SUCCESS) {
                return false;
            }

            // 8. 加载调试函数
            if (mConfig.enableDebugMarkers) {
                auto device = mDevice->getLogicalDevice();
                mSetDebugUtilsObjectName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
                    vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
                mCmdBeginDebugLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
                    vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
                mCmdEndDebugLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
                    vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));
                mCmdInsertDebugLabel = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
                    vkGetDeviceProcAddr(device, "vkCmdInsertDebugUtilsLabelEXT"));
            }

            mInitialized = true;
            mStatistics.frameStartTime = std::chrono::high_resolution_clock::now();

            return true;

        }
        catch (const std::exception& e) {
            std::cerr << "Failed to initialize VulkanBackend: " << e.what() << std::endl;
            return false;
        }
    }

    void VulkanBackend::shutdown() {
        if (!mInitialized) {
            return;
        }

        waitIdle();

        // 清理所有资源
        for (auto& [id, data] : mPipelines) {
            vkDestroyPipeline(mDevice->getLogicalDevice(), data.pipeline, nullptr);
        }
        mPipelines.clear();

        for (auto& [id, data] : mPipelineLayouts) {
            vkDestroyPipelineLayout(mDevice->getLogicalDevice(), data.layout, nullptr);
            if (data.descriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(mDevice->getLogicalDevice(), data.descriptorSetLayout, nullptr);
            }
        }
        mPipelineLayouts.clear();

        for (auto& [id, data] : mShaderModules) {
            vkDestroyShaderModule(mDevice->getLogicalDevice(), data.module, nullptr);
        }
        mShaderModules.clear();

        for (auto& [id, data] : mSamplers) {
            vkDestroySampler(mDevice->getLogicalDevice(), data.sampler, nullptr);
        }
        mSamplers.clear();

        for (auto& [id, data] : mTextures) {
            vkDestroyImageView(mDevice->getLogicalDevice(), data.view, nullptr);
            mDevice->destroyImageWithVMA(data.image, data.allocation);
        }
        mTextures.clear();

        for (auto& [id, data] : mBuffers) {
            if (data.mappedData) {
                vmaUnmapMemory(mDevice->getAllocator(), data.allocation);
            }
            mDevice->destroyBufferWithVMA(data.buffer, data.allocation);
        }
        mBuffers.clear();

        // 清理命令池
        if (mTransferCommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(mDevice->getLogicalDevice(), mTransferCommandPool, nullptr);
            mTransferCommandPool = VK_NULL_HANDLE;
        }

        // 清理现有组件
        if (mFrameContext) {
            mFrameContext->cleanup();
            mFrameContext.reset();
        }

        if (mSwapChain) {
            mSwapChain.reset();
        }

        if (mDevice) {
            mDevice->cleanupVMA();
            mDevice.reset();
        }

        if (mSurface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(mInstance->getHandle(), mSurface, nullptr);
            mSurface = VK_NULL_HANDLE;
        }

        if (mInstance) {
            mInstance.reset();
        }

        mInitialized = false;
        mNextHandleId = 1;
    }

    bool VulkanBackend::beginFrame() {
        if (!mInitialized || !mSwapChain->isValid()) {
            return false;
        }

        // 使用FrameContext开始帧
        auto acquireFunc = [this](VkSemaphore semaphore, VkFence fence, uint32_t& imageIndex) {
            return mSwapChain->acquireNextImage(semaphore, fence, UINT64_MAX);
            };

        auto frameInfo = mFrameContext->beginFrame(acquireFunc);
        if (!frameInfo.commandBuffer) {
            return false;
        }

        mCommandState.currentCommandBuffer = frameInfo.commandBuffer;
        mIsRecording = true;

        // 开始调试标签
        if (mConfig.enableDebugMarkers && mCmdBeginDebugLabel) {
            VkDebugUtilsLabelEXT labelInfo{};
            labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            labelInfo.pLabelName = "Frame";
            labelInfo.color[0] = 0.2f;
            labelInfo.color[1] = 0.4f;
            labelInfo.color[2] = 0.8f;
            labelInfo.color[3] = 1.0f;
            mCmdBeginDebugLabel(mCommandState.currentCommandBuffer, &labelInfo);
        }

        // 重置命令状态
        mCommandState.boundPipeline = PipelineHandle{};
        mCommandState.boundVertexBuffers.clear();
        mCommandState.boundIndexBuffer = BufferHandle{};

        // 记录帧开始时间
        mStatistics.frameStartTime = std::chrono::high_resolution_clock::now();

        return true;
    }

    void VulkanBackend::endFrame() {
        if (!mIsRecording || !mCommandState.currentCommandBuffer) {
            return;
        }

        // 结束调试标签
        if (mConfig.enableDebugMarkers && mCmdEndDebugLabel) {
            mCmdEndDebugLabel(mCommandState.currentCommandBuffer);
        }

        // 使用FrameContext结束帧
        FrameContext::FrameInfo frameInfo;
        frameInfo.commandBuffer = mCommandState.currentCommandBuffer;
        mFrameContext->endFrame(frameInfo);

        mIsRecording = false;
        mCommandState.currentCommandBuffer = VK_NULL_HANDLE;

        // 计算帧时间
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        double frameTime = std::chrono::duration<double, std::milli>(frameEndTime - mStatistics.frameStartTime).count();

        mStatistics.frameTimes.push_back(frameTime);
        if (mStatistics.frameTimes.size() > 60) {
            mStatistics.frameTimes.pop_front();
        }

        double totalTime = 0.0;
        for (double time : mStatistics.frameTimes) {
            totalTime += time;
        }
        mStatistics.frameTime = totalTime / mStatistics.frameTimes.size();
        mStatistics.fps = static_cast<uint32_t>(1000.0 / mStatistics.frameTime);
        mStatistics.totalFrames++;
    }

    void VulkanBackend::present() {
        if (!mInitialized) {
            return;
        }

        // 使用FrameContext提交并呈现帧
        auto frameInfo = mFrameContext->getCurrentFrameInfo();
        auto presentFunc = [this](VkQueue queue, uint32_t imageIndex, VkSemaphore semaphore) {
            return mSwapChain->present(queue, imageIndex, semaphore);
            };

        mFrameContext->submitFrame(frameInfo, mDevice->getGraphicsQueue(), presentFunc);
    }

    void VulkanBackend::waitIdle() {
        if (mDevice) {
            vkDeviceWaitIdle(mDevice->getLogicalDevice());
        }
    }

    BufferHandle VulkanBackend::createBuffer(const BufferDesc& desc) {
        if (!mInitialized) {
            return {};
        }

        BufferHandle handle{ generateHandleId() };

        // 转换为Vulkan参数
        VkBufferUsageFlags usage = convertBufferUsage(desc.usage);
        VmaMemoryUsage memoryUsage = convertMemoryType(desc.memoryType);

        // 使用Device的VMA功能创建缓冲区
        VmaAllocationInfo allocInfo;
        auto vmaBuffer = mDevice->createBufferWithVMA(
            desc.size,
            usage,
            memoryUsage,
            0,  // flags
            &allocInfo
        );

        if (!vmaBuffer.buffer) {
            return {};
        }

        BufferData data;
        data.buffer = vmaBuffer.buffer;
        data.allocation = vmaBuffer.allocation;
        data.desc = desc;
        data.allocationSize = allocInfo.size;

        // 处理初始数据
        if (desc.initialData && desc.initialDataSize > 0) {
            if (desc.memoryType == MemoryType::GPU) {
                // 需要暂存缓冲区
                BufferDesc stagingDesc = desc;
                stagingDesc.memoryType = MemoryType::CPU_To_GPU;
                stagingDesc.allowUpdate = true;
                stagingDesc.debugName = desc.debugName + "_Staging";

                auto stagingHandle = createBuffer(stagingDesc);
                updateBuffer(stagingHandle, desc.initialData, 0, desc.initialDataSize);

                // 复制到GPU缓冲区
                copyBuffer(stagingHandle, handle, desc.initialDataSize);

                destroyBuffer(stagingHandle);
            }
            else {
                // 直接映射并复制
                void* mapped = mapBuffer(handle);
                if (mapped) {
                    memcpy(mapped, desc.initialData, desc.initialDataSize);
                    unmapBuffer(handle);
                }
            }
        }

        // 设置调试名称
        if (!desc.debugName.empty()) {
            setDebugName(handle, desc.debugName.c_str());
        }

        // 更新统计
        mStatistics.totalAllocatedMemory += data.allocationSize;
        mStatistics.peakMemoryUsage = std::max(mStatistics.peakMemoryUsage, mStatistics.totalAllocatedMemory);

        mBuffers[handle.id] = std::move(data);
        return handle;
    }

    void VulkanBackend::destroyBuffer(BufferHandle handle) {
        if (!handle.isValid()) {
            return;
        }

        auto it = mBuffers.find(handle.id);
        if (it != mBuffers.end()) {
            // 更新统计
            mStatistics.totalAllocatedMemory -= it->second.allocationSize;

            // 使用Device的功能销毁缓冲区
            mDevice->destroyBufferWithVMA(it->second.buffer, it->second.allocation);
            mBuffers.erase(it);
        }
    }

    TextureHandle VulkanBackend::createTexture(const TextureDesc& desc) {
        if (!mInitialized) {
            return {};
        }

        TextureHandle handle{ generateHandleId() };

        // 转换为Vulkan参数
        VkFormat format = convertFormat(desc.format);
        VkImageUsageFlags usage = convertTextureUsage(desc);

        // 计算mip等级
        uint32_t mipLevels = desc.generateMips ?
            calculateMipLevels(desc.width, desc.height) : desc.mipLevels;

        // 使用Device的VMA功能创建图像
        VmaAllocationInfo allocInfo;
        auto vmaImage = mDevice->createImageWithVMA(
            desc.width,
            desc.height,
            format,
            VK_IMAGE_TILING_OPTIMAL,
            usage,
            VMA_MEMORY_USAGE_GPU_ONLY,
            0,  // flags
            mipLevels,
            desc.arrayLayers
        );

        if (!vmaImage.image) {
            return {};
        }

        // 创建图像视图
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
        if (desc.type == TextureType::TextureCube) {
            viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        }
        else if (desc.type == TextureType::Texture3D) {
            viewType = VK_IMAGE_VIEW_TYPE_3D;
        }
        else if (desc.type == TextureType::Texture2DArray) {
            viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }

        VkImageView view = mDevice->createImageView(
            vmaImage.image,
            format,
            getImageAspectFlags(desc.format),
            viewType,
            mipLevels,
            0,  // baseArrayLayer
            desc.arrayLayers,
            desc.debugName.empty() ? nullptr : desc.debugName.c_str()
        );

        if (!view) {
            mDevice->destroyImageWithVMA(vmaImage.image, vmaImage.allocation);
            return {};
        }

        TextureData data;
        data.image = vmaImage.image;
        data.view = view;
        data.allocation = vmaImage.allocation;
        data.desc = desc;

        // 获取分配大小
        vmaGetAllocationInfo(mDevice->getAllocator(), vmaImage.allocation, &allocInfo);
        data.allocationSize = allocInfo.size;

        // 设置调试名称
        if (!desc.debugName.empty()) {
            setDebugName(handle, desc.debugName.c_str());
        }

        // 更新统计
        mStatistics.totalAllocatedMemory += data.allocationSize;
        mStatistics.peakMemoryUsage = std::max(mStatistics.peakMemoryUsage, mStatistics.totalAllocatedMemory);

        mTextures[handle.id] = std::move(data);
        return handle;
    }

    void VulkanBackend::destroyTexture(TextureHandle handle) {
        if (!handle.isValid()) {
            return;
        }

        auto it = mTextures.find(handle.id);
        if (it != mTextures.end()) {
            // 更新统计
            mStatistics.totalAllocatedMemory -= it->second.allocationSize;

            // 使用Device的功能销毁资源
            vkDestroyImageView(mDevice->getLogicalDevice(), it->second.view, nullptr);
            mDevice->destroyImageWithVMA(it->second.image, it->second.allocation);
            mTextures.erase(it);
        }
    }

    SamplerHandle VulkanBackend::createSampler(const SamplerDesc& desc) {
        if (!mInitialized) {
            return {};
        }

        SamplerHandle handle{ generateHandleId() };

        // 转换为Vulkan参数
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = convertSamplerFilter(desc.magFilter);
        samplerInfo.minFilter = convertSamplerFilter(desc.minFilter);
        samplerInfo.mipmapMode = (desc.mipFilter == SamplerFilter::Linear) ?
            VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = convertSamplerAddressMode(desc.addressU);
        samplerInfo.addressModeV = convertSamplerAddressMode(desc.addressV);
        samplerInfo.addressModeW = convertSamplerAddressMode(desc.addressW);
        samplerInfo.mipLodBias = desc.mipLodBias;
        samplerInfo.anisotropyEnable = (desc.maxAnisotropy > 1.0f) ? VK_TRUE : VK_FALSE;
        samplerInfo.maxAnisotropy = desc.maxAnisotropy;
        samplerInfo.compareEnable = desc.compareEnable ? VK_TRUE : VK_FALSE;
        samplerInfo.compareOp = convertCompareOp(desc.compareOp);
        samplerInfo.minLod = desc.minLod;
        samplerInfo.maxLod = desc.maxLod;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

        // 使用Device的功能创建采样器
        VkSampler sampler = mDevice->createSampler(
            samplerInfo.magFilter,
            samplerInfo.minFilter,
            samplerInfo.addressModeU,
            samplerInfo.addressModeV,
            samplerInfo.addressModeW,
            samplerInfo.anisotropyEnable,
            samplerInfo.maxAnisotropy,
            samplerInfo.compareEnable,
            samplerInfo.compareOp,
            samplerInfo.mipLodBias,
            samplerInfo.minLod,
            samplerInfo.maxLod
        );

        if (!sampler) {
            return {};
        }

        SamplerData data;
        data.sampler = sampler;
        data.desc = desc;

        // 设置调试名称
        if (!desc.debugName.empty()) {
            setDebugName(handle, desc.debugName.c_str());
        }

        mSamplers[handle.id] = std::move(data);
        return handle;
    }

    void VulkanBackend::destroySampler(SamplerHandle handle) {
        if (!handle.isValid()) {
            return;
        }

        auto it = mSamplers.find(handle.id);
        if (it != mSamplers.end()) {
            vkDestroySampler(mDevice->getLogicalDevice(), it->second.sampler, nullptr);
            mSamplers.erase(it);
        }
    }

    void VulkanBackend::updateBuffer(BufferHandle handle, const void* data, uint64_t offset, uint64_t size) {
        auto bufferData = getBufferData(handle);
        if (!bufferData || !data || size == 0) {
            return;
        }

        if (bufferData->desc.memoryType == MemoryType::GPU) {
            // 需要暂存缓冲区
            BufferDesc stagingDesc;
            stagingDesc.size = size;
            stagingDesc.usage = BufferUsage::TransferSrc;
            stagingDesc.memoryType = MemoryType::CPU_To_GPU;
            stagingDesc.allowUpdate = true;

            auto stagingHandle = createBuffer(stagingDesc);
            updateBuffer(stagingHandle, data, 0, size);

            copyBuffer(stagingHandle, handle, size, 0, offset);

            destroyBuffer(stagingHandle);
        }
        else {
            // 直接映射并更新
            void* mapped = mapBuffer(handle, offset, size);
            if (mapped) {
                memcpy(mapped, data, size);
                unmapBuffer(handle);
            }
        }
    }

    void* VulkanBackend::mapBuffer(BufferHandle handle, uint64_t offset, uint64_t size) {
        auto bufferData = getBufferData(handle);
        if (!bufferData || bufferData->desc.memoryType == MemoryType::GPU) {
            return nullptr;
        }

        if (bufferData->mappedData) {
            return static_cast<char*>(bufferData->mappedData) + offset;
        }

        void* mapped = nullptr;
        VkResult result = vmaMapMemory(mDevice->getAllocator(), bufferData->allocation, &mapped);
        if (result == VK_SUCCESS) {
            bufferData->mappedData = mapped;
            return static_cast<char*>(mapped) + offset;
        }

        return nullptr;
    }

    void VulkanBackend::unmapBuffer(BufferHandle handle) {
        auto bufferData = getBufferData(handle);
        if (!bufferData || !bufferData->mappedData) {
            return;
        }

        vmaUnmapMemory(mDevice->getAllocator(), bufferData->allocation);
        bufferData->mappedData = nullptr;
    }

    void VulkanBackend::beginCommandList() {
        // 在beginFrame中已经开始录制
        if (!mIsRecording) {
            beginFrame();
        }
    }

    void VulkanBackend::endCommandList() {
        // 命令列表在endFrame中结束
    }

    void VulkanBackend::setPipeline(PipelineHandle pipeline) {
        if (!mIsRecording || !pipeline.isValid()) {
            return;
        }

        auto pipelineData = getPipelineData(pipeline);
        if (!pipelineData) {
            return;
        }

        vkCmdBindPipeline(mCommandState.currentCommandBuffer,
            pipelineData->isCompute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineData->pipeline);

        mCommandState.boundPipeline = pipeline;

        // 重置绑定的资源
        mCommandState.boundVertexBuffers.clear();
        mCommandState.boundIndexBuffer = BufferHandle{};
    }

    void VulkanBackend::setVertexBuffer(BufferHandle buffer, uint32_t slot, uint64_t offset) {
        if (!mIsRecording || !buffer.isValid()) {
            return;
        }

        auto bufferData = getBufferData(buffer);
        if (!bufferData) {
            return;
        }

        VkBuffer vkBuffer = bufferData->buffer;
        VkDeviceSize vkOffset = offset;

        vkCmdBindVertexBuffers(mCommandState.currentCommandBuffer, slot, 1, &vkBuffer, &vkOffset);

        // 跟踪绑定的顶点缓冲区
        if (mCommandState.boundVertexBuffers.size() <= slot) {
            mCommandState.boundVertexBuffers.resize(slot + 1);
        }
        mCommandState.boundVertexBuffers[slot] = buffer;
    }

    void VulkanBackend::setIndexBuffer(BufferHandle buffer, uint64_t offset, bool use32Bit) {
        if (!mIsRecording || !buffer.isValid()) {
            return;
        }

        auto bufferData = getBufferData(buffer);
        if (!bufferData) {
            return;
        }

        VkIndexType indexType = use32Bit ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
        vkCmdBindIndexBuffer(mCommandState.currentCommandBuffer, bufferData->buffer, offset, indexType);

        mCommandState.boundIndexBuffer = buffer;
        mCommandState.isIndexBuffer32Bit = use32Bit;
        mCommandState.indexBufferOffset = offset;
    }

    void VulkanBackend::draw(uint32_t vertexCount, uint32_t instanceCount,
        uint32_t firstVertex, uint32_t firstInstance) {
        if (!mIsRecording) {
            return;
        }

        vkCmdDraw(mCommandState.currentCommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

        // 更新统计
        updateStatistics(1, vertexCount / 3);
    }

    void VulkanBackend::drawIndexed(uint32_t indexCount, uint32_t instanceCount,
        uint32_t firstIndex, int32_t vertexOffset,
        uint32_t firstInstance) {
        if (!mIsRecording) {
            return;
        }

        vkCmdDrawIndexed(mCommandState.currentCommandBuffer, indexCount, instanceCount,
            firstIndex, vertexOffset, firstInstance);

        // 更新统计
        updateStatistics(1, indexCount / 3);
    }

    uint32_t VulkanBackend::getCurrentFrameIndex() const {
        if (mFrameContext) {
            return mFrameContext->getCurrentFrameIndex();
        }
        return 0;
    }

    uint32_t VulkanBackend::getCurrentImageIndex() const {
        if (mSwapChain) {
            return mSwapChain->getCurrentImageIndex();
        }
        return 0;
    }

    uint32_t VulkanBackend::getMaxTextureSize() const {
        if (mDevice) {
            return mDevice->getPhysicalDeviceProperties().limits.maxImageDimension2D;
        }
        return 0;
    }

    uint32_t VulkanBackend::getMaxAnisotropy() const {
        if (mDevice) {
            return static_cast<uint32_t>(mDevice->getPhysicalDeviceProperties().limits.maxSamplerAnisotropy);
        }
        return 1;
    }

    bool VulkanBackend::supportsFormat(Format format) const {
        if (!mDevice) {
            return false;
        }

        VkFormat vkFormat = convertFormat(format);
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(mDevice->getPhysicalDevice(), vkFormat, &props);

        return props.optimalTilingFeatures != 0;
    }

    bool VulkanBackend::supportsFeature(const std::string& feature) const {
        if (!mDevice) {
            return false;
        }

        if (feature == "geometryShader") {
            return mDevice->supportsGeometryShader();
        }
        else if (feature == "tessellationShader") {
            return mDevice->supportsTessellationShader();
        }
        else if (feature == "anisotropy") {
            return mDevice->supportsAnisotropy();
        }
        else if (feature == "wideLines") {
            return mDevice->supportsWideLines();
        }

        return false;
    }

    uint64_t VulkanBackend::getTotalAllocatedMemory() const {
        return mStatistics.totalAllocatedMemory;
    }

    uint64_t VulkanBackend::getPeakMemoryUsage() const {
        return mStatistics.peakMemoryUsage;
    }

    uint64_t VulkanBackend::getDrawCallCount() const {
        return mStatistics.drawCallCount;
    }

    uint64_t VulkanBackend::getTriangleCount() const {
        return mStatistics.triangleCount;
    }

    double VulkanBackend::getFrameTime() const {
        return mStatistics.frameTime;
    }

    double VulkanBackend::getGPUTime() const {
        return mStatistics.gpuTime;
    }

    uint32_t VulkanBackend::getFPS() const {
        return mStatistics.fps;
    }

    void VulkanBackend::setDebugName(BufferHandle handle, const char* name) {
        if (!mSetDebugUtilsObjectName || !name) {
            return;
        }

        auto bufferData = getBufferData(handle);
        if (!bufferData) {
            return;
        }

        VkDebugUtilsObjectNameInfoEXT nameInfo{};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
        nameInfo.objectHandle = reinterpret_cast<uint64_t>(bufferData->buffer);
        nameInfo.pObjectName = name;

        mSetDebugUtilsObjectName(mDevice->getLogicalDevice(), &nameInfo);
    }

    void VulkanBackend::beginDebugLabel(const char* name, const float color[4]) {
        if (!mCmdBeginDebugLabel || !name || !mIsRecording) {
            return;
        }

        VkDebugUtilsLabelEXT labelInfo{};
        labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        labelInfo.pLabelName = name;
        if (color) {
            memcpy(labelInfo.color, color, sizeof(float) * 4);
        }
        else {
            labelInfo.color[0] = 0.5f;
            labelInfo.color[1] = 0.5f;
            labelInfo.color[2] = 0.5f;
            labelInfo.color[3] = 1.0f;
        }

        mCmdBeginDebugLabel(mCommandState.currentCommandBuffer, &labelInfo);
    }

    void VulkanBackend::endDebugLabel() {
        if (!mCmdEndDebugLabel || !mIsRecording) {
            return;
        }

        mCmdEndDebugLabel(mCommandState.currentCommandBuffer);
    }

    void* VulkanBackend::getNativeDevice() const {
        if (mDevice) {
            return mDevice->getLogicalDevice();
        }
        return nullptr;
    }

    void* VulkanBackend::getNativeCommandQueue() const {
        if (mDevice) {
            return mDevice->getGraphicsQueue();
        }
        return nullptr;
    }

    // 转换函数实现
    VkFormat VulkanBackend::convertFormat(Format format) const {
        switch (format) {
        case Format::R8_UNorm: return VK_FORMAT_R8_UNORM;
        case Format::RGBA8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::RGBA16_Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case Format::Depth32_Float: return VK_FORMAT_D32_SFLOAT;
        case Format::Depth24_Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
        case Format::BC1_RGB_UNorm: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case Format::BC3_UNorm: return VK_FORMAT_BC3_UNORM_BLOCK;
        default: return VK_FORMAT_UNDEFINED;
        }
    }

    VkBufferUsageFlags VulkanBackend::convertBufferUsage(BufferUsage usage) const {
        VkBufferUsageFlags flags = 0;

        if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(BufferUsage::Vertex)) {
            flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        }
        if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(BufferUsage::Index)) {
            flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        }
        if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(BufferUsage::Uniform)) {
            flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }
        if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(BufferUsage::Storage)) {
            flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        }
        if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(BufferUsage::Indirect)) {
            flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        }
        if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(BufferUsage::TransferSrc)) {
            flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }
        if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(BufferUsage::TransferDst)) {
            flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        return flags;
    }

    VmaMemoryUsage VulkanBackend::convertMemoryType(MemoryType type) const {
        switch (type) {
        case MemoryType::GPU: return VMA_MEMORY_USAGE_GPU_ONLY;
        case MemoryType::CPU_To_GPU: return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case MemoryType::CPU_Only: return VMA_MEMORY_USAGE_CPU_ONLY;
        case MemoryType::GPU_To_CPU: return VMA_MEMORY_USAGE_GPU_TO_CPU;
        default: return VMA_MEMORY_USAGE_UNKNOWN;
        }
    }

    uint64_t VulkanBackend::generateHandleId() {
        return mNextHandleId++;
    }

    VulkanBackend::BufferData* VulkanBackend::getBufferData(BufferHandle handle) {
        auto it = mBuffers.find(handle.id);
        return it != mBuffers.end() ? &it->second : nullptr;
    }

    VulkanBackend::TextureData* VulkanBackend::getTextureData(TextureHandle handle) {
        auto it = mTextures.find(handle.id);
        return it != mTextures.end() ? &it->second : nullptr;
    }

    void VulkanBackend::updateStatistics(uint64_t drawCalls, uint64_t triangleCount) {
        mStatistics.drawCallCount += drawCalls;
        mStatistics.triangleCount += triangleCount;
    }
} // namespace StarryEngine