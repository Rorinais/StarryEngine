#define VMA_IMPLEMENTATION
#include "Instance.hpp"
#include "Device.hpp"

namespace StarryEngine {

    // ==================== 构造函数和析构函数 ====================

    Device::Device(std::shared_ptr<Instance> instance, VkSurfaceKHR surface, const Config& config)
        : mInstance(instance), mSurface(surface), mConfig(config) {

        // 选择物理设备
        mPhysicalDevice = selectPhysicalDevice(surface);

        // 获取物理设备属性
        vkGetPhysicalDeviceProperties(mPhysicalDevice, &mProperties);
        vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mPhysicalDeviceFeatures);
        vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mMemoryProperties);

        // 查找队列族
        mQueueFamilyIndices = findQueueFamilies(mPhysicalDevice, surface);

        // 创建逻辑设备
        createLogicalDevice();

        // 初始化VMA内存分配器
        if (mConfig.enableVMA) {
            initializeVMA();
        }

        // 获取调试函数指针
        mSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)
            vkGetDeviceProcAddr(mLogicalDevice, "vkSetDebugUtilsObjectNameEXT");

        // 初始化性能计数器
        if (mConfig.enablePerformanceCounters) {
            initializePerformanceCounters();
        }

        // 设置队列句柄的设备指针
        mQueueHandles->setDevice(this);
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

        // 销毁逻辑设备
        if (mLogicalDevice != VK_NULL_HANDLE) {
            vkDestroyDevice(mLogicalDevice, nullptr);
            mLogicalDevice = VK_NULL_HANDLE;
        }
    }

    // ==================== 内存分配器相关 ====================

    bool Device::initializeVMA() {
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.vulkanApiVersion = getApiVersion();
        allocatorInfo.physicalDevice = mPhysicalDevice;
        allocatorInfo.device = mLogicalDevice;
        allocatorInfo.instance = mInstance->getHandle();
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

        VkResult result = vmaCreateAllocator(&allocatorInfo, &mVmaAllocator);
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create VMA allocator: " << result << std::endl;
            return false;
        }

        std::cout << "VMA allocator created successfully" << std::endl;
        return true;
    }

    void Device::cleanupVMA() {
        if (mVmaAllocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(mVmaAllocator);
            mVmaAllocator = VK_NULL_HANDLE;
            std::cout << "VMA allocator destroyed" << std::endl;
        }
    }

    // ==================== 缓冲区创建和管理 ====================

    // VMA 方式创建缓冲区
    VMABuffer Device::createBufferWithVMA(VkDeviceSize size, VkBufferUsageFlags usage,
        VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags,
        const void* initialData, size_t initialDataSize) {

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

        if (vmaCreateBuffer(mVmaAllocator, &bufferInfo, &allocInfo,
            &buffer, &allocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer with VMA!");
        }

        // 如果有初始数据，上传
        if (initialData && initialDataSize > 0) {
            uploadDataToVmaBuffer(buffer, allocation, initialData, initialDataSize);
        }

        return { buffer, allocation };
    }

    VMABuffer Device::createBufferWithVMA(VkDeviceSize size, VkBufferUsageFlags usage,
        VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags,
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

        if (vmaCreateBuffer(mVmaAllocator, &bufferInfo, &allocInfo,
            &buffer, &allocation, allocationInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer with VMA!");
        }

        return { buffer, allocation };
    }

    void Device::uploadDataToVmaBuffer(VkBuffer buffer, VmaAllocation allocation,
        const void* data, size_t dataSize) {

        void* mapped;
        if (vmaMapMemory(mVmaAllocator, allocation, &mapped) == VK_SUCCESS) {
            memcpy(mapped, data, dataSize);

            // 获取内存属性来判断是否需要刷新
            VmaAllocationInfo allocInfo;
            vmaGetAllocationInfo(mVmaAllocator, allocation, &allocInfo);

            // 获取内存类型属性
            VkMemoryPropertyFlags memProperties;
            vmaGetMemoryTypeProperties(mVmaAllocator, allocInfo.memoryType, &memProperties);

            // 检查是否不是主机一致的内存，需要手动刷新
            if (!(memProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                vmaFlushAllocation(mVmaAllocator, allocation, 0, dataSize);
            }

            vmaUnmapMemory(mVmaAllocator, allocation);
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

    // 传统方式创建缓冲区
    VMATraditionalBuffer Device::createBufferTraditional(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties) {

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
                // 直接映射上传
                void* mapped;
                vkMapMemory(mLogicalDevice, buffer.memory, 0, size, 0, &mapped);
                memcpy(mapped, initialData, size);

                if (!(properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                    VkMappedMemoryRange mappedRange = {};
                    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
                    mappedRange.memory = buffer.memory;
                    mappedRange.offset = 0;
                    mappedRange.size = size;
                    vkFlushMappedMemoryRanges(mLogicalDevice, 1, &mappedRange);
                }

                vkUnmapMemory(mLogicalDevice, buffer.memory);
            }
            else {
                // 使用暂存缓冲区
                VMATraditionalBuffer stagingBuffer = createBufferTraditional(size,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

                // 上传数据到暂存缓冲区
                void* mapped;
                vkMapMemory(mLogicalDevice, stagingBuffer.memory, 0, size, 0, &mapped);
                memcpy(mapped, initialData, size);
                vkUnmapMemory(mLogicalDevice, stagingBuffer.memory);

                // 复制到设备本地缓冲区
                copyBuffer(commandPool, stagingBuffer.buffer, buffer.buffer, size);

                // 清理暂存缓冲区
                destroyBufferTraditional(stagingBuffer);
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
        }
    }

    // ==================== 图像创建和管理 ====================

    // VMA 方式创建图像
    VMAImage Device::createImageWithVMA(uint32_t width, uint32_t height, VkFormat format,
        VkImageTiling tiling, VkImageUsageFlags usage,
        VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags,
        uint32_t mipLevels, uint32_t arrayLayers) {

        if (mVmaAllocator == VK_NULL_HANDLE) {
            throw std::runtime_error("VMA not initialized!");
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

        if (vmaCreateImage(mVmaAllocator, &imageInfo, &allocInfo,
            &image, &allocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image with VMA!");
        }

        return { image, allocation };
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
            throw std::runtime_error("Failed to create image!");
        }

        // 分配内存
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(mLogicalDevice, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        VkDeviceMemory imageMemory;
        if (vkAllocateMemory(mLogicalDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            vkDestroyImage(mLogicalDevice, image, nullptr);
            throw std::runtime_error("Failed to allocate image memory!");
        }

        vkBindImageMemory(mLogicalDevice, image, imageMemory, 0);

        return { image, imageMemory };
    }

    // 销毁传统图像
    void Device::destroyImageTraditional(const VMATraditionalImage& image) {
        destroyImageTraditional(image.image, image.memory);
    }

    void Device::destroyImageTraditional(VkImage image, VkDeviceMemory memory) {
        vkDestroyImage(mLogicalDevice, image, nullptr);
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(mLogicalDevice, memory, nullptr);
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

    void Device::destroyImageView(VkImageView imageView) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(mLogicalDevice, imageView, nullptr);
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

    void Device::destroyImageWithVMAFull(const VMAImageFull& image) {
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

    void Device::destroyImageTraditionalFull(const TraditionalImageFull& image) {
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

    void Device::destroyFramebuffer(VkFramebuffer framebuffer) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(mLogicalDevice, framebuffer, nullptr);
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

        VkRenderPass renderPass;
        if (vkCreateRenderPass(mLogicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass!");
        }

        return renderPass;
    }

    void Device::destroyRenderPass(VkRenderPass renderPass) {
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(mLogicalDevice, renderPass, nullptr);
        }
    }

    // ==================== 命令系统 ====================

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

    void Device::destroyCommandPool(VkCommandPool commandPool) {
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(mLogicalDevice, commandPool, nullptr);
        }
    }

    VkCommandBuffer Device::allocateCommandBuffer(VkCommandPool pool, VkCommandBufferLevel level) {
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

    std::vector<VkCommandBuffer> Device::allocateCommandBuffers(VkCommandPool pool,
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

    void Device::freeCommandBuffers(VkCommandPool pool, const std::vector<VkCommandBuffer>& commandBuffers) {
        vkFreeCommandBuffers(mLogicalDevice, pool,
            static_cast<uint32_t>(commandBuffers.size()),
            commandBuffers.data());
    }

    // ==================== 管道创建和管理 ====================

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

    void Device::destroyPipelineLayout(VkPipelineLayout layout) {
        if (layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(mLogicalDevice, layout, nullptr);
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

    void Device::destroyPipeline(VkPipeline pipeline) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(mLogicalDevice, pipeline, nullptr);
        }
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

    void Device::destroyDescriptorPool(VkDescriptorPool pool) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(mLogicalDevice, pool, nullptr);
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

    void Device::destroyDescriptorSetLayout(VkDescriptorSetLayout layout) {
        if (layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(mLogicalDevice, layout, nullptr);
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

    void Device::destroySemaphore(VkSemaphore semaphore) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(mLogicalDevice, semaphore, nullptr);
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

    void Device::resetFence(VkFence fence) {
        if (fence != VK_NULL_HANDLE) {
            vkResetFences(mLogicalDevice, 1, &fence);
        }
    }

    void Device::blockFence(VkFence fence,uint64_t timeout) {
        if (fence != VK_NULL_HANDLE) {
            vkWaitForFences(mLogicalDevice, 1, &fence, VK_TRUE, timeout);
        }
    }

    void Device::destroyFence(VkFence fence) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(mLogicalDevice, fence, nullptr);
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

    void Device::destroySampler(VkSampler sampler) {
        if (sampler != VK_NULL_HANDLE) {
            vkDestroySampler(mLogicalDevice, sampler, nullptr);
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

    void Device::copyBuffer(VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(commandPool);

        VkBufferCopy copyRegion = {};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandPool, commandBuffer);
    }

    void Device::copyBufferToImage(VkCommandPool commandPool, VkBuffer buffer, VkImage image,
        uint32_t width, uint32_t height,
        uint32_t layerCount) {

        VkCommandBuffer commandBuffer = beginSingleTimeCommands(commandPool);

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

        vkCmdCopyBufferToImage(commandBuffer, buffer, image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(commandPool, commandBuffer);
    }

    void Device::transitionImageLayout(VkCommandPool commandPool,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkImageAspectFlags aspectMask,
        uint32_t mipLevels,
        uint32_t layerCount,
        uint32_t baseMipLevel,
        uint32_t baseArrayLayer) {

        if (oldLayout == newLayout) {
            return;  // 布局相同，不需要转换
        }

        VkCommandBuffer commandBuffer = beginSingleTimeCommands(commandPool);

        VkAccessFlags srcAccessMask = 0;
        VkAccessFlags dstAccessMask = 0;
        VkPipelineStageFlags srcStageMask = 0;
        VkPipelineStageFlags dstStageMask = 0;

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

        vkCmdPipelineBarrier(commandBuffer,
            srcStageMask, dstStageMask,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        endSingleTimeCommands(commandPool, commandBuffer);
    }

    void Device::getLayoutTransitionInfo(VkImageLayout oldLayout,
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

    void Device::generateMipmaps(VkCommandPool commandPool,VkImage image, VkFormat imageFormat,
        int32_t width, int32_t height, uint32_t mipLevels) {

        // 检查图像格式是否支持线性过滤
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, imageFormat, &formatProperties);

        if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
            throw std::runtime_error("Texture image format does not support linear filtering!");
        }

        VkCommandBuffer commandBuffer = beginSingleTimeCommands(commandPool);

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

            VkImageBlit blit = {};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(commandBuffer,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

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

        endSingleTimeCommands(commandPool, commandBuffer);
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

        float queuePriority = mConfig.queuePriority;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // 配置设备特性
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = mConfig.samplerAnisotropy;
        deviceFeatures.geometryShader = mConfig.geometryShader;
        deviceFeatures.tessellationShader = mConfig.tessellationShader;
        deviceFeatures.fillModeNonSolid = mConfig.fillModeNonSolid;
        deviceFeatures.wideLines = mConfig.wideLines;

        // 创建设备
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(mConfig.extensions.size());
        createInfo.ppEnabledExtensionNames = mConfig.extensions.data();

        // 设备级别的验证层
        if (mConfig.enableValidation) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(mInstance->getConfig().validationLayers.size());
            createInfo.ppEnabledLayerNames = mInstance->getConfig().validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mLogicalDevice) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device!");
        }

        // 获取队列句柄并添加到 QueueHandles
        if (mQueueFamilyIndices.graphicsFamily.has_value()) {
            VkQueue graphicsQueue = VK_NULL_HANDLE;
            vkGetDeviceQueue(mLogicalDevice, mQueueFamilyIndices.graphicsFamily.value(), 0, &graphicsQueue);
            mQueueHandles->addQueue(QueueHandles::QueueType::Graphics,
                graphicsQueue,
                mQueueFamilyIndices.graphicsFamily.value(),
                0,
                mConfig.queuePriority);
        }

        if (mQueueFamilyIndices.presentFamily.has_value()) {
            VkQueue presentQueue = VK_NULL_HANDLE;
            vkGetDeviceQueue(mLogicalDevice, mQueueFamilyIndices.presentFamily.value(), 0, &presentQueue);
            mQueueHandles->addQueue(QueueHandles::QueueType::Present,
                presentQueue,
                mQueueFamilyIndices.presentFamily.value(),
                0,
                mConfig.queuePriority);
        }

        // 添加计算队列（如果存在且不同于图形队列）
        if (mQueueFamilyIndices.computeFamily.has_value()) {
            VkQueue computeQueue = VK_NULL_HANDLE;
            vkGetDeviceQueue(mLogicalDevice, mQueueFamilyIndices.computeFamily.value(), 0, &computeQueue);
            mQueueHandles->addQueue(QueueHandles::QueueType::Compute,
                computeQueue,
                mQueueFamilyIndices.computeFamily.value(),
                0,
                mConfig.queuePriority);
        }

        // 添加传输队列（如果存在且不同于其他队列）
        if (mQueueFamilyIndices.transferFamily.has_value()) {
            VkQueue transferQueue = VK_NULL_HANDLE;
            vkGetDeviceQueue(mLogicalDevice, mQueueFamilyIndices.transferFamily.value(), 0, &transferQueue);
            mQueueHandles->addQueue(QueueHandles::QueueType::Transfer,
                transferQueue,
                mQueueFamilyIndices.transferFamily.value(),
                0,
                mConfig.queuePriority);
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
        std::cout << "\n=== Device Information ===" << std::endl;
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