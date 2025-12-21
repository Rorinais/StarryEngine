#pragma once
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <iomanip>
#include <cstring> 
#include <stdexcept>

#include <algorithm>
#include <functional>
#include <optional>
#include <set>
#include <vector>
#include "QueueHandles.hpp"

namespace StarryEngine {
    namespace {
        struct QueueFamilyIndices {
            std::optional<uint32_t> graphicsFamily;
            std::optional<uint32_t> presentFamily;
            std::optional<uint32_t> computeFamily;
            std::optional<uint32_t> transferFamily;

            bool isComplete() const {
                return graphicsFamily.has_value() && presentFamily.has_value();
            }

            std::set<uint32_t> getUniqueFamilies() const {
                std::set<uint32_t> uniqueFamilies;
                if (graphicsFamily.has_value()) uniqueFamilies.insert(graphicsFamily.value());
                if (presentFamily.has_value()) uniqueFamilies.insert(presentFamily.value());
                if (computeFamily.has_value()) uniqueFamilies.insert(computeFamily.value());
                if (transferFamily.has_value()) uniqueFamilies.insert(transferFamily.value());
                return uniqueFamilies;
            }
        };

        struct SwapChainSupportDetails {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };

        struct VMATraditionalBuffer {
            VkBuffer buffer;
            VkDeviceMemory memory;
        };

        struct VMABuffer {
            VkBuffer buffer;
            VmaAllocation allocation;
        };

        struct VMATraditionalImage {
            VkImage image;
            VkDeviceMemory memory;
        };

        struct VMAImage {
            VkImage image;
            VmaAllocation allocation;
        };

        struct VMAImageFull {
            VkImage image;
            VkImageView view;
            VmaAllocation allocation;
        };

        struct TraditionalImageFull {
            VkImage image;
            VkImageView view;
            VkDeviceMemory memory;
        };

        struct ImageViewHandle {
            VkImage image;
            VkImageView view;
        };
    }

    class Instance;

    class Device {
    public:
        struct Config {
            // 设备扩展配置
            std::vector<const char*> extensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                VK_KHR_MAINTENANCE1_EXTENSION_NAME
            };

            // 设备特性配置
            VkBool32 samplerAnisotropy = VK_FALSE;
            VkBool32 geometryShader = VK_FALSE;
            VkBool32 tessellationShader = VK_FALSE;
            VkBool32 fillModeNonSolid = VK_FALSE;
            VkBool32 wideLines = VK_FALSE;

            // 队列配置
            float queuePriority = 1.0f;

            // 验证层配置
            bool enableValidation = true;

            // 性能计数器配置
            bool enablePerformanceCounters = false;

            // 内存分配器配置
            bool enableVMA = true;
        };


        using Ptr = std::shared_ptr<Device>;

        static Ptr create(std::shared_ptr<Instance> instance, VkSurfaceKHR surface, const Config& config = Config()) {
            return std::make_shared<Device>(instance, surface, config);
        }

        Device(std::shared_ptr<Instance> instance, VkSurfaceKHR surface, const Config& config = Config());
        ~Device();

        // === 核心对象访问 ===
        VkPhysicalDevice getPhysicalDevice() const { return mPhysicalDevice; }
        VkPhysicalDeviceProperties getPhysicalDeviceProperties() const { return mProperties; }
        VkPhysicalDeviceFeatures getPhysicalDeviceFeatures() const;
        VkPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties() const;
        VkDevice getLogicalDevice() const { return mLogicalDevice; }

        // === 队列管理 ===
        std::shared_ptr<QueueHandles> getQueueHandles() { return mQueueHandles; }
        VkQueue getGraphicsQueue() const { return mQueueHandles->getGraphicsQueue(); }
        VkQueue getPresentQueue() const { return mQueueHandles->getPresentQueue(); }
        VkQueue getComputeQueue() const { return mQueueHandles->getComputeQueue(); }
        VkQueue getTransferQueue() const { return mQueueHandles->getTransferQueue(); }

        // === 内存分配器 ===
        bool initializeVMA();

        void cleanupVMA();

        VmaAllocator getAllocator() const { return mVmaAllocator; }

        bool isVMAEnabled() const { return mVmaAllocator != VK_NULL_HANDLE; }

        // ==================== 缓冲区创建和管理 ====================

        // VMA 方式创建缓冲区
        VMABuffer createBufferWithVMA(VkDeviceSize size, VkBufferUsageFlags usage,
            VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags = 0,
            const void* initialData = nullptr, size_t initialDataSize = 0);

        VMABuffer createBufferWithVMA(VkDeviceSize size, VkBufferUsageFlags usage,
            VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags,
            VmaAllocationInfo* allocationInfo);

        void uploadDataToVmaBuffer(VkBuffer buffer, VmaAllocation allocation,
            const void* data, size_t dataSize);

        void destroyBufferWithVMA(const VMABuffer& buffer);
        void destroyBufferWithVMA(VkBuffer buffer, VmaAllocation allocation);

        // 传统方式创建缓冲区
        VMATraditionalBuffer createBufferTraditional(VkDeviceSize size, VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties);

        VMATraditionalBuffer createBufferTraditionalWithData(VkCommandPool commandPool,
            VkDeviceSize size, VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties, const void* initialData);

        void destroyBufferTraditional(const VMATraditionalBuffer& buffer);
        void destroyBufferTraditional(VkBuffer buffer, VkDeviceMemory memory);

        // ==================== 图像创建和管理 ====================

        // VMA 方式创建图像
        VMAImage createImageWithVMA(uint32_t width, uint32_t height, VkFormat format,
            VkImageTiling tiling, VkImageUsageFlags usage,
            VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags = 0,
            uint32_t mipLevels = 1, uint32_t arrayLayers = 1);

        void destroyImageWithVMA(const VMAImage& image);
        void destroyImageWithVMA(VkImage image, VmaAllocation allocation);

        // 传统方式创建图像
        VMATraditionalImage createImageTraditional(uint32_t width, uint32_t height, VkFormat format,
            VkImageTiling tiling, VkImageUsageFlags usage,
            VkMemoryPropertyFlags properties,
            uint32_t mipLevels = 1, uint32_t arrayLayers = 1);

        void destroyImageTraditional(const VMATraditionalImage& image);
        void destroyImageTraditional(VkImage image, VkDeviceMemory memory);

        // ==================== 图像视图管理 ====================

        // 创建图像视图
        VkImageView createImageView(VkImage image, VkFormat format,
            VkImageAspectFlags aspectFlags,
            VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
            uint32_t mipLevels = 1, uint32_t baseArrayLayer = 0,
            uint32_t layerCount = 1, const char* debugName ="");

        void destroyImageView(VkImageView imageView);

        // ==================== 组合函数 ====================

        // 创建完整的图像资源（VMA方式：图像+分配+视图）
        VMAImageFull createImageWithVMAFull(uint32_t width, uint32_t height, VkFormat format,
            VkImageTiling tiling, VkImageUsageFlags usage,
            VmaMemoryUsage memoryUsage, VkImageAspectFlags aspectFlags,
            VmaAllocationCreateFlags flags = 0,
            uint32_t mipLevels = 1, uint32_t arrayLayers = 1,
            VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D);

        void destroyImageWithVMAFull(const VMAImageFull& image);

        // 创建完整的图像资源（传统方式：图像+内存+视图）
        TraditionalImageFull createImageTraditionalFull(uint32_t width, uint32_t height, VkFormat format,
            VkImageTiling tiling, VkImageUsageFlags usage,
            VkMemoryPropertyFlags properties, VkImageAspectFlags aspectFlags,
            uint32_t mipLevels = 1, uint32_t arrayLayers = 1,
            VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D);

        void destroyImageTraditionalFull(const TraditionalImageFull& image);

        // ==================== 帧缓冲创建和管理 ====================
        VkFramebuffer createFramebuffer(VkRenderPass renderPass,
            const std::vector<VkImageView>& attachments,
            uint32_t width, uint32_t height, uint32_t layers = 1);
        void destroyFramebuffer(VkFramebuffer framebuffer);

        // ==================== 渲染通道创建和管理 ====================
        VkRenderPass createRenderPass(const std::vector<VkAttachmentDescription>& attachments,
            const std::vector<VkSubpassDescription>& subpasses,
            const std::vector<VkSubpassDependency>& dependencies);
        void destroyRenderPass(VkRenderPass renderPass);

        // ==================== 命令系统 ====================
        VkCommandPool createCommandPool(uint32_t queueFamilyIndex,
            VkCommandPoolCreateFlags flags = 0);
        void destroyCommandPool(VkCommandPool commandPool);

        VkCommandBuffer allocateCommandBuffer(VkCommandPool pool,
            VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        std::vector<VkCommandBuffer> allocateCommandBuffers(VkCommandPool pool,
            uint32_t count, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        void freeCommandBuffers(VkCommandPool pool, const std::vector<VkCommandBuffer>& commandBuffers);

        // ==================== 管道创建和管理 ====================
        VkPipelineLayout createPipelineLayout(const std::vector<VkDescriptorSetLayout>& setLayouts = {},
            const std::vector<VkPushConstantRange>& pushConstants = {});
        void destroyPipelineLayout(VkPipelineLayout layout);

        VkPipeline createGraphicsPipeline(const VkGraphicsPipelineCreateInfo& createInfo);
        VkPipeline createComputePipeline(const VkComputePipelineCreateInfo& createInfo);
        void destroyPipeline(VkPipeline pipeline);

        // ==================== 描述符系统 ====================
        VkDescriptorPool createDescriptorPool(const std::vector<VkDescriptorPoolSize>& poolSizes,
            uint32_t maxSets);
        void destroyDescriptorPool(VkDescriptorPool pool);

        VkDescriptorSetLayout createDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);
        void destroyDescriptorSetLayout(VkDescriptorSetLayout layout);

        VkDescriptorSet allocateDescriptorSet(VkDescriptorPool pool, VkDescriptorSetLayout layout);
        void updateDescriptorSet(VkDescriptorSet set,
            const std::vector<VkWriteDescriptorSet>& writes);

        // ==================== 同步对象 ====================
        VkSemaphore createSemaphore();
        void destroySemaphore(VkSemaphore semaphore);

        VkFence createFence(VkFenceCreateFlags flags = 0);
        void resetFence(VkFence fence);
        void blockFence(VkFence fence, uint64_t timeout = UINT64_MAX);
        void destroyFence(VkFence fence);

        // ==================== 采样器 ====================
        VkSampler createSampler(VkFilter magFilter = VK_FILTER_LINEAR,
            VkFilter minFilter = VK_FILTER_LINEAR,
            VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            bool anisotropyEnable = false, float maxAnisotropy = 1.0f,
            VkBool32 compareEnable = VK_FALSE, VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS,
            float mipLodBias = 0.0f, float minLod = 0.0f, float maxLod = 0.0f,
            VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK);
        void destroySampler(VkSampler sampler);

        // ==================== 实用功能 ====================
        VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool);
        void endSingleTimeCommands(VkCommandPool commandPool, VkCommandBuffer commandBuffer);

        void copyBuffer(VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        void copyBufferToImage(VkCommandPool commandPool, VkBuffer buffer, VkImage image,
            uint32_t width, uint32_t height, uint32_t layerCount = 1);

        void transitionImageLayout(VkCommandPool commandPool,
            VkImage image,
            VkImageLayout oldLayout,
            VkImageLayout newLayout,
            VkImageAspectFlags aspectMask,
            uint32_t mipLevels = 1,
            uint32_t layerCount = 1,
            uint32_t baseMipLevel = 0,
            uint32_t baseArrayLayer = 0);

        // 获取布局转换的访问掩码和流水线阶段
        static void getLayoutTransitionInfo(VkImageLayout oldLayout,
            VkImageLayout newLayout,
            VkAccessFlags& srcAccessMask,
            VkAccessFlags& dstAccessMask,
            VkPipelineStageFlags& srcStageMask,
            VkPipelineStageFlags& dstStageMask);

        void generateMipmaps(VkCommandPool commandPool, VkImage image, VkFormat imageFormat,
            int32_t width, int32_t height, uint32_t mipLevels);

        // ==================== 查询功能 ====================
        std::shared_ptr<Instance> getInstance() const { return mInstance; }
        QueueFamilyIndices getQueueFamilyIndices() const { return mQueueFamilyIndices; }
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
            VkImageTiling tiling, VkFormatFeatureFlags features) const;
        VkFormat findDepthFormat() const;

        static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
        SwapChainSupportDetails querySwapChainSupport() const;

        bool isExtensionSupported(const char* extensionName) const;

        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

        // ==================== 设备功能检查 ====================
        bool supportsGeometryShader() const { return mPhysicalDeviceFeatures.geometryShader; }
        bool supportsTessellationShader() const { return mPhysicalDeviceFeatures.tessellationShader; }
        bool supportsAnisotropy() const { return mPhysicalDeviceFeatures.samplerAnisotropy; }
        bool supportsWideLines() const { return mPhysicalDeviceFeatures.wideLines; }
        bool supportsFillModeNonSolid() const { return mPhysicalDeviceFeatures.fillModeNonSolid; }

        // ==================== 设备信息 ====================
        const char* getDeviceName() const { return mProperties.deviceName; }
        VkPhysicalDeviceType getDeviceType() const { return mProperties.deviceType; }
        uint32_t getApiVersion() const { return mProperties.apiVersion; }
        uint32_t getDriverVersion() const { return mProperties.driverVersion; }
        uint32_t getVendorID() const { return mProperties.vendorID; }
        uint32_t getDeviceID() const { return mProperties.deviceID; }

        uint32_t getMaxUniformBufferRange() const { return mProperties.limits.maxUniformBufferRange; }
        uint32_t getMaxImageDimension2D() const { return mProperties.limits.maxImageDimension2D; }
        uint32_t getMaxBoundDescriptorSets() const { return mProperties.limits.maxBoundDescriptorSets; }

        // ==================== 性能计数器 ====================
        bool isPerformanceCounterSupported() const { return mPerformanceCounterSupported; }
        std::vector<uint64_t> getPerformanceCounterValues(QueueHandles::QueueType queueType,
            const std::vector<uint32_t>& counterIndices);

        // ==================== 调试和统计 ====================
        void printDeviceInfo() const;
        void setObjectName(uint64_t object, VkObjectType objectType, const char* name);
        void setBufferName(VkBuffer buffer, const char* name);
        void setImageName(VkImage image, const char* name);

        // ==================== 等待和同步 ====================
        void waitIdle() const;

    private:
        // 物理设备选择
        VkPhysicalDevice selectPhysicalDevice(VkSurfaceKHR surface);
        bool isPhysicalDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
        int32_t ratePhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR surface);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;

        // 逻辑设备创建
        void createLogicalDevice();

        // 性能计数器初始化
        void initializePerformanceCounters();
        void cleanupPerformanceCounters();

    private:
        // 外部引用
        std::shared_ptr<Instance> mInstance;
        VkSurfaceKHR mSurface;
        Config mConfig;

        // 物理设备
        VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties mProperties{};
        VkPhysicalDeviceFeatures mPhysicalDeviceFeatures{};
        VkPhysicalDeviceMemoryProperties mMemoryProperties{};
        QueueFamilyIndices mQueueFamilyIndices;

        // 逻辑设备
        VkDevice mLogicalDevice = VK_NULL_HANDLE;
        std::shared_ptr<QueueHandles> mQueueHandles;

        // 内存分配器
        VmaAllocator mVmaAllocator = VK_NULL_HANDLE;

        // 性能计数器支持
        bool mPerformanceCounterSupported = false;

        // 调试功能
        PFN_vkSetDebugUtilsObjectNameEXT mSetDebugUtilsObjectNameEXT = nullptr;
    };
}