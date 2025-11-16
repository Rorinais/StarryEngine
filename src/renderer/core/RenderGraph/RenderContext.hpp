#pragma once
#include "RenderGraphTypes.hpp"
#include "ResourceSystem.hpp"
#include <vulkan/vulkan.h>
#include <functional>
#include <unordered_map>
#include <string>
#include <memory>

namespace StarryEngine {
    // 前向声明
    class DescriptorAllocator;
    class PipelineCache;

    // 不需要前向声明 CommandBuffer，因为这里只使用 VkCommandBuffer

    class RenderContext {
    public:
        RenderContext(VkDevice device, VkCommandBuffer cmd, uint32_t frameIndex,
            ResourceRegistry* registry, DescriptorAllocator* descriptorAllocator,
            PipelineCache* pipelineCache);

        // 资源访问接口
        VkImage getImage(ResourceHandle handle) const;
        VkBuffer getBuffer(ResourceHandle handle) const;
        VkImageView getImageView(ResourceHandle handle) const;
        VkImageLayout getImageLayout(ResourceHandle handle) const;
        VkDeviceSize getBufferSize(ResourceHandle handle) const;
        void* getBufferMappedPointer(ResourceHandle handle) const;
        const ResourceDescription& getResourceDescription(ResourceHandle handle) const;
        std::string getResourceName(ResourceHandle handle) const;

        // 描述符管理
        VkDescriptorSet createDescriptorSet(const std::vector<VkDescriptorSetLayoutBinding>& bindings,
            const std::vector<VkWriteDescriptorSet>& writes);
        VkDescriptorSet getOrCreateDescriptorSet(const std::string& key,
            const std::vector<VkDescriptorSetLayoutBinding>& bindings,
            const std::vector<VkWriteDescriptorSet>& writes);

        // 渲染通道管理
        void beginRenderPass(const VkRenderPassBeginInfo* renderPassBeginInfo, VkSubpassContents subpassContents);
        void endRenderPass();

        // 管线状态管理
        void bindGraphicsPipeline(VkPipeline pipeline);
        void bindComputePipeline(const std::string& pipelineName);
        void setViewport(const VkViewport& viewport);
        void setScissor(const VkRect2D& scissor);
        void setBlendConstants(const float constants[4]);

        // 资源绑定
        void bindVertexBuffer(VkBuffer buffer, uint32_t binding = 0, VkDeviceSize offset = 0);
        void bindVertexBuffers(const std::vector<VkBuffer>& buffers);
        void bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset = 0,
            VkIndexType indexType = VK_INDEX_TYPE_UINT32);
        void bindDescriptorSet(VkPipelineBindPoint bindPoint, VkDescriptorSet descriptorSet,
            uint32_t firstSet = 0, VkPipelineLayout layout = VK_NULL_HANDLE);
        void bindDescriptorSets(VkPipelineBindPoint bindPoint, VkPipelineLayout layout,
            uint32_t firstSet, const std::vector<VkDescriptorSet>& descriptorSets);

        // 绘制和分发命令
        void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);
        void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0,
            int32_t vertexOffset = 0, uint32_t firstInstance = 0);
        void drawIndirect(ResourceHandle bufferHandle, VkDeviceSize offset, uint32_t drawCount, uint32_t stride);
        void drawIndexedIndirect(ResourceHandle bufferHandle, VkDeviceSize offset, uint32_t drawCount, uint32_t stride);
        void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
        void dispatchIndirect(ResourceHandle bufferHandle, VkDeviceSize offset);

        // 资源更新命令
        void updateBuffer(ResourceHandle bufferHandle, const void* data, size_t size, VkDeviceSize offset = 0);
        void fillBuffer(ResourceHandle bufferHandle, uint32_t data, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
        void copyBuffer(ResourceHandle srcBuffer, ResourceHandle dstBuffer, const std::vector<VkBufferCopy>& regions);
        void copyImage(ResourceHandle srcImage, ResourceHandle dstImage, const std::vector<VkImageCopy>& regions);
        void blitImage(ResourceHandle srcImage, ResourceHandle dstImage, const std::vector<VkImageBlit>& regions, VkFilter filter);

        // 动态资源管理
        ResourceHandle createTemporaryBuffer(const ResourceDescription& desc, const void* initialData = nullptr);
        ResourceHandle createTemporaryImage(const ResourceDescription& desc);
        void uploadToTemporary(ResourceHandle handle, const void* data, size_t size);

        // 获取底层对象
        VkCommandBuffer getCommandBuffer() const { return mCommandBuffer; }
        uint32_t getFrameIndex() const { return mFrameIndex; }
        VkDevice getDevice() const { return mDevice; }

        // 生命周期管理
        void beginFrame();
        void endFrame();

    private:
        VkDevice mDevice;
        VkCommandBuffer mCommandBuffer;
        uint32_t mFrameIndex;
        ResourceRegistry* mResourceRegistry;
        DescriptorAllocator* mDescriptorAllocator;
        PipelineCache* mPipelineCache;

        struct TemporaryResource {
            ResourceHandle handle;
            uint32_t frameCreated;
        };
        std::vector<TemporaryResource> mTemporaryResources;
        std::unordered_map<std::string, VkDescriptorSet> mDescriptorSetCache;

        struct BindingState {
            VkPipelineBindPoint currentBindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
            VkPipeline currentPipeline = VK_NULL_HANDLE;
            VkPipelineLayout currentPipelineLayout = VK_NULL_HANDLE;
        };
        BindingState mBindingState;

        void cleanupTemporaryResources();
        void validateResourceAccess(ResourceHandle handle, VkImageLayout expectedLayout = VK_IMAGE_LAYOUT_MAX_ENUM) const;
    };
} // namespace StarryEngine