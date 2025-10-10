#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <variant>
#include "RenderGraphTypes.hpp"

namespace StarryEngine {

    struct VirtualResource {
        ResourceHandle handle;
        std::string name;
        ResourceDescription description;

        uint32_t firstUse = UINT32_MAX;
        uint32_t lastUse = 0;
        bool isImported = false;
        bool isTransient = false;

        ResourceState initialState;
        ResourceState finalState;
        ResourceState currentState;
    };

    struct ActualResource {
    private:
        struct Image {
            VkImage image = VK_NULL_HANDLE;
            VkImageView defaultView = VK_NULL_HANDLE;
        };
        struct Buffer {
            VkBuffer buffer = VK_NULL_HANDLE;
        };

    public:
        ResourceHandle virtualHandle;
        ResourceState currentState;
        uint32_t frameIndex = 0;

        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo{};
        std::variant<Image, Buffer> actualResource;
    };

    class ResourceRegistry {
    private:
        VkDevice mDevice = VK_NULL_HANDLE;
        VmaAllocator mAllocator = VK_NULL_HANDLE;

        std::vector<VirtualResource> mVirtualResources;
        std::vector<ActualResource> mActualResources;
        std::unordered_map<ResourceHandle, uint32_t> mVirtualToActualMap;

        bool createImage(ResourceHandle handle, uint32_t frameIndex);
        bool createBuffer(ResourceHandle handle, uint32_t frameIndex);

    public:
        ResourceRegistry(VkDevice device, VmaAllocator allocator);
        ~ResourceRegistry();

        ResourceHandle createVirtualResource(const std::string& name, const ResourceDescription& desc);
        void destroyResource(ResourceHandle handle);
        bool allocateActualResources(uint32_t framesInFlight);
        void destroyActualResources();

        VirtualResource& getVirtualResource(ResourceHandle handle);
        ActualResource& getActualResource(ResourceHandle handle, uint32_t frameIndex);
        const VirtualResource& getVirtualResource(ResourceHandle handle) const;
        const ActualResource& getActualResource(ResourceHandle handle, uint32_t frameIndex) const;

        bool importResource(ResourceHandle handle, VkImage image, VkImageView imageView, ResourceState initialState);
        void updateResourceLifetime(ResourceHandle handle, uint32_t passIndex, bool isWrite);
        void computeResourceLifetimes();

        uint32_t getVirtualResourceCount() const { return static_cast<uint32_t>(mVirtualResources.size()); }
        const std::vector<VirtualResource>& getAllVirtualResources() const { return mVirtualResources; }
    };

} // namespace StarryEngine