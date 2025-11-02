#include <cassert>
#include "ResourceSystem.hpp"

namespace StarryEngine {

    ResourceRegistry::ResourceRegistry(VkDevice device, VmaAllocator allocator)
        : mDevice(device), mAllocator(allocator) {
    }

    ResourceRegistry::~ResourceRegistry() {
        destroyActualResources();
    }

    ResourceHandle ResourceRegistry::createVirtualResource(const std::string& name, const ResourceDescription& desc) {
        // 验证资源描述的有效性
        if (!desc.isValid()) {
            std::cerr << "Error: Invalid resource description - cannot create both image and buffer" << std::endl;
            return ResourceHandle{};
        }

        VirtualResource resource;
        resource.name = name;
        resource.handle.setId(static_cast<uint32_t>(mVirtualResources.size()));
        resource.description = desc;
        resource.isTransient = desc.isTransient;

        // 同步字段
        resource.description.syncFields();

        // 根据资源类型设置初始状态
        if (desc.isImage()) {
            resource.initialState.layout = VK_IMAGE_LAYOUT_UNDEFINED;
            resource.currentState.layout = VK_IMAGE_LAYOUT_UNDEFINED;

            // 如果是附件，可能需要不同的初始状态
            if (desc.isAttachment) {
                resource.initialState.layout = VK_IMAGE_LAYOUT_UNDEFINED;
                resource.initialState.accessMask = 0;
                resource.initialState.stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            }
        }
        else {
            // 缓冲区不需要布局
            resource.initialState.layout = VK_IMAGE_LAYOUT_UNDEFINED;
            resource.currentState.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        }

        mVirtualResources.push_back(std::move(resource));
        return mVirtualResources.back().handle;
    }

    void ResourceRegistry::updateResourceLifetime(ResourceHandle handle, uint32_t passIndex, bool isWrite) {
        auto& resource = getVirtualResource(handle);
        if (passIndex < resource.firstUse) {
            resource.firstUse = passIndex;
        }

        if (passIndex > resource.lastUse) {
            resource.lastUse = passIndex;
        }

        if (isWrite) {
            resource.finalState.accessMask |= VK_ACCESS_SHADER_WRITE_BIT;
            resource.finalState.stageMask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        }
    }

    bool ResourceRegistry::allocateActualResources(uint32_t framesInFlight) {
        destroyActualResources();

        for (uint32_t i = 0; i < mVirtualResources.size(); ++i) {
            ResourceHandle handle{ i };
            auto& virtualResource = mVirtualResources[i];

            if (virtualResource.isTransient) {
                continue;
            }

            for (uint32_t frameIndex = 0; frameIndex < framesInFlight; ++frameIndex) {
                bool success = false;

                if (virtualResource.isImage()) {
                    success = createImage(handle, frameIndex);
                }
                else if (virtualResource.isBuffer()) {
                    success = createBuffer(handle, frameIndex);
                }

                if (!success) {
                    destroyActualResources();
                    return false;
                }
            }
        }
        return true;
    }

    void ResourceRegistry::destroyActualResources() {
        for (auto& resource : mActualResources) {
            if (resource.allocation != VK_NULL_HANDLE) {
                std::visit([&](auto&& actual_res) {
                    using T = std::decay_t<decltype(actual_res)>;

                    if constexpr (std::is_same_v<T, ActualResource::Image>) {
                        if (actual_res.defaultView != VK_NULL_HANDLE) {
                            vkDestroyImageView(mDevice, actual_res.defaultView, nullptr);
                        }
                        if (actual_res.image != VK_NULL_HANDLE) {
                            vmaDestroyImage(mAllocator, actual_res.image, resource.allocation);
                        }
                    }
                    else if constexpr (std::is_same_v<T, ActualResource::Buffer>) {
                        if (actual_res.buffer != VK_NULL_HANDLE) {
                            vmaDestroyBuffer(mAllocator, actual_res.buffer, resource.allocation);
                        }
                    }
                    }, resource.actualResource);
            }
        }

        mActualResources.clear();
        mVirtualToActualMap.clear();
    }

    void ResourceRegistry::destroyResource(ResourceHandle handle) {
        assert(handle.getId() < mVirtualResources.size() && "Invalid resource handle");
        // 标记虚拟资源为无效
        auto& virtResource = mVirtualResources[handle.getId()];
        virtResource.handle = ResourceHandle{}; // 设为无效句柄

        // 销毁对应的实际资源
        auto range = mVirtualToActualMap.equal_range(handle);
        for (auto it = range.first; it != range.second; ++it) {
            uint32_t actualIndex = it->second;
            if (actualIndex < mActualResources.size()) {
                auto& actualResource = mActualResources[actualIndex];
                // 销毁实际资源
                std::visit([&](auto&& actual_res) {
                    using T = std::decay_t<decltype(actual_res)>;
                    if constexpr (std::is_same_v<T, ActualResource::Image>) {
                        if (actual_res.defaultView != VK_NULL_HANDLE) {
                            vkDestroyImageView(mDevice, actual_res.defaultView, nullptr);
                        }
                        if (actual_res.image != VK_NULL_HANDLE) {
                            vmaDestroyImage(mAllocator, actual_res.image, actualResource.allocation);
                        }
                    }
                    else if constexpr (std::is_same_v<T, ActualResource::Buffer>) {
                        if (actual_res.buffer != VK_NULL_HANDLE) {
                            vmaDestroyBuffer(mAllocator, actual_res.buffer, actualResource.allocation);
                        }
                    }
                    }, actualResource.actualResource);
            }
        }

        // 从映射中移除
        mVirtualToActualMap.erase(handle);
    }

    VirtualResource& ResourceRegistry::getVirtualResource(ResourceHandle handle) {
        assert(handle.getId() < mVirtualResources.size() && "Invalid resource handle");
        return mVirtualResources[handle.getId()];
    }

    ActualResource& ResourceRegistry::getActualResource(ResourceHandle handle, uint32_t frameIndex) {
        // 由于一个虚拟资源可能对应多个实际资源（每帧一个），我们需要根据帧索引查找
        auto range = mVirtualToActualMap.equal_range(handle);
        for (auto it = range.first; it != range.second; ++it) {
            if (mActualResources[it->second].frameIndex == frameIndex) {
                return mActualResources[it->second];
            }
        }
        throw std::runtime_error("Actual resource not found for handle and frame index");
    }

    const VirtualResource& ResourceRegistry::getVirtualResource(ResourceHandle handle) const {
        assert(handle.getId() < mVirtualResources.size() && "Invalid resource handle");
        return mVirtualResources[handle.getId()];
    }

    const ActualResource& ResourceRegistry::getActualResource(ResourceHandle handle, uint32_t frameIndex) const {
        auto range = mVirtualToActualMap.equal_range(handle);
        for (auto it = range.first; it != range.second; ++it) {
            if (mActualResources[it->second].frameIndex == frameIndex) {
                return mActualResources[it->second];
            }
        }
        throw std::runtime_error("Actual resource not found for handle and frame index");
    }

    bool ResourceRegistry::importResource(ResourceHandle handle, VkImage image, VkImageView imageView, ResourceState initialState) {
        auto& virtResource = getVirtualResource(handle);
        virtResource.isImported = true;
        virtResource.initialState = initialState;
        virtResource.currentState = initialState;

        ActualResource actualResource;
        actualResource.virtualHandle = handle;
        actualResource.frameIndex = 0; // 导入的资源通常只用于第0帧
        actualResource.allocation = VK_NULL_HANDLE;
        actualResource.currentState = initialState;

        ActualResource::Image importedImage;
        importedImage.image = image;
        importedImage.defaultView = imageView;
        actualResource.actualResource = std::move(importedImage);

        uint32_t index = static_cast<uint32_t>(mActualResources.size());
        mActualResources.push_back(std::move(actualResource));
        mVirtualToActualMap.emplace(handle, index);

        return true;
    }

    void ResourceRegistry::computeResourceLifetimes() {
        // 重置所有资源的生命周期
        for (auto& resource : mVirtualResources) {
            resource.firstUse = UINT32_MAX;
            resource.lastUse = 0;
        }
        // 这里需要根据渲染通道的使用情况来计算生命周期
        // 具体实现取决于你的渲染图系统
    }

    bool ResourceRegistry::createImage(ResourceHandle handle, uint32_t frameIndex) {
        auto& virtResource = getVirtualResource(handle);
        const auto& desc = virtResource.description;

        VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = desc.format;
        imageInfo.extent = desc.extent;
        imageInfo.mipLevels = desc.mipLevels;
        imageInfo.arrayLayers = desc.arrayLayers;
        imageInfo.samples = desc.samples;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = desc.imageUsage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = desc.memoryUsage;
        allocInfo.flags = virtResource.isTransient ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT : 0;

        ActualResource actualResource;
        actualResource.virtualHandle = handle;
        actualResource.frameIndex = frameIndex;

        ActualResource::Image newImage;

        VkResult result = vmaCreateImage(mAllocator, &imageInfo, &allocInfo,
            &newImage.image,
            &actualResource.allocation,
            &actualResource.allocationInfo);

        if (result != VK_SUCCESS) {
            return false;
        }

        // 确定图像方面
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        if (desc.format == VK_FORMAT_D32_SFLOAT || desc.format == VK_FORMAT_D16_UNORM) {
            aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        else if (desc.format == VK_FORMAT_D24_UNORM_S8_UINT || desc.format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
            aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        viewInfo.image = newImage.image;
        viewInfo.viewType = desc.arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = desc.format;
        viewInfo.subresourceRange.aspectMask = aspectMask;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = desc.mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = desc.arrayLayers;

        result = vkCreateImageView(mDevice, &viewInfo, nullptr, &newImage.defaultView);
        if (result != VK_SUCCESS) {
            vmaDestroyImage(mAllocator, newImage.image, actualResource.allocation);
            return false;
        }

        actualResource.actualResource = std::move(newImage);
        actualResource.currentState = virtResource.initialState;

        uint32_t index = static_cast<uint32_t>(mActualResources.size());
        mActualResources.push_back(std::move(actualResource));
        mVirtualToActualMap.emplace(handle, index);

        return true;
    }

    bool ResourceRegistry::createBuffer(ResourceHandle handle, uint32_t frameIndex) {
        auto& virtResource = getVirtualResource(handle);
        const auto& desc = virtResource.description;

        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = desc.size;
        bufferInfo.usage = desc.bufferUsage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = desc.memoryUsage;

        ActualResource actualResource;
        actualResource.virtualHandle = handle;
        actualResource.frameIndex = frameIndex;

        ActualResource::Buffer newBuffer;

        VkResult result = vmaCreateBuffer(mAllocator, &bufferInfo, &allocInfo,
            &newBuffer.buffer,
            &actualResource.allocation,
            &actualResource.allocationInfo);

        if (result != VK_SUCCESS) {
            return false;
        }

        actualResource.actualResource = std::move(newBuffer);
        actualResource.currentState = virtResource.initialState;

        uint32_t index = static_cast<uint32_t>(mActualResources.size());
        mActualResources.push_back(std::move(actualResource));
        mVirtualToActualMap.emplace(handle, index);

        return true;
    }

} // namespace StarryEngine