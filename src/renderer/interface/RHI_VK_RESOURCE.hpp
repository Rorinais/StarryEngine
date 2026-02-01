#pragma once
#include "../backend/Instance.hpp"
#include "../backend/QueueHandles.hpp"
#include "../backend/Device.hpp"
#include "../backend/Swapchain.hpp"
#include "../backend/FrameContext.hpp"
#include "RHI_STRUCTS_RESOURCE.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <stdexcept>

namespace StarryEngine::RHI {

    class RHI_VK_PipelineLayout : public RHIPipelineLayout {
    public:
        RHI_VK_PipelineLayout(
            Device::Ptr device,
            const PipelineLayoutDesc& desc
        ) : mDevice(device), mDesc(desc) {
            createPipelineLayout();
        }

        ~RHI_VK_PipelineLayout() override { destroy(); }

        void destroy();

        const PipelineLayoutDesc& getDesc() const override { return mDesc; }

        uint32_t getDescriptorSetCount() const override {return static_cast<uint32_t>(mDesc.descriptorSets.size());}

        const std::vector<DescriptorSetLayoutBinding>& getDescriptorSetLayout(uint32_t set) const override;

        uint32_t getPushConstantRangeCount() const override {return static_cast<uint32_t>(mDesc.pushConstants.size());}

        const PushConstantRange& getPushConstantRange(uint32_t index) const override;

        uint32_t getBindingPoint(uint32_t set, uint32_t binding) const override;

        bool isValid() const override { return mPipelineLayout != VK_NULL_HANDLE; }

        void* getNativeHandle() const override { return reinterpret_cast<void*>(mPipelineLayout); }

        void release() override { destroy(); }

        size_t getMemoryUsage() const override;

        const char* getTypeName() const override { return "VK_PipelineLayout"; }

        VkPipelineLayout getVkPipelineLayout() const { return mPipelineLayout; }

        VkDescriptorSetLayout getVkDescriptorSetLayout(uint32_t set) const;

    private:
        void createPipelineLayout();

    private:
        Device::Ptr mDevice;
        VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
        PipelineLayoutDesc mDesc;
        std::vector<VkDescriptorSetLayout> mVkDescriptorSetLayouts;
    };

    class RHI_VK_Pipeline : public RHIPipeline {
    public:
        RHI_VK_Pipeline(
            Device::Ptr device,
            const GraphicsPipelineDesc& pipelineDesc,
            PipelineType type,
            std::unique_ptr<RHI_VK_PipelineLayout> layout = nullptr
        );

        ~RHI_VK_Pipeline() override {destroy();}

        PipelineType getType() const override { return mType; }

        RHIPipelineLayout* getLayout() const override { return mLayout.get(); }

        void setLayout(std::unique_ptr<RHI_VK_PipelineLayout> layout) {mLayout = std::move(layout);}

        void* getNativeHandle() const override { return reinterpret_cast<void*>(mPipeline); }

        void release() override { destroy(); }

        bool isValid() const override { return mPipeline != VK_NULL_HANDLE; }

        size_t getMemoryUsage() const override {return sizeof(*this) + mDesc.debugName.size();}

        const char* getTypeName() const override {return mType == PipelineType::Graphics ? "VK_GraphicsPipeline" : "VK_ComputePipeline";}

        bool isComputePipeline() const override { return mType == PipelineType::Compute; }

        bool isGraphicsPipeline() const override { return mType == PipelineType::Graphics; }

        bool isRayTracingPipeline() const override { return mType == PipelineType::RayTracing; }

        bool canBeReloaded() const override { return false; } 

        bool reload(const void* newPipelineData) override { return false; }

        void destroy();

    private:
        void createGraphicsPipeline();

        VkStencilOpState convertStencilOpState(const StencilOpState& state);

        VkShaderModule createShaderModule(const ShaderModuleDesc& desc);

    private:
        Device::Ptr mDevice;
        VkPipeline mPipeline = VK_NULL_HANDLE;
        std::unique_ptr<RHI_VK_PipelineLayout> mLayout;
        PipelineType mType;
        GraphicsPipelineDesc mDesc;
    };

} // namespace StarryEngine::RHI