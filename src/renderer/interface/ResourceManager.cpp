#pragma once
#include "ResourceManager.hpp"
#include "../backend/Device.hpp"

namespace StarryEngine::RHI {

    // ==================== Vulkan缓冲区数据 ====================

    class VulkanBufferData : public ResourceData {
    public:
        VulkanBufferData(Device::Ptr device, VkBuffer buffer, VmaAllocation allocation,
            const BufferDesc& desc)
            : device_(device), buffer_(buffer), allocation_(allocation), desc_(desc) {}

        ~VulkanBufferData() override {
            release();
        }

        void release() override {
            if (device_ && device_->getAllocator() && buffer_ != VK_NULL_HANDLE) {
                device_->destroyBufferWithVMA(buffer_, allocation_);
                buffer_ = VK_NULL_HANDLE;
                allocation_ = VK_NULL_HANDLE;
            }
        }

        bool isValid() const override {
            return buffer_ != VK_NULL_HANDLE && device_ != nullptr;
        }

        void* getNativeHandle() const override {
            return reinterpret_cast<void*>(buffer_);
        }

        size_t getMemoryUsage() const override {
            return desc_.size;
        }

        VkBuffer getVkBuffer() const { return buffer_; }
        const BufferDesc& getDesc() const { return desc_; }

    private:
        Device::Ptr device_;
        VkBuffer buffer_ = VK_NULL_HANDLE;
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        BufferDesc desc_;
    };

    // ==================== Vulkan管线布局数据 ====================

    class VulkanPipelineLayoutData : public ResourceData {
    public:
        VulkanPipelineLayoutData(Device::Ptr device, VkPipelineLayout layout,
            std::vector<VkDescriptorSetLayout> setLayouts,
            const PipelineLayoutDesc& desc)
            : device_(device), layout_(layout), setLayouts_(std::move(setLayouts)), desc_(desc) {}

        ~VulkanPipelineLayoutData() override {
            release();
        }

        void release() override {
            if (device_ && device_->getLogicalDevice()) {
                if (layout_ != VK_NULL_HANDLE) {
                    vkDestroyPipelineLayout(device_->getLogicalDevice(), layout_, nullptr);
                    layout_ = VK_NULL_HANDLE;
                }

                for (auto setLayout : setLayouts_) {
                    vkDestroyDescriptorSetLayout(device_->getLogicalDevice(), setLayout, nullptr);
                }
                setLayouts_.clear();
            }
        }

        bool isValid() const override {
            return layout_ != VK_NULL_HANDLE && device_ != nullptr;
        }

        void* getNativeHandle() const override {
            return reinterpret_cast<void*>(layout_);
        }

        size_t getMemoryUsage() const override {
            return 0; // 管线布局不占用GPU内存
        }

        VkPipelineLayout getVkPipelineLayout() const { return layout_; }
        const std::vector<VkDescriptorSetLayout>& getVkDescriptorSetLayouts() const { return setLayouts_; }
        const PipelineLayoutDesc& getDesc() const { return desc_; }

    private:
        Device::Ptr device_;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
        std::vector<VkDescriptorSetLayout> setLayouts_;
        PipelineLayoutDesc desc_;
    };

    // ==================== Vulkan管线数据 ====================

    class VulkanPipelineData : public ResourceData {
    public:
        VulkanPipelineData(Device::Ptr device, VkPipeline pipeline, PipelineLayoutHandle layoutHandle,
            PipelineType type, const GraphicsPipelineDesc& desc)
            : device_(device), pipeline_(pipeline), layoutHandle_(layoutHandle),
            type_(type), graphicsDesc_(desc) {}

        VulkanPipelineData(Device::Ptr device, VkPipeline pipeline, PipelineLayoutHandle layoutHandle,
            PipelineType type, const ComputePipelineDesc& desc)
            : device_(device), pipeline_(pipeline), layoutHandle_(layoutHandle),
            type_(type), computeDesc_(desc) {}

        ~VulkanPipelineData() override {
            release();
        }

        void release() override {
            if (device_ && device_->getLogicalDevice() && pipeline_ != VK_NULL_HANDLE) {
                vkDestroyPipeline(device_->getLogicalDevice(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            // 减少布局引用计数
            if (layoutHandle_.isValid()) {
                RHI_PIPELINE_LAYOUT_SLOT.release(layoutHandle_);
            }
        }

        bool isValid() const override {
            return pipeline_ != VK_NULL_HANDLE && device_ != nullptr;
        }

        void* getNativeHandle() const override {
            return reinterpret_cast<void*>(pipeline_);
        }

        size_t getMemoryUsage() const override {
            return 0; // 管线不占用GPU内存
        }

        VkPipeline getVkPipeline() const { return pipeline_; }
        PipelineLayoutHandle getLayoutHandle() const { return layoutHandle_; }
        PipelineType getType() const { return type_; }

    private:
        Device::Ptr device_;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        PipelineLayoutHandle layoutHandle_;
        PipelineType type_;

        // 存储描述信息（根据需要选择）
        std::optional<GraphicsPipelineDesc> graphicsDesc_;
        std::optional<ComputePipelineDesc> computeDesc_;
    };

} // namespace StarryEngine::RHI