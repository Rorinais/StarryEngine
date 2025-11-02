#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <cstdint>
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>

namespace StarryEngine {

    // 泛型句柄
    template<typename TypeName>
    class Handle {
    private:
        uint32_t mId;
    public:
        Handle(uint32_t id = UINT32_MAX) : mId(id) {}
        ~Handle() = default;

        bool operator==(const Handle& other) const { return mId == other.mId; }
        bool operator!=(const Handle& other) const { return mId != other.mId; }

        uint32_t getId() const { return mId; }
        void setId(uint32_t id) { mId = id; }

        bool isValid() const { return mId != UINT32_MAX; }

        struct Hash {
            size_t operator()(const Handle& handle) const {
                return std::hash<uint32_t>()(handle.getId());
            }
        };
    };

    // 渲染通道句柄和资源句柄
    struct RenderPassTag {};
    struct ResourceTag {};

    using RenderPassHandle = Handle<RenderPassTag>;
    using ResourceHandle = Handle<ResourceTag>;

    // 资源描述符
    struct ResourceDescription {
        // 图像相关字段
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent3D extent = { 0, 0, 0 };
        uint32_t arrayLayers = 1;
        uint32_t mipLevels = 1;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkImageUsageFlags imageUsage = 0;

        // 兼容字段
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 1;
        VkImageUsageFlags usage = 0;  // 兼容 usage
        VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;  // 添加 VMA 支持

        // 缓冲区相关字段
        VkBufferUsageFlags bufferUsage = 0;
        VkMemoryPropertyFlags memoryProperties = 0;
        size_t size = 0;

        bool isTransient = false;
        bool isAttachment = false;

        // 同步字段
        void syncFields() {
            // 同步 extent 和 width/height/depth
            if (width > 0 || height > 0 || depth > 0) {
                extent = { width, height, depth };
            }
            else if (extent.width > 0 || extent.height > 0 || extent.depth > 0) {
                width = extent.width;
                height = extent.height;
                depth = extent.depth;
            }

            // 同步 usage
            if (usage != 0 && imageUsage == 0) {
                imageUsage = usage;
            }
            else if (imageUsage != 0 && usage == 0) {
                usage = imageUsage;
            }
        }

        bool isImage() const {
            return imageUsage != 0 || usage != 0 || format != VK_FORMAT_UNDEFINED;
        }

        bool isBuffer() const {
            return bufferUsage != 0 && size > 0;
        }

        bool isValid() const {
            return (isImage() && !isBuffer()) || (!isImage() && isBuffer());
        }
    };

    inline ResourceDescription createTextureDescription(
        VkFormat format,
        VkExtent3D extent,
        VkImageUsageFlags usage,
        uint32_t mipLevels = 1,
        uint32_t arrayLayers = 1) {

        ResourceDescription desc;
        desc.format = format;
        desc.extent = extent;
        desc.width = extent.width;
        desc.height = extent.height;
        desc.depth = extent.depth;
        desc.imageUsage = usage;
        desc.usage = usage;  // 设置兼容字段
        desc.mipLevels = mipLevels;
        desc.arrayLayers = arrayLayers;
        desc.samples = VK_SAMPLE_COUNT_1_BIT;
        return desc;
    }

    // 添加重载版本以兼容旧代码
    inline ResourceDescription createTextureDescription(
        VkFormat format,
        uint32_t width, uint32_t height,
        VkImageUsageFlags usage,
        uint32_t mipLevels = 1,
        uint32_t arrayLayers = 1) {

        ResourceDescription desc;
        desc.format = format;
        desc.extent = { width, height, 1 };
        desc.width = width;
        desc.height = height;
        desc.depth = 1;
        desc.imageUsage = usage;
        desc.usage = usage;
        desc.mipLevels = mipLevels;
        desc.arrayLayers = arrayLayers;
        desc.samples = VK_SAMPLE_COUNT_1_BIT;
        return desc;
    }

    inline ResourceDescription createBufferDescription(
        size_t size,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY) {

        ResourceDescription desc;
        desc.size = size;
        desc.bufferUsage = usage;
        desc.memoryUsage = memoryUsage;
        return desc;
    }

    inline ResourceDescription createAttachmentDescription(
        VkFormat format,
        VkExtent3D extent,
        VkImageUsageFlags usage) {

        auto desc = createTextureDescription(format, extent, usage);
        desc.isAttachment = true;
        return desc;
    }

    // 添加重载版本
    inline ResourceDescription createAttachmentDescription(
        VkFormat format,
        uint32_t width, uint32_t height,
        VkImageUsageFlags usage) {

        auto desc = createTextureDescription(format, width, height, usage);
        desc.isAttachment = true;
        return desc;
    }

    struct ResourceState {
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkAccessFlags accessMask = 0;
        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        bool isWrite() const {
            return accessMask & (VK_ACCESS_SHADER_WRITE_BIT |
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                VK_ACCESS_TRANSFER_WRITE_BIT);
        }
    };


    // 资源使用类型
    struct ResourceUsage {
        ResourceHandle resource;
        VkPipelineStageFlags stageFlags;
        VkAccessFlags accessFlags;
        VkImageLayout layout;
        bool isWrite;

        uint32_t binding = 0;
        uint32_t descriptorSet = 0;
        VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    };

    struct BarrierBatch {
        std::vector<VkMemoryBarrier> memoryBarriers;
        std::vector<VkBufferMemoryBarrier> bufferBarriers;
        std::vector<VkImageMemoryBarrier> imageBarriers;

        bool empty() const {
            return memoryBarriers.empty() &&
                bufferBarriers.empty() &&
                imageBarriers.empty();
        }
    };

    struct Dependency {
        RenderPassHandle producer;
        RenderPassHandle consumer;
        ResourceHandle resource;
        ResourceState stateBefore;
        ResourceState stateAfter;
    };

    struct ResourceAliasGroup {
        std::vector<ResourceHandle> resources;
        size_t requiredSize = 0;
        VkMemoryRequirements memoryRequirements;
        bool canAlias = false;
    };

    // 哈希函数
    inline size_t hash(const ResourceDescription& desc) {
        size_t seed = 0;
        auto combine = [&seed](size_t value) {
            seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            };

        combine(static_cast<size_t>(desc.format));
        combine(static_cast<size_t>(desc.extent.width));
        combine(static_cast<size_t>(desc.extent.height));
        combine(static_cast<size_t>(desc.extent.depth));
        combine(static_cast<size_t>(desc.arrayLayers));
        combine(static_cast<size_t>(desc.mipLevels));
        combine(static_cast<size_t>(desc.samples));
        combine(static_cast<size_t>(desc.imageUsage));
        combine(static_cast<size_t>(desc.usage));  
        combine(static_cast<size_t>(desc.bufferUsage));
        combine(static_cast<size_t>(desc.memoryProperties));
        combine(static_cast<size_t>(desc.memoryUsage)); 
        combine(static_cast<size_t>(desc.size));
        combine(static_cast<size_t>(desc.isTransient));
        combine(static_cast<size_t>(desc.isAttachment));

        return seed;
    }

} // namespace StarryEngine

namespace std {
    template<>
    struct hash<StarryEngine::ResourceDescription> {
        size_t operator()(const StarryEngine::ResourceDescription& desc) const {
            return StarryEngine::hash(desc);
        }
    };

    template<typename Tag>
    struct hash<StarryEngine::Handle<Tag>> {
        size_t operator()(const StarryEngine::Handle<Tag>& handle) const {
            return std::hash<uint32_t>()(handle.getId());
        }
    };
}