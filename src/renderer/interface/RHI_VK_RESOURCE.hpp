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
#include <shaderc/shaderc.hpp>

namespace StarryEngine::RHI {
    class RHI_VK_ShaderModule : public RHIShaderModule {
    public:
        RHI_VK_ShaderModule(Device::Ptr device, ShaderModuleDesc desc);

        ~RHI_VK_ShaderModule() = default;

        void release() override;

        bool isValid() const { return true; }
        void* getNativeHandle() const { return mShaderModule; }
        size_t getMemoryUsage() const { return 0; }
        const char* getTypeName() const { return ""; }

        ShaderStage getStage() const override { return mDesc.stage; }
        const std::string& getEntryPoint() const override { return mDesc.entryPoint; }

        // TODO
        bool hasReflectionData() const override { return false; }
        const void* getReflectionData() const override { return nullptr; }
        void setDefines(const std::vector<std::string>& defines) override{}
        void setIncludePaths(const std::vector<std::string>& includePaths) override{}
        bool recompile(const std::vector<uint8_t>& newBytecode) override { return false; }

        std::vector<uint32_t> compileGLSL(
            const std::string& source,
            shaderc_shader_kind kind,
            const std::vector<std::pair<std::string, std::string>>& macros,
            const std::string& debugName);


    private:
        Device::Ptr mDevice;
        ShaderModuleDesc mDesc;
        VkShaderModule mShaderModule = VK_NULL_HANDLE;
        shaderc::Compiler mCompiler;
    };


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

        ~RHI_VK_Pipeline() override { release();}

        PipelineType getType() const override { return mType; }

        RHIPipelineLayout* getLayout() const override { return mLayout.get(); }

        void setLayout(std::unique_ptr<RHI_VK_PipelineLayout> layout) { mLayout = std::move(layout); }

        void* getNativeHandle() const override { return reinterpret_cast<void*>(mPipeline); }

        bool isValid() const override { return mPipeline != VK_NULL_HANDLE; }

        size_t getMemoryUsage() const override {return sizeof(*this) + mDesc.debugName.size();}

        const char* getTypeName() const override {return mType == PipelineType::Graphics ? "VK_GraphicsPipeline" : "VK_ComputePipeline";}

        bool isComputePipeline() const override { return mType == PipelineType::Compute; }

        bool isGraphicsPipeline() const override { return mType == PipelineType::Graphics; }

        bool isRayTracingPipeline() const override { return mType == PipelineType::RayTracing; }

        bool canBeReloaded() const override { return false; } 

        bool reload(const void* newPipelineData) override { return false; }

        void release() override;

        void setShaderState();

    private:
        void createGraphicsPipeline();

        VkStencilOpState convertStencilOpState(const StencilOpState& state);

    private:
        Device::Ptr mDevice;
        VkPipeline mPipeline = VK_NULL_HANDLE;
        std::unique_ptr<RHI_VK_PipelineLayout> mLayout;
        PipelineType mType;
        GraphicsPipelineDesc mDesc;
    };

} // namespace StarryEngine::RHI