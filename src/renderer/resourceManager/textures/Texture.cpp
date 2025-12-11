#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include"Texture.hpp"
namespace StarryEngine {
    void Texture::loadTexture(const char* imagePath) {
        stbi_uc* pixelData = stbi_load(imagePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (!pixelData) {
            throw std::runtime_error("Failed to load texture image!");
        }

        size_t imageSize = texWidth * texHeight * 4;
        pixels.resize(imageSize);
        memcpy(pixels.data(), pixelData, imageSize);
        stbi_image_free(pixelData);
    }

    Texture::Texture(
        const LogicalDevice::Ptr& logicalDevice,
        const char* imagePath,
        CommandPool::Ptr commandPool
    ) : mLogicalDevice(logicalDevice), mCommandPool(commandPool), mType(Type::Color) {
        loadTexture(imagePath);
        VkExtent2D extent = { static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight) };
        mFormat = VK_FORMAT_R8G8B8A8_SRGB;

        createImage(
            mFormat,
            extent,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_TILING_OPTIMAL);

        allocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        createImageView();

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        createSampler(samplerInfo);

        if (pixels.data() && pixels.size() > 0) {
            uploadData(pixels.data(), pixels.size(), extent);
        }
    }

    // 深度纹理专用构造函数
    Texture::Texture(
        const LogicalDevice::Ptr& logicalDevice,
        Type type,
        VkExtent2D extent,
        CommandPool::Ptr commandPool)
        : mLogicalDevice(logicalDevice), mType(type), mCommandPool(commandPool) {

        if (type != Type::Depth)
            throw std::runtime_error("Invalid constructor for non-depth texture");

        mFormat = findSupportedDepthFormat(mLogicalDevice->getPhysicalDevice()->getHandle());

        createImage(
            mFormat,
            extent,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_TILING_OPTIMAL);

        allocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        createImageView();

        // 仅在命令池可用时执行布局转换
        if (mCommandPool) {
            transitionImageLayout(mImage,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }
    }

    Texture::~Texture() {
        cleanup();
    }

    void Texture::cleanup() {
        if (mImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(mLogicalDevice->getHandle(), mImageView, nullptr);
            mImageView = VK_NULL_HANDLE;
        }
        if (mSampler != VK_NULL_HANDLE) {
            vkDestroySampler(mLogicalDevice->getHandle(), mSampler, nullptr);
            mSampler = VK_NULL_HANDLE;
        }
        if (mImage != VK_NULL_HANDLE) {
            vkDestroyImage(mLogicalDevice->getHandle(), mImage, nullptr);
            mImage = VK_NULL_HANDLE;
        }
        if (mMemory != VK_NULL_HANDLE) {
            vkFreeMemory(mLogicalDevice->getHandle(), mMemory, nullptr);
            mMemory = VK_NULL_HANDLE;
        }
    }

    void Texture::createImage(VkFormat format, VkExtent2D extent, VkImageUsageFlags usage, VkImageTiling tiling)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = { extent.width, extent.height, 1 };
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(mLogicalDevice->getHandle(), &imageInfo, nullptr, &mImage) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image!");
        }
    }

    void Texture::recreate(VkExtent2D newExtent) {
        if (mType != Type::Depth) {
            return;
        }

        cleanup(); // 清理旧资源

        // 重新创建深度纹理
        mFormat = findSupportedDepthFormat(mLogicalDevice->getPhysicalDevice()->getHandle());
        createImage(
            mFormat,
            newExtent,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_TILING_OPTIMAL);

        allocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        createImageView();

        // 仅在命令池可用时执行布局转换
        if (mCommandPool) {
            transitionImageLayout(mImage,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }
    }

    void Texture::allocateMemory(VkMemoryPropertyFlags properties) {
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(mLogicalDevice->getHandle(), mImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(mLogicalDevice->getHandle(), &allocInfo, nullptr, &mMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate image memory!");
        }

        vkBindImageMemory(mLogicalDevice->getHandle(), mImage, mMemory, 0);
    }

    void Texture::createImageView()
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = mImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = mFormat;

        // 自动设置aspect mask
        if (mType == Type::Depth) {
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (hasStencilComponent(mFormat)) {
                viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        else {
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(mLogicalDevice->getHandle(), &viewInfo, nullptr, &mImageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture image view!");
        }
    }

    void Texture::createSampler(const VkSamplerCreateInfo& samplerInfo) {
        if (vkCreateSampler(mLogicalDevice->getHandle(), &samplerInfo, nullptr, &mSampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture sampler!");
        }
    }

    void Texture::uploadData(const void* data, size_t dataSize, VkExtent2D extent) {
        // 创建暂存缓冲区
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = dataSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(mLogicalDevice->getHandle(), &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create staging buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(mLogicalDevice->getHandle(), stagingBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(mLogicalDevice->getHandle(), &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
            vkDestroyBuffer(mLogicalDevice->getHandle(), stagingBuffer, nullptr);
            throw std::runtime_error("Failed to allocate staging memory!");
        }

        vkBindBufferMemory(mLogicalDevice->getHandle(), stagingBuffer, stagingMemory, 0);

        // 拷贝数据到暂存缓冲区
        void* mapped;
        vkMapMemory(mLogicalDevice->getHandle(), stagingMemory, 0, dataSize, 0, &mapped);
        memcpy(mapped, data, dataSize);
        vkUnmapMemory(mLogicalDevice->getHandle(), stagingMemory);

        // 转换图像布局并拷贝数据
        transitionImageLayout(mImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        copyBufferToImage(stagingBuffer, mImage, extent);

        transitionImageLayout(mImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // 清理暂存资源
        vkDestroyBuffer(mLogicalDevice->getHandle(), stagingBuffer, nullptr);
        vkFreeMemory(mLogicalDevice->getHandle(), stagingMemory, nullptr);
    }

    // 修改后的布局转换函数
    void Texture::transitionImageLayout(VkImage image, VkImageLayout oldLayout,
        VkImageLayout newLayout)
    {
        VkCommandBuffer cmdBuf = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;

        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (hasStencilComponent(mFormat)) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage;
        VkPipelineStageFlags dstStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
            if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                // 普通纹理初始化情况
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            else if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
                // 深度纹理初始化情况
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            }
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
            newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            // 普通纹理最终布局转换
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else {
            throw std::invalid_argument("Unsupported layout transition!");
        }

        vkCmdPipelineBarrier(cmdBuf, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        endSingleTimeCommands(cmdBuf);
    }

    void Texture::copyBufferToImage(VkBuffer buffer, VkImage image,
        VkExtent2D extent) {
        VkCommandBuffer cmdBuf = beginSingleTimeCommands();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { extent.width, extent.height, 1 };

        vkCmdCopyBufferToImage(cmdBuf, buffer, image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region);

        endSingleTimeCommands(cmdBuf);
    }

    VkCommandBuffer Texture::beginSingleTimeCommands() {
        if (!mCommandPool) {
            throw std::runtime_error("Command pool not available for texture operations");
        }

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = mCommandPool->getHandle();
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmdBuf;
        vkAllocateCommandBuffers(mLogicalDevice->getHandle(), &allocInfo, &cmdBuf);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmdBuf, &beginInfo);

        return cmdBuf;
    }

    void Texture::endSingleTimeCommands(VkCommandBuffer cmdBuf) {
        vkEndCommandBuffer(cmdBuf);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuf;

        vkQueueSubmit(mLogicalDevice->getQueueHandles().graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(mLogicalDevice->getQueueHandles().graphicsQueue);

        vkFreeCommandBuffers(mLogicalDevice->getHandle(), mCommandPool->getHandle(), 1, &cmdBuf);
    }

    uint32_t Texture::findMemoryType(uint32_t typeFilter,
        VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(mLogicalDevice->getPhysicalDevice()->getHandle(), &memProps);

        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) &&
                (memProps.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }
        throw std::runtime_error("Failed to find suitable memory type");
    }

    VkFormat Texture::findSupportedDepthFormat(VkPhysicalDevice physicalDevice) {
        const std::vector<VkFormat> candidates = {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D24_UNORM_S8_UINT
        };

        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                return format;
            }
        }
        throw std::runtime_error("Failed to find supported depth format!");
    }

    bool Texture::hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            format == VK_FORMAT_D24_UNORM_S8_UINT;
    }
}