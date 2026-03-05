#define VMA_IMPLEMENTATION
#include "Instance.hpp"
#include "Device.hpp"

namespace StarryEngine {

    // ==================== 构造函数和析构函数 ====================

    Device::Device(std::shared_ptr<Instance> instance, VkSurfaceKHR surface, const Config& config)
        : mInstance(instance)
        , mSurface(surface)
        , mConfig(config)
        , mQueueHandles(std::make_shared<QueueHandles>())
        , mInstanceApiVersion(instance->getConfig().apiVersion)
    {

        try {
            mPhysicalDevice = selectPhysicalDevice(surface);

            if (mPhysicalDevice == VK_NULL_HANDLE) {
                throw std::runtime_error("Failed to find suitable physical device!");
            }
            vkGetPhysicalDeviceProperties(mPhysicalDevice, &mProperties);
            vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mPhysicalDeviceFeatures);
            vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mMemoryProperties);

            mQueueFamilyIndices = findQueueFamilies(mPhysicalDevice, surface);

            if (!mQueueFamilyIndices.isComplete()) {
                std::cerr << "[ERROR] Queue families not complete:" << std::endl;
                std::cerr << "  Graphics: " << (mQueueFamilyIndices.graphicsFamily.has_value() ? "Yes" : "No") << std::endl;
                std::cerr << "  Present: " << (mQueueFamilyIndices.presentFamily.has_value() ? "Yes" : "No") << std::endl;
                throw std::runtime_error("Failed to find required queue families!");
            }

            createLogicalDevice();

            // 获取调试函数指针
            if (mConfig.enableValidation) {
                mSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)
                    vkGetDeviceProcAddr(mLogicalDevice, "vkSetDebugUtilsObjectNameEXT");
            }

            // 性能计数器也可以稍后初始化
            if (mConfig.enablePerformanceCounters) {
                std::cout << "[DEBUG] Performance counters will be initialized later" << std::endl;
            }

        }
        catch (const std::exception& e) {
            std::cerr << "[ERROR] Device construction failed: " << e.what() << std::endl;

            // 清理部分初始化的资源
            if (mLogicalDevice != VK_NULL_HANDLE) {
                vkDestroyDevice(mLogicalDevice, nullptr);
                mLogicalDevice = VK_NULL_HANDLE;
            }

            throw;
        }
    }

    Device::~Device() {
        // 等待设备空闲
        waitIdle();

        // 清理性能计数器
        if (mConfig.enablePerformanceCounters) {
            cleanupPerformanceCounters();
        }

        // 清理内存分配器
        cleanupVMA();

        if (mTransferCommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(mLogicalDevice, mTransferCommandPool, nullptr);
            mTransferCommandPool = VK_NULL_HANDLE;
        }

        // 销毁逻辑设备
        if (mLogicalDevice != VK_NULL_HANDLE) {
            vkDestroyDevice(mLogicalDevice, nullptr);
            mLogicalDevice = VK_NULL_HANDLE;
        }
    }

    // ==================== 内存分配器相关 ====================

    bool Device::initializeVMA() {
        if (mVmaAllocator != VK_NULL_HANDLE) {
            return true;
        }

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = mPhysicalDevice;
        allocatorInfo.device = mLogicalDevice;
        allocatorInfo.instance = mInstance->getHandle();

        allocatorInfo.vulkanApiVersion = mInstanceApiVersion;
        allocatorInfo.pVulkanFunctions = nullptr;
        allocatorInfo.flags = 0;

        VkResult result = vmaCreateAllocator(&allocatorInfo, &mVmaAllocator);
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create VMA allocator: " << result << std::endl;
            return false;
        }
        return true;
    }

    void Device::cleanupVMA() {
        if (mVmaAllocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(mVmaAllocator);
            mVmaAllocator = VK_NULL_HANDLE;
            if (mConfig.enableVMA) {
                std::cout << "[Device] VMA allocator destroyed" << std::endl;
            }
        }
    }

    // ==================== 缓冲区创建和管理 ====================

    // VMA 方式创建缓冲区
    VMABuffer Device::createBufferWithVMA(VkDeviceSize size, VkBufferUsageFlags usage,
        VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags,
        const void* initialData, size_t initialDataSize,
        VmaAllocationInfo* allocationInfo) {

        if (mVmaAllocator == VK_NULL_HANDLE) {
            throw std::runtime_error("VMA not initialized!");
        }

        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = memoryUsage;
        allocInfo.flags = flags;

        VkBuffer buffer;
        VmaAllocation allocation;

        VkResult result = vmaCreateBuffer(mVmaAllocator, &bufferInfo, &allocInfo,
            &buffer, &allocation, allocationInfo);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer with VMA!");
        }

        if (initialData && initialDataSize > 0) {
            uploadDataToVmaBuffer(buffer, allocation, initialData, initialDataSize);
        }

        return { buffer, allocation };
    }

    void Device::uploadDataToVmaBuffer(VkBuffer buffer, VmaAllocation allocation,
        const void* data, size_t dataSize, VkDeviceSize offset) {

        if (!data || dataSize == 0) {
            return;
        }

        void* mapped = nullptr;
        VkResult result = vmaMapMemory(mVmaAllocator, allocation, &mapped);
        if (result == VK_SUCCESS && mapped) {
            // 添加offset支持
            void* target = reinterpret_cast<uint8_t*>(mapped) + offset;
            memcpy(target, data, dataSize);

            VmaAllocationInfo allocInfo;
            vmaGetAllocationInfo(mVmaAllocator, allocation, &allocInfo);

            VkMemoryPropertyFlags memProperties;
            vmaGetMemoryTypeProperties(mVmaAllocator, allocInfo.memoryType, &memProperties);

            if (!(memProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                vmaFlushAllocation(mVmaAllocator, allocation, offset, dataSize);
            }

            vmaUnmapMemory(mVmaAllocator, allocation);
        }
        else {
            std::cerr << "Failed to map VMA memory: " << result << std::endl;
        }
    }

    void Device::destroyBufferWithVMA(const VMABuffer& buffer) {
        destroyBufferWithVMA(buffer.buffer, buffer.allocation);
    }

    void Device::destroyBufferWithVMA(VkBuffer buffer, VmaAllocation allocation) {
        if (mVmaAllocator != VK_NULL_HANDLE) {
            vmaDestroyBuffer(mVmaAllocator, buffer, allocation);
        }
    }

    void Device::uploadDataToTraditionalMemory(VkDeviceMemory memory, const void* data,
        size_t dataSize, VkDeviceSize offset,
        bool hostCoherent) {
        if (!data || dataSize == 0) return;

        void* mapped = nullptr;
        VkResult result = vkMapMemory(mLogicalDevice, memory, offset, dataSize, 0, &mapped);
        if (result == VK_SUCCESS && mapped) {
            memcpy(mapped, data, dataSize);

            if (!hostCoherent) {
                VkMappedMemoryRange mappedRange = {};
                mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
                mappedRange.memory = memory;
                mappedRange.offset = offset;
                mappedRange.size = dataSize;
                vkFlushMappedMemoryRanges(mLogicalDevice, 1, &mappedRange);
            }

            vkUnmapMemory(mLogicalDevice, memory);
        }
        else {
            std::cerr << "[Device] Failed to map memory: " << result << std::endl;
        }
    }

    void Device::uploadDataViaStagingBuffer(VkCommandPool commandPool,
        const VMATraditionalBuffer& dstBuffer,
        const void* data, size_t dataSize) {
        if (!data || dataSize == 0) {
            return;
        }

        try {
            // 1. 创建暂存缓冲区
            VMATraditionalBuffer stagingBuffer = createBufferTraditional(
                dataSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                data,      // 直接上传数据
                dataSize   // 数据大小
            );

            // 2. 复制缓冲区（从暂存缓冲区复制到目标缓冲区）
            copyBuffer(commandPool, stagingBuffer.buffer, dstBuffer.buffer, dataSize);

            // 3. 销毁暂存缓冲区
            destroyBufferTraditional(stagingBuffer);

        }
        catch (const std::exception& e) {
            std::cerr << "[Device] Failed to upload data via staging buffer: " << e.what() << std::endl;
            throw;
        }
    }

    // 传统方式创建缓冲区
    VMATraditionalBuffer Device::createBufferTraditional(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties, const void* initialData,
        size_t initialDataSize, VkCommandPool commandPool) {

        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buffer;
        if (vkCreateBuffer(mLogicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer!");
        }

        // 分配内存
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(mLogicalDevice, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        VkDeviceMemory bufferMemory;
        if (vkAllocateMemory(mLogicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            vkDestroyBuffer(mLogicalDevice, buffer, nullptr);
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        vkBindBufferMemory(mLogicalDevice, buffer, bufferMemory, 0);

        // 处理初始数据
        if (initialData && initialDataSize > 0) {
            // 如果是主机可见内存，直接映射上传
            if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
                bool hostCoherent = (properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
                uploadDataToTraditionalMemory(bufferMemory, initialData, initialDataSize, 0, hostCoherent);
            }
            // 如果是设备本地内存，需要暂存缓冲区
            else if (commandPool != VK_NULL_HANDLE) {
                uploadDataViaStagingBuffer(commandPool, { buffer, bufferMemory }, initialData, initialDataSize);
            }
            // 如果是设备本地内存但没有提供命令池，抛出异常
            else {
                vkFreeMemory(mLogicalDevice, bufferMemory, nullptr);
                vkDestroyBuffer(mLogicalDevice, buffer, nullptr);
                throw std::runtime_error("Command pool required for uploading data to device local memory!");
            }
        }

        return { buffer, bufferMemory };
    }

    // 传统方式创建缓冲区并上传数据
    VMATraditionalBuffer Device::createBufferTraditionalWithData(VkCommandPool commandPool,
        VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties, const void* initialData) {

        // 创建缓冲区
        auto buffer = createBufferTraditional(size, usage, properties);

        // 上传数据
        if (initialData) {
            if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
                bool hostCoherent = (properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
                uploadDataToTraditionalMemory(buffer.memory, initialData, size, 0, hostCoherent);
            }
            else {
                uploadDataViaStagingBuffer(commandPool, buffer, initialData, size);
            }
        }

        return buffer;
    }

    // 传统方式销毁缓冲区
    void Device::destroyBufferTraditional(const VMATraditionalBuffer& buffer) {
        destroyBufferTraditional(buffer.buffer, buffer.memory);
    }

    void Device::destroyBufferTraditional(VkBuffer buffer, VkDeviceMemory memory) {
        vkDestroyBuffer(mLogicalDevice, buffer, nullptr);
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(mLogicalDevice, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    }

    std::vector<VMABuffer> Device::createVmaBuffers(uint32_t count,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memoryUsage,
        VmaAllocationCreateFlags flags) {
        std::vector<VMABuffer> buffers;
        buffers.reserve(count);

        for (uint32_t i = 0; i < count; ++i) {
            buffers.push_back(createBufferWithVMA(size, usage, memoryUsage, flags));
        }

        return buffers;
    }

    std::vector<VMATraditionalBuffer> Device::createTraditionalBuffers(uint32_t count,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties) {
        std::vector<VMATraditionalBuffer> buffers;
        buffers.reserve(count);

        for (uint32_t i = 0; i < count; ++i) {
            buffers.push_back(createBufferTraditional(size, usage, properties));
        }

        return buffers;
    }

    void Device::destroyVmaBuffers(const std::vector<VMABuffer>& buffers) {
        for (const auto& buffer : buffers) {
            destroyBufferWithVMA(buffer);
        }
    }

    void Device::destroyTraditionalBuffers(const std::vector<VMATraditionalBuffer>& buffers) {
        for (const auto& buffer : buffers) {
            destroyBufferTraditional(buffer);
        }
    }

    // ==================== 图像创建和管理 ====================

    // VMA 方式创建图像
    VMAImage Device::createImageWithVMA(uint32_t width, uint32_t height, VkFormat format,
        VkImageTiling tiling, VkImageUsageFlags usage,
        VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags,
        uint32_t mipLevels, uint32_t arrayLayers) {

        VMAImage result = { VK_NULL_HANDLE, VK_NULL_HANDLE };

        if (mVmaAllocator == VK_NULL_HANDLE) {
            std::cerr << "[Device] VMA not initialized, cannot create image!" << std::endl;
            return result;
        }

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = arrayLayers;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = 0;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = memoryUsage;
        allocInfo.flags = flags;

        VkImage image;
        VmaAllocation allocation;

        VkResult res = vmaCreateImage(mVmaAllocator, &imageInfo, &allocInfo,
            &image, &allocation, nullptr);
        if (res != VK_SUCCESS) {
            std::cerr << "[Device] vmaCreateImage failed with code: " << res << std::endl;
            return result;
        }

        result.image = image;
        result.allocation = allocation;
        return result;
    }

    // 销毁 VMA 图像
    void Device::destroyImageWithVMA(const VMAImage& image) {
        destroyImageWithVMA(image.image, image.allocation);
    }

    void Device::destroyImageWithVMA(VkImage image, VmaAllocation allocation) {
        if (mVmaAllocator != VK_NULL_HANDLE) {
            vmaDestroyImage(mVmaAllocator, image, allocation);
        }
    }

    // 传统方式创建图像
    VMATraditionalImage Device::createImageTraditional(uint32_t width, uint32_t height, VkFormat format,
        VkImageTiling tiling, VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        uint32_t mipLevels, uint32_t arrayLayers) {

        VMATraditionalImage result = { VK_NULL_HANDLE, VK_NULL_HANDLE };

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = arrayLayers;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = 0;

        VkImage image;
        if (vkCreateImage(mLogicalDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            std::cerr << "[Device] vkCreateImage failed!" << std::endl;
            return result;
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(mLogicalDevice, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        VkDeviceMemory imageMemory;
        if (vkAllocateMemory(mLogicalDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            vkDestroyImage(mLogicalDevice, image, nullptr);
            std::cerr << "[Device] Failed to allocate image memory!" << std::endl;
            return result;
        }

        if (vkBindImageMemory(mLogicalDevice, image, imageMemory, 0) != VK_SUCCESS) {
            vkDestroyImage(mLogicalDevice, image, nullptr);
            vkFreeMemory(mLogicalDevice, imageMemory, nullptr);
            std::cerr << "[Device] Failed to bind image memory!" << std::endl;
            return result;
        }

        result.image = image;
        result.memory = imageMemory;
        return result;
    }

    // 销毁传统图像
    void Device::destroyImageTraditional(const VMATraditionalImage& image) {
        destroyImageTraditional(image.image, image.memory);
    }

    void Device::destroyImageTraditional(VkImage image, VkDeviceMemory memory) {
        vkDestroyImage(mLogicalDevice, image, nullptr);
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(mLogicalDevice, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    }

    // ==================== 图像视图管理 ====================

    VkImageView Device::createImageView(VkImage image,
        VkFormat format,
        VkImageAspectFlags aspectFlags,
        VkImageViewType viewType,
        uint32_t mipLevels,
        uint32_t baseArrayLayer,
        uint32_t layerCount,
        const char* debugName) {

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = viewType;
        viewInfo.format = format;

        // 组件映射
        viewInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };

        // 子资源范围
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;  // 固定为 0，让调用者通过参数控制
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
        viewInfo.subresourceRange.layerCount = layerCount;

        VkImageView imageView = VK_NULL_HANDLE;
        VkResult result = vkCreateImageView(mLogicalDevice, &viewInfo, nullptr, &imageView);

        if (result != VK_SUCCESS) {
            std::string errorMsg = "Failed to create image view";
            if (debugName) {
                errorMsg += std::string(" '") + debugName + "'";
            }
            errorMsg += ": " + std::to_string(result);
            throw std::runtime_error(errorMsg);
        }

        // 设置调试名称
        if (debugName && mSetDebugUtilsObjectNameEXT) {
            setObjectName(reinterpret_cast<uint64_t>(imageView),
                VK_OBJECT_TYPE_IMAGE_VIEW, debugName);
        }

        return imageView;
    }

    void Device::destroyImageView(VkImageView& imageView) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(mLogicalDevice, imageView, nullptr);
            imageView = VK_NULL_HANDLE;
        }
    }

	void Device::destroySwapChain(VkSwapchainKHR& swapchain) {
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(mLogicalDevice, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }
    }

    void Device::destroySurface(VkSurfaceKHR& surface) {
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(mInstance->getHandle(), surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
    }
    // ==================== 组合函数 ====================

    VMAImageFull Device::createImageWithVMAFull(uint32_t width, uint32_t height, VkFormat format,
        VkImageTiling tiling, VkImageUsageFlags usage,
        VmaMemoryUsage memoryUsage, VkImageAspectFlags aspectFlags,
        VmaAllocationCreateFlags flags,
        uint32_t mipLevels, uint32_t arrayLayers,
        VkImageViewType viewType) {

        VMAImage image = createImageWithVMA(width, height, format, tiling, usage,
            memoryUsage, flags, mipLevels, arrayLayers);

        VkImageView view = createImageView(image.image, format, aspectFlags,
            viewType, mipLevels, 0, arrayLayers);

        return { image.image, view, image.allocation };
    }

    void Device::destroyImageWithVMAFull(VMAImageFull& image) {
        destroyImageView(image.view);
        destroyImageWithVMA(image.image, image.allocation);
    }

    TraditionalImageFull Device::createImageTraditionalFull(uint32_t width, uint32_t height, VkFormat format,
        VkImageTiling tiling, VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties, VkImageAspectFlags aspectFlags,
        uint32_t mipLevels, uint32_t arrayLayers,
        VkImageViewType viewType) {

        VMATraditionalImage image = createImageTraditional(width, height, format, tiling, usage,
            properties, mipLevels, arrayLayers);

        VkImageView view = createImageView(image.image, format, aspectFlags,
            viewType, mipLevels, 0, arrayLayers);

        return { image.image, view, image.memory };
    }

    void Device::destroyImageTraditionalFull(TraditionalImageFull& image) {
        destroyImageView(image.view);
        destroyImageTraditional(image.image, image.memory);
    }

    // ==================== 帧缓冲创建和管理 ====================

    VkFramebuffer Device::createFramebuffer(VkRenderPass renderPass,
        const std::vector<VkImageView>& attachments,
        uint32_t width, uint32_t height, uint32_t layers) {

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = width;
        framebufferInfo.height = height;
        framebufferInfo.layers = layers;

        VkFramebuffer framebuffer;
        if (vkCreateFramebuffer(mLogicalDevice, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }

        return framebuffer;
    }

    void Device::destroyFramebuffer(VkFramebuffer& framebuffer) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(mLogicalDevice, framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
        }
    }

    // ==================== 渲染通道创建和管理 ====================

    VkRenderPass Device::createRenderPass(const std::vector<VkAttachmentDescription>& attachments,
        const std::vector<VkSubpassDescription>& subpasses,
        const std::vector<VkSubpassDependency>& dependencies) {
        
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
        renderPassInfo.pSubpasses = subpasses.data();
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        VkRenderPass renderPass = VK_NULL_HANDLE;
        if (vkCreateRenderPass(mLogicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass!");
        }

        return renderPass;
    }

    void Device::destroyRenderPass(VkRenderPass& renderPass) {
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(mLogicalDevice, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }
    }

    // ==================== 命令系统 ====================
    VkCommandPool Device::getTransferCommandPool() {
        if (mTransferCommandPool == VK_NULL_HANDLE) {
            VkCommandPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = getTransferQueueFamilyIndex();
            poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            
            if (vkCreateCommandPool(mLogicalDevice, &poolInfo, nullptr, &mTransferCommandPool) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create transfer command pool!");
            }
        }
        return mTransferCommandPool;
    }

    VkCommandPool Device::createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags) {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndex;
        poolInfo.flags = flags;

        VkCommandPool commandPool;
        if (vkCreateCommandPool(mLogicalDevice, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool!");
        }

        return commandPool;
    }

    void Device::destroyCommandPool(VkCommandPool& commandPool) {
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(mLogicalDevice, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }
    }

    void  Device::destroyQueryPool(VkQueryPool& queryPool) {
        if (queryPool != VK_NULL_HANDLE) {
            vkDestroyQueryPool(mLogicalDevice, queryPool, nullptr);
            queryPool = VK_NULL_HANDLE;
        }
    }


    VkCommandBuffer Device::allocateCommandBuffer(VkCommandPool& pool, VkCommandBufferLevel level) {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = pool;
        allocInfo.level = level;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        if (vkAllocateCommandBuffers(mLogicalDevice, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffer!");
        }

        return commandBuffer;
    }

    std::vector<VkCommandBuffer> Device::allocateCommandBuffers(VkCommandPool& pool,
        uint32_t count,
        VkCommandBufferLevel level) {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = pool;
        allocInfo.level = level;
        allocInfo.commandBufferCount = count;

        std::vector<VkCommandBuffer> commandBuffers(count);
        if (vkAllocateCommandBuffers(mLogicalDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers!");
        }

        return commandBuffers;
    }

    void Device::freeCommandBuffers(VkCommandPool& pool, const std::vector<VkCommandBuffer>& commandBuffers) {
        vkFreeCommandBuffers(mLogicalDevice, pool,
            static_cast<uint32_t>(commandBuffers.size()),
            commandBuffers.data());
    }

    // ==================== 管道创建和管理 ====================

    VkPipelineLayout Device::createPipelineLayout(const VkPipelineLayoutCreateInfo& pipelineLayoutInfo) {
        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(mLogicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        return pipelineLayout;
    }

    VkPipelineLayout Device::createPipelineLayout(const std::vector<VkDescriptorSetLayout>& setLayouts,
        const std::vector<VkPushConstantRange>& pushConstants) {

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
        pipelineLayoutInfo.pSetLayouts = setLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
        pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();

        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(mLogicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        return pipelineLayout;
    }

    void Device::destroyPipelineLayout(VkPipelineLayout& layout) {
        if (layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(mLogicalDevice, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    }

    VkPipeline Device::createGraphicsPipeline(const VkGraphicsPipelineCreateInfo& createInfo) {
        VkPipeline pipeline;
        if (vkCreateGraphicsPipelines(mLogicalDevice, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        return pipeline;
    }

    VkPipeline Device::createComputePipeline(const VkComputePipelineCreateInfo& createInfo) {
        VkPipeline pipeline;
        if (vkCreateComputePipelines(mLogicalDevice, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline!");
        }

        return pipeline;
    }

    void Device::destroyPipeline(VkPipeline& pipeline) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(mLogicalDevice, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
    }

    VkShaderModule Device::createShaderModule(const std::vector<uint32_t>& code, const std::string& debugName) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size() * sizeof(uint32_t);
        createInfo.pCode = code.data();

        VkShaderModule module;
        if (vkCreateShaderModule(mLogicalDevice, &createInfo, nullptr, &module) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module: " + debugName);
        }
        return module;
    }

    void Device::destroyShaderModule(VkShaderModule& module) {
        if (module!=VK_NULL_HANDLE){
            vkDestroyShaderModule(mLogicalDevice, module, nullptr);
        }
        module = VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo Device::createShaderStageInfo(
        VkShaderModule module,
        VkShaderStageFlagBits stage,
        const char* entryPoint) {
        return VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = stage,
            .module = module,
            .pName = entryPoint,
            .pSpecializationInfo = nullptr
        };
    }

    // ==================== 描述符系统 ====================

    VkDescriptorPool Device::createDescriptorPool(const std::vector<VkDescriptorPoolSize>& poolSizes,
        uint32_t maxSets) {

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = maxSets;

        VkDescriptorPool descriptorPool;
        if (vkCreateDescriptorPool(mLogicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }

        return descriptorPool;
    }

    void Device::destroyDescriptorPool(VkDescriptorPool& pool) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(mLogicalDevice, pool, nullptr);
            pool = VK_NULL_HANDLE;
        }
    }

    VkDescriptorSetLayout Device::createDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VkDescriptorSetLayout layout;
        if (vkCreateDescriptorSetLayout(mLogicalDevice, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }

        return layout;
    }

    void Device::destroyDescriptorSetLayout(VkDescriptorSetLayout& layout) {
        if (layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(mLogicalDevice, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    }

    VkDescriptorSet Device::allocateDescriptorSet(VkDescriptorPool pool, VkDescriptorSetLayout layout) {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSet descriptorSet;
        if (vkAllocateDescriptorSets(mLogicalDevice, &allocInfo, &descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor set!");
        }

        return descriptorSet;
    }

    void Device::updateDescriptorSet(VkDescriptorSet set,
        const std::vector<VkWriteDescriptorSet>& writes) {
        vkUpdateDescriptorSets(mLogicalDevice,
            static_cast<uint32_t>(writes.size()),
            writes.data(),
            0, nullptr);
    }

    // ==================== 同步对象 ====================

    VkSemaphore Device::createSemaphore() {
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreInfo.pNext = nullptr;
        semaphoreInfo.flags = 0;

        VkSemaphore semaphore;
        if (vkCreateSemaphore(mLogicalDevice, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create semaphore!");
        }
        return semaphore;
    }

    void Device::destroySemaphore(VkSemaphore& semaphore) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(mLogicalDevice, semaphore, nullptr);
            semaphore = VK_NULL_HANDLE;
        }
    }

    VkFence Device::createFence(VkFenceCreateFlags flags) {
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = flags;

        VkFence fence;
        if (vkCreateFence(mLogicalDevice, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create fence!");
        }
        return fence;
    }

    void Device::resetFence(VkFence& fence) {
        if (fence != VK_NULL_HANDLE) {
            vkResetFences(mLogicalDevice, 1, &fence);
        }
    }

    void Device::blockFence(VkFence& fence,uint64_t timeout) {
        if (fence != VK_NULL_HANDLE) {
            vkWaitForFences(mLogicalDevice, 1, &fence, VK_TRUE, timeout);
        }
    }

    void Device::destroyFence(VkFence& fence) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(mLogicalDevice, fence, nullptr);
            fence = VK_NULL_HANDLE;
        }
    }

    // ==================== 采样器 ====================

    VkSampler Device::createSampler(VkFilter magFilter, VkFilter minFilter,
        VkSamplerAddressMode addressModeU,
        VkSamplerAddressMode addressModeV,
        VkSamplerAddressMode addressModeW,
        bool anisotropyEnable, float maxAnisotropy,
        VkBool32 compareEnable, VkCompareOp compareOp,
        float mipLodBias, float minLod, float maxLod,
        VkBorderColor borderColor) {

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = magFilter;
        samplerInfo.minFilter = minFilter;
        samplerInfo.addressModeU = addressModeU;
        samplerInfo.addressModeV = addressModeV;
        samplerInfo.addressModeW = addressModeW;
        samplerInfo.anisotropyEnable = anisotropyEnable ? VK_TRUE : VK_FALSE;
        samplerInfo.maxAnisotropy = maxAnisotropy;
        samplerInfo.borderColor = borderColor;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = compareEnable;
        samplerInfo.compareOp = compareOp;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = mipLodBias;
        samplerInfo.minLod = minLod;
        samplerInfo.maxLod = maxLod;

        VkSampler sampler;
        if (vkCreateSampler(mLogicalDevice, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sampler!");
        }

        return sampler;
    }

    void Device::destroySampler(VkSampler&sampler) {
        if (sampler != VK_NULL_HANDLE) {
            vkDestroySampler(mLogicalDevice, sampler, nullptr);
            sampler = VK_NULL_HANDLE;
        }
    }

    // ==================== 实用功能 ====================

    VkCommandBuffer Device::beginSingleTimeCommands(VkCommandPool commandPool) {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(mLogicalDevice, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void Device::endSingleTimeCommands(VkCommandPool commandPool, VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(mQueueHandles->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(mQueueHandles->getGraphicsQueue());

        vkFreeCommandBuffers(mLogicalDevice, commandPool, 1, &commandBuffer);
    }

    void Device::copyBuffer(VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size){
        executeSingleTimeCommands(commandPool, [&](VkCommandBuffer cmd) {
            VkBufferCopy copyRegion = {};
            copyRegion.size = size;
            vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copyRegion);
        });
    }

    void Device::copyBufferToImage(
        VkCommandPool commandPool, 
        VkBuffer buffer, VkImage image,
        uint32_t width, uint32_t height,
        uint32_t layerCount) {
        executeSingleTimeCommands(commandPool, [&](VkCommandBuffer cmd) {
            VkBufferImageCopy region = {};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = layerCount;
            region.imageOffset = { 0, 0, 0 };
            region.imageExtent = { width, height, 1 };
            
            vkCmdCopyBufferToImage(cmd, buffer, image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        });
    }

    void Device::transitionImageLayout(
        VkCommandPool commandPool,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkImageAspectFlags aspectMask,
        uint32_t mipLevels,
        uint32_t layerCount,
        uint32_t baseMipLevel,
        uint32_t baseArrayLayer) {
        if (oldLayout == newLayout) return;
        
        executeSingleTimeCommands(commandPool, [&](VkCommandBuffer cmd) {
            VkAccessFlags srcAccessMask, dstAccessMask;
            VkPipelineStageFlags srcStageMask, dstStageMask;
            
            getLayoutTransitionInfo(oldLayout, newLayout,
                srcAccessMask, dstAccessMask,
                srcStageMask, dstStageMask);
            
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = oldLayout;
            barrier.newLayout = newLayout;
            barrier.srcAccessMask = srcAccessMask;
            barrier.dstAccessMask = dstAccessMask;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = aspectMask;
            barrier.subresourceRange.baseMipLevel = baseMipLevel;
            barrier.subresourceRange.levelCount = mipLevels;
            barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
            barrier.subresourceRange.layerCount = layerCount;
            
            vkCmdPipelineBarrier(cmd,
                srcStageMask, dstStageMask,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier);
        });
    }

    void Device::getLayoutTransitionInfo(
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkAccessFlags& srcAccessMask,
        VkAccessFlags& dstAccessMask,
        VkPipelineStageFlags& srcStageMask,
        VkPipelineStageFlags& dstStageMask) {

        // 常见布局转换
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            srcAccessMask = 0;
            dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            srcAccessMask = 0;
            dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            srcAccessMask = 0;
            dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            srcAccessMask = 0;
            dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
            srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dstAccessMask = 0;
            srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            srcAccessMask = 0;
            dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else {
            throw std::invalid_argument("Unsupported layout transition");
        }
    }

    void Device::generateMipmaps(
        VkCommandPool commandPool, 
        VkImage image, 
        VkFormat imageFormat,               
        int32_t width, 
        int32_t height, 
        uint32_t mipLevels) {
        
        // 检查图像格式是否支持线性过滤
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, imageFormat, &formatProperties);
        
        if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
            throw std::runtime_error("Texture image format does not support linear filtering!");
        }
        
        // 使用统一执行器
        executeSingleTimeCommands(commandPool, [&](VkCommandBuffer commandBuffer) {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.image = image;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.subresourceRange.levelCount = 1;
            
            int32_t mipWidth = width;
            int32_t mipHeight = height;
            
            for (uint32_t i = 1; i < mipLevels; i++) {
                // 将当前mip级别从TRANSFER_DST转换为TRANSFER_SRC
                barrier.subresourceRange.baseMipLevel = i - 1;
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                
                vkCmdPipelineBarrier(commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier);
                
                // 从当前mip级别blit到下一级
                VkImageBlit blit = {};
                blit.srcOffsets[0] = { 0, 0, 0 };
                blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
                blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.srcSubresource.mipLevel = i - 1;
                blit.srcSubresource.baseArrayLayer = 0;
                blit.srcSubresource.layerCount = 1;
                blit.dstOffsets[0] = { 0, 0, 0 };
                blit.dstOffsets[1] = { 
                    mipWidth > 1 ? mipWidth / 2 : 1, 
                    mipHeight > 1 ? mipHeight / 2 : 1, 
                    1 
                };
                blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.dstSubresource.mipLevel = i;
                blit.dstSubresource.baseArrayLayer = 0;
                blit.dstSubresource.layerCount = 1;
                
                vkCmdBlitImage(commandBuffer,
                    image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &blit,
                    VK_FILTER_LINEAR);
                
                // 将当前mip级别从TRANSFER_SRC转换为SHADER_READ_ONLY
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                
                vkCmdPipelineBarrier(commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier);
                
                // 更新下一级mip尺寸
                if (mipWidth > 1) mipWidth /= 2;
                if (mipHeight > 1) mipHeight /= 2;
            }
            
            // 将最后一个mip级别从TRANSFER_DST转换为SHADER_READ_ONLY
            barrier.subresourceRange.baseMipLevel = mipLevels - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            
            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);
        });
    }
    // ==================== 调试功能 ====================

    void Device::setObjectName(uint64_t object, VkObjectType objectType, const char* name) {
        if (mSetDebugUtilsObjectNameEXT && name) {
            VkDebugUtilsObjectNameInfoEXT nameInfo = {};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.objectType = objectType;
            nameInfo.objectHandle = object;
            nameInfo.pObjectName = name;
            mSetDebugUtilsObjectNameEXT(mLogicalDevice, &nameInfo);
        }
    }

    void Device::setBufferName(VkBuffer buffer, const char* name) {
        setObjectName(reinterpret_cast<uint64_t>(buffer), VK_OBJECT_TYPE_BUFFER, name);
    }

    void Device::setImageName(VkImage image, const char* name) {
        setObjectName(reinterpret_cast<uint64_t>(image), VK_OBJECT_TYPE_IMAGE, name);
    }

    // ==================== 等待和同步 ====================

    void Device::waitIdle() const {
        vkDeviceWaitIdle(mLogicalDevice);
    }

    // ==================== 物理设备选择相关 ====================

    VkPhysicalDevice Device::selectPhysicalDevice(VkSurfaceKHR surface) {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(mInstance->getHandle(), &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(mInstance->getHandle(), &deviceCount, physicalDevices.data());

        // 使用评分系统选择最佳设备
        std::vector<std::pair<int32_t, VkPhysicalDevice>> ratedDevices;

        for (const auto& device : physicalDevices) {
            int32_t score = ratePhysicalDevice(device, surface);
            if (score > 0) {
                ratedDevices.emplace_back(score, device);
            }
        }

        if (ratedDevices.empty()) {
            throw std::runtime_error("Failed to find a suitable GPU!");
        }

        // 按评分排序，选择最高分的设备
        std::sort(ratedDevices.begin(), ratedDevices.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        return ratedDevices[0].second;
    }

    int32_t Device::ratePhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR surface) {
        // 基本检查
        if (!isPhysicalDeviceSuitable(device, surface)) {
            return 0;
        }

        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceProperties(device, &properties);
        vkGetPhysicalDeviceFeatures(device, &features);

        int32_t score = 0;

        // 设备类型评分
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000; // 独显优先
        }
        else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 500; // 集显次之
        }
        else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) {
            score += 300;
        }
        else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
            score += 100;
        }

        // 纹理大小限制
        score += properties.limits.maxImageDimension2D / 1000;

        // 特征支持
        if (features.geometryShader) score += 100;
        if (features.tessellationShader) score += 100;
        if (features.samplerAnisotropy) score += 100;
        if (features.multiViewport) score += 50;
        if (features.wideLines) score += 50;
        if (features.fillModeNonSolid) score += 50;

        return score;
    }

    bool Device::isPhysicalDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
        // 检查队列族支持
        QueueFamilyIndices indices = findQueueFamilies(device, surface);
        if (!indices.isComplete()) {
            return false;
        }

        // 检查扩展支持
        bool extensionsSupported = checkDeviceExtensionSupport(device);
        if (!extensionsSupported) {
            return false;
        }

        // 检查交换链支持
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
        if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()) {
            return false;
        }

        // 检查设备特性
        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        // 如果配置要求特定特性但设备不支持，则不合适
        if (mConfig.samplerAnisotropy && !supportedFeatures.samplerAnisotropy) {
            return false;
        }
        if (mConfig.geometryShader && !supportedFeatures.geometryShader) {
            return false;
        }
        if (mConfig.tessellationShader && !supportedFeatures.tessellationShader) {
            return false;
        }

        return true;
    }

    bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device) const {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(mConfig.extensions.begin(), mConfig.extensions.end());
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilies.size(); i++) {
            // 图形队列
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            // 呈现队列
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }

            // 计算队列（可选，优先选择独立的计算队列）
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                if (!indices.computeFamily.has_value() ||
                    (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
                    indices.computeFamily = i;
                }
            }

            // 传输队列（可选，优先选择独立的传输队列）
            if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
                if (!indices.transferFamily.has_value() ||
                    ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 &&
                        (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)) {
                    indices.transferFamily = i;
                }
            }

            // 如果已经找到所有需要的队列，可以提前退出
            if (indices.isComplete() &&
                (!mConfig.extensions.empty() ||
                    indices.computeFamily.has_value()) &&
                indices.transferFamily.has_value()) {
                break;
            }
        }

        return indices;
    }

    void Device::createLogicalDevice() {
        // 创建队列创建信息
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = mQueueFamilyIndices.getUniqueFamilies();

        // 为每个唯一的队列族创建队列信息
        std::vector<float> queuePriorities;

        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;

            // 为每个队列族创建一个优先级数组
            queuePriorities.push_back(mConfig.queuePriority);
            queueCreateInfo.pQueuePriorities = &queuePriorities.back();

            queueCreateInfos.push_back(queueCreateInfo);
        }

        // 配置设备特性 - 只启用设备实际支持的特性
        VkPhysicalDeviceFeatures deviceFeatures{};

        // 检查并启用实际支持的特性
        if (mConfig.samplerAnisotropy && mPhysicalDeviceFeatures.samplerAnisotropy) {
            deviceFeatures.samplerAnisotropy = VK_TRUE;
        }

        if (mConfig.geometryShader && mPhysicalDeviceFeatures.geometryShader) {
            deviceFeatures.geometryShader = VK_TRUE;
        }

        if (mConfig.tessellationShader && mPhysicalDeviceFeatures.tessellationShader) {
            deviceFeatures.tessellationShader = VK_TRUE;
        }

        if (mConfig.fillModeNonSolid && mPhysicalDeviceFeatures.fillModeNonSolid) {
            deviceFeatures.fillModeNonSolid = VK_TRUE;
        }

        if (mConfig.wideLines && mPhysicalDeviceFeatures.wideLines) {
            deviceFeatures.wideLines = VK_TRUE;
        }

        // ==================== 扩展处理 - 修复重复问题 ====================
        std::vector<const char*> enabledExtensions;
        std::set<std::string> uniqueExtensionSet;

        // 先去重
        for (const char* extension : mConfig.extensions) {
            if (extension != nullptr && strlen(extension) > 0) {
                std::string extStr(extension);
                if (uniqueExtensionSet.find(extStr) == uniqueExtensionSet.end()) {
                    uniqueExtensionSet.insert(extStr);
                }
            }
        }

        // 获取所有可用的扩展
        uint32_t availableExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &availableExtensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions;
        if (availableExtensionCount > 0) {
            availableExtensions.resize(availableExtensionCount);
            vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &availableExtensionCount, availableExtensions.data());
        }

        // 检查每个扩展是否支持
        for (const std::string& extension : uniqueExtensionSet) {
            bool supported = false;

            for (const auto& availableExt : availableExtensions) {
                if (strcmp(availableExt.extensionName, extension.c_str()) == 0) {
                    supported = true;
                    break;
                }
            }

            if (supported) {
                enabledExtensions.push_back(extension.c_str());
            }
            else {
                // 只对关键扩展抛出错误，非关键扩展只警告
                bool isCritical = (extension == VK_KHR_SWAPCHAIN_EXTENSION_NAME);
                if (isCritical) {
                    throw std::runtime_error("Critical device extension not supported: " + extension);
                }
                else {
                    std::cerr << "[WARNING] Device extension not supported: " << extension
                        << ", but continuing without it." << std::endl;
                }
            }
        }

        // 确保至少有一个扩展被启用
        if (enabledExtensions.empty()) {
            throw std::runtime_error("No extensions enabled for device!");
        }

        // ==================== 验证层处理 ====================
        // 设备级别的验证层在现代Vulkan中通常不需要
        // 为了安全，我们只在配置要求时启用
        std::vector<const char*> enabledLayers;

        if (mConfig.enableValidation && !mInstance->getConfig().validationLayers.empty()) {
            // 检查设备是否支持这些验证层
            uint32_t layerCount = 0;
            vkEnumerateDeviceLayerProperties(mPhysicalDevice, &layerCount, nullptr);

            std::vector<VkLayerProperties> availableLayers;
            if (layerCount > 0) {
                availableLayers.resize(layerCount);
                vkEnumerateDeviceLayerProperties(mPhysicalDevice, &layerCount, availableLayers.data());
            }

            // 检查每个验证层是否可用
            for (const char* layerName : mInstance->getConfig().validationLayers) {
                bool layerAvailable = false;

                for (const auto& layerProperties : availableLayers) {
                    if (strcmp(layerName, layerProperties.layerName) == 0) {
                        layerAvailable = true;
                        break;
                    }
                }

                if (layerAvailable) {
                    enabledLayers.push_back(layerName);
                }
                else {
                    std::cerr << "[WARNING] Device validation layer not available: " << layerName << std::endl;
                }
            }
        }

        // ==================== 创建设备 ====================
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();

        // 设置验证层
        if (!enabledLayers.empty()) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(enabledLayers.size());
            createInfo.ppEnabledLayerNames = enabledLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
            // 重要：设置为nullptr，而不是空数组
            createInfo.ppEnabledLayerNames = nullptr;
        }

        VkResult result = vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mLogicalDevice);
        if (result != VK_SUCCESS) {
            std::string errorMsg = "Failed to create logical device! Error code: ";
            errorMsg += std::to_string(result);

            // 详细的错误信息
            switch (result) {
            case VK_ERROR_OUT_OF_HOST_MEMORY:
                errorMsg += " (VK_ERROR_OUT_OF_HOST_MEMORY)";
                break;
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                errorMsg += " (VK_ERROR_OUT_OF_DEVICE_MEMORY)";
                break;
            case VK_ERROR_INITIALIZATION_FAILED:
                errorMsg += " (VK_ERROR_INITIALIZATION_FAILED)";
                break;
            case VK_ERROR_EXTENSION_NOT_PRESENT:
                errorMsg += " (VK_ERROR_EXTENSION_NOT_PRESENT)";
                break;
            case VK_ERROR_FEATURE_NOT_PRESENT:
                errorMsg += " (VK_ERROR_FEATURE_NOT_PRESENT)";
                break;
            case VK_ERROR_TOO_MANY_OBJECTS:
                errorMsg += " (VK_ERROR_TOO_MANY_OBJECTS)";
                break;
            case VK_ERROR_DEVICE_LOST:
                errorMsg += " (VK_ERROR_DEVICE_LOST)";
                break;
            default:
                errorMsg += " (Unknown error)";
                break;
            }

            throw std::runtime_error(errorMsg);
        }

        // ==================== 获取队列句柄 ====================
        // 确保 mQueueHandles 已经初始化
        if (!mQueueHandles) {
            throw std::runtime_error("QueueHandles not initialized in createLogicalDevice!");
        }

        // 获取图形队列
        if (mQueueFamilyIndices.graphicsFamily.has_value()) {
            VkQueue graphicsQueue = VK_NULL_HANDLE;
            vkGetDeviceQueue(mLogicalDevice, mQueueFamilyIndices.graphicsFamily.value(), 0, &graphicsQueue);
            if (graphicsQueue != VK_NULL_HANDLE) {
                mQueueHandles->addQueue(QueueHandles::QueueType::Graphics,
                    graphicsQueue,
                    mQueueFamilyIndices.graphicsFamily.value(),
                    0,
                    mConfig.queuePriority);
            }
            else {
                std::cerr << "[WARNING] Failed to get graphics queue" << std::endl;
            }
        }

        // 获取呈现队列（可能与图形队列相同）
        if (mQueueFamilyIndices.presentFamily.has_value()) {
            VkQueue presentQueue = VK_NULL_HANDLE;
            vkGetDeviceQueue(mLogicalDevice, mQueueFamilyIndices.presentFamily.value(), 0, &presentQueue);
            if (presentQueue != VK_NULL_HANDLE) {
                mQueueHandles->addQueue(QueueHandles::QueueType::Present,
                    presentQueue,
                    mQueueFamilyIndices.presentFamily.value(),
                    0,
                    mConfig.queuePriority);
            }
            else {
                std::cerr << "[WARNING] Failed to get present queue" << std::endl;
            }
        }

        // 如果有独立的计算队列，获取它
        if (mQueueFamilyIndices.computeFamily.has_value() &&
            mQueueFamilyIndices.computeFamily.value() != mQueueFamilyIndices.graphicsFamily.value()) {
            VkQueue computeQueue = VK_NULL_HANDLE;
            vkGetDeviceQueue(mLogicalDevice, mQueueFamilyIndices.computeFamily.value(), 0, &computeQueue);
            if (computeQueue != VK_NULL_HANDLE) {
                mQueueHandles->addQueue(QueueHandles::QueueType::Compute,
                    computeQueue,
                    mQueueFamilyIndices.computeFamily.value(),
                    0,
                    mConfig.queuePriority);
                std::cout << "[DEBUG] Compute queue obtained" << std::endl;
            }
        }

        // 如果有独立的传输队列，获取它
        if (mQueueFamilyIndices.transferFamily.has_value() &&
            mQueueFamilyIndices.transferFamily.value() != mQueueFamilyIndices.graphicsFamily.value() &&
            mQueueFamilyIndices.transferFamily.value() != mQueueFamilyIndices.computeFamily.value()) {
            VkQueue transferQueue = VK_NULL_HANDLE;
            vkGetDeviceQueue(mLogicalDevice, mQueueFamilyIndices.transferFamily.value(), 0, &transferQueue);
            if (transferQueue != VK_NULL_HANDLE) {
                mQueueHandles->addQueue(QueueHandles::QueueType::Transfer,
                    transferQueue,
                    mQueueFamilyIndices.transferFamily.value(),
                    0,
                    mConfig.queuePriority);
                std::cout << "[DEBUG] Transfer queue obtained" << std::endl;
            }
        }
    }
    // ==================== 查询功能 ====================

    VkPhysicalDeviceFeatures Device::getPhysicalDeviceFeatures() const {
        return mPhysicalDeviceFeatures;
    }

    VkPhysicalDeviceMemoryProperties Device::getPhysicalDeviceMemoryProperties() const {
        return mMemoryProperties;
    }

    VkFormat Device::findSupportedFormat(const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features) const {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR &&
                (props.linearTilingFeatures & features) == features) {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("Failed to find supported format!");
    }

    VkFormat Device::findDepthFormat() const {
        return findSupportedFormat(
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    SwapChainSupportDetails Device::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    SwapChainSupportDetails Device::querySwapChainSupport() const {
        return querySwapChainSupport(mPhysicalDevice, mSurface);
    }

    uint32_t Device::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
        for (uint32_t i = 0; i < mMemoryProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) &&
                (mMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type!");
    }

    bool Device::isExtensionSupported(const char* extensionName) const {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &extensionCount, availableExtensions.data());

        for (const auto& extension : availableExtensions) {
            if (strcmp(extension.extensionName, extensionName) == 0) {
                return true;
            }
        }

        return false;
    }

    // ==================== 性能计数器 ====================

    void Device::initializePerformanceCounters() {
#ifdef VK_EXT_PERFORMANCE_QUERY_EXTENSION_NAME
        mPerformanceCounterSupported = isExtensionSupported(VK_EXT_PERFORMANCE_QUERY_EXTENSION_NAME);
#else
        mPerformanceCounterSupported = isExtensionSupported("VK_EXT_performance_query");
#endif

        if (mPerformanceCounterSupported) {
            std::cout << "[INFO] Performance counters are supported and enabled." << std::endl;
        }
        else {
            std::cout << "[INFO] Performance counters are not supported." << std::endl;
        }
    }

    void Device::cleanupPerformanceCounters() {
        // 清理性能计数器相关资源
        mPerformanceCounterSupported = false;
    }

    std::vector<uint64_t> Device::getPerformanceCounterValues(QueueHandles::QueueType queueType,
        const std::vector<uint32_t>& counterIndices) {
        if (!mPerformanceCounterSupported) {
            throw std::runtime_error("Performance counters are not supported!");
        }

        // 这里应该实现具体的性能计数器查询逻辑
        // 由于Vulkan性能计数器API相对复杂，这里返回空向量作为占位符
        return std::vector<uint64_t>(counterIndices.size(), 0);
    }

    // ==================== 调试和统计 ====================

    void Device::printDeviceInfo() const {
        std::cout << "=== Device Information ===" << std::endl;
        std::cout << "Device Name: " << mProperties.deviceName << std::endl;
        std::cout << "Device Type: ";

        switch (mProperties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            std::cout << "Discrete GPU";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            std::cout << "Integrated GPU";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            std::cout << "Virtual GPU";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            std::cout << "CPU";
            break;
        default:
            std::cout << "Other";
            break;
        }

        std::cout << std::endl;
        std::cout << "API Version: "
            << VK_VERSION_MAJOR(mProperties.apiVersion) << "."
            << VK_VERSION_MINOR(mProperties.apiVersion) << "."
            << VK_VERSION_PATCH(mProperties.apiVersion) << std::endl;

        std::cout << "Driver Version: " << mProperties.driverVersion << std::endl;
        std::cout << "Vendor ID: 0x" << std::hex << mProperties.vendorID << std::dec << std::endl;
        std::cout << "Device ID: 0x" << std::hex << mProperties.deviceID << std::dec << std::endl;

        std::cout << "\n=== Device Features ===" << std::endl;
        std::cout << "Geometry Shader: " << (mPhysicalDeviceFeatures.geometryShader ? "Yes" : "No") << std::endl;
        std::cout << "Tessellation Shader: " << (mPhysicalDeviceFeatures.tessellationShader ? "Yes" : "No") << std::endl;
        std::cout << "Sampler Anisotropy: " << (mPhysicalDeviceFeatures.samplerAnisotropy ? "Yes" : "No") << std::endl;
        std::cout << "Wide Lines: " << (mPhysicalDeviceFeatures.wideLines ? "Yes" : "No") << std::endl;
        std::cout << "Fill Mode Non-Solid: " << (mPhysicalDeviceFeatures.fillModeNonSolid ? "Yes" : "No") << std::endl;

        std::cout << "\n=== Device Limits ===" << std::endl;
        std::cout << "Max Image Dimension 2D: " << mProperties.limits.maxImageDimension2D << std::endl;
        std::cout << "Max Uniform Buffer Range: " << mProperties.limits.maxUniformBufferRange << std::endl;
        std::cout << "Max Bound Descriptor Sets: " << mProperties.limits.maxBoundDescriptorSets << std::endl;

        std::cout << "\n=== Memory Properties ===" << std::endl;
        std::cout << "Memory Heap Count: " << mMemoryProperties.memoryHeapCount << std::endl;
        for (uint32_t i = 0; i < mMemoryProperties.memoryHeapCount; i++) {
            std::cout << "  Heap " << i << ": Size=" << mMemoryProperties.memoryHeaps[i].size
                << " bytes, Flags=0x" << std::hex << mMemoryProperties.memoryHeaps[i].flags << std::dec << std::endl;
        }

        std::cout << "\n=== Queue Families ===" << std::endl;
        std::cout << "Graphics Family: " << (mQueueFamilyIndices.graphicsFamily.has_value() ?
            std::to_string(mQueueFamilyIndices.graphicsFamily.value()) : "N/A") << std::endl;
        std::cout << "Present Family: " << (mQueueFamilyIndices.presentFamily.has_value() ?
            std::to_string(mQueueFamilyIndices.presentFamily.value()) : "N/A") << std::endl;
        std::cout << "Compute Family: " << (mQueueFamilyIndices.computeFamily.has_value() ?
            std::to_string(mQueueFamilyIndices.computeFamily.value()) : "N/A") << std::endl;
        std::cout << "Transfer Family: " << (mQueueFamilyIndices.transferFamily.has_value() ?
            std::to_string(mQueueFamilyIndices.transferFamily.value()) : "N/A") << std::endl;

        std::cout << "\n=== Enabled Extensions ===" << std::endl;
        for (const auto& extension : mConfig.extensions) {
            std::cout << "  " << extension << std::endl;
        }
        std::cout << std::endl; 
    }
}