#pragma once
#include "Instance.hpp"
#include "QueueHandles.hpp"
#include "Device.hpp"
#include "Swapchain.hpp"
#include "FrameContext.hpp"

#include "../interface/RHI_TYPES.hpp"
#include "../interface/RHI_TO_VK_FUNC.hpp"

namespace StarryEngine{

    class ConfigConverter {
    public:
        static StarryEngine::Instance::Config convertInstanceConfig(const StarryEngine::RHI::RHIInitConfig& rhiConfig);
        static StarryEngine::Device::Config convertDeviceConfig(const StarryEngine::RHI::RHIInitConfig& rhiConfig);
        static StarryEngine::SwapChainConfig convertSwapChainConfig(const StarryEngine::RHI::RHIInitConfig& rhiConfig);
        static StarryEngine::FrameContext::Config convertFrameContextConfig(const StarryEngine::RHI::RHIInitConfig& rhiConfig);

    private:
        static uint32_t convertFeatureLevel(StarryEngine::RHI::FeatureLevel level);
        static StarryEngine::RHI::MessageSeverity convertToRHISeverity(VkDebugUtilsMessageSeverityFlagBitsEXT vulkanSeverity);
        static StarryEngine::RHI::MessageSource convertToRHISource(VkDebugUtilsMessageTypeFlagsEXT vulkanType);
        static const char* messageSourceToString(StarryEngine::RHI::MessageSource source);
        static const char* messageSeverityToString(StarryEngine::RHI::MessageSeverity severity);
    };

    class RHI_VK_PipelineLayout :public RHI::RHIPipelineLayout {
    public:
        RHI_VK_PipelineLayout(
            Device::Ptr device,
            StarryEngine::RHI::PipelineLayoutDesc desc
        ):mDevice(device) {
            createPipelineLayout(desc);
        }

        ~RHI_VK_PipelineLayout() = default;

        const RHI::PipelineLayoutDesc& getDesc() const override {
            StarryEngine::RHI::PipelineLayoutDesc desc;
            return desc;
        }

        uint32_t getDescriptorSetCount() const override {
            return 0;
        }

        const std::vector<RHI::DescriptorSetLayoutBinding>& getDescriptorSetLayout(uint32_t set) const override {
            std::vector<RHI::DescriptorSetLayoutBinding> binding;

            return binding;
        }

        uint32_t getPushConstantRangeCount() const override {
            return 0;
        }

        const RHI::PushConstantRange& getPushConstantRange(uint32_t index) const override {

        }

        uint32_t getBindingPoint(uint32_t set, uint32_t binding) const override {
            return 0;
        }

        uint64_t getId() const override {
            return 0;
        }
        const std::string& getName() const override {
            return "";
        }
        bool isValid() const override {
            return false;
        }

    private:
        void createPipelineLayout(StarryEngine::RHI::PipelineLayoutDesc desc) {

            //VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            //pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            //pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
            //pipelineLayoutInfo.pSetLayouts = setLayouts.data();
            //pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
            //pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();

            mPipelineLayout = mDevice->createPipelineLayout();
        }

    private:
        Device::Ptr mDevice;

        VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
    };

    class RHI_VK_Pipeline : public RHI::RHIPipeline {
    public:
        RHI_VK_Pipeline(
            Device::Ptr device,
            StarryEngine::RHI::GraphicsPipelineDesc pipelineDesc,
            StarryEngine::RHI::PipelineType type
        ) : mDevice(device), mType(type) {

            createPipelineLayout(pipelineDesc);

            createGraphicsPipeline(pipelineDesc);
        }

        ~RHI_VK_Pipeline() override {
            destroy();
        }

        StarryEngine::RHI::PipelineType getType() const override { return mType; }

        StarryEngine::RHI::RHIPipelineLayout* getLayout() const override { return rhiLayout; }

        void destroy() {
            if (mDevice && mDevice->getLogicalDevice() != VK_NULL_HANDLE) {

                mDevice->destroyPipeline(mPipeline);
                mDevice->destroyPipelineLayout(mLayout);
            }
            if (rhiLayout) {
                delete rhiLayout; 
                rhiLayout = nullptr;
            }
        }
    private:
        void createPipelineLayout(StarryEngine::RHI::GraphicsPipelineDesc pipelineDesc) {
            rhiLayout = new RHI_VK_PipelineLayout(mDevice,pipelineDesc.layoutDesc);
        }

        void createGraphicsPipeline(StarryEngine::RHI::GraphicsPipelineDesc pipelineDesc) {

        }

    private:
        Device::Ptr mDevice;
        VkPipeline mPipeline = VK_NULL_HANDLE;
        VkPipelineLayout mLayout = VK_NULL_HANDLE;

        RHI_VK_PipelineLayout* rhiLayout = nullptr;
        StarryEngine::RHI::PipelineType mType = StarryEngine::RHI::PipelineType::Graphics;
    };


    class VulkanRHI{
    public:
        VulkanRHI() = default;
        ~VulkanRHI(){ clear(); }

        bool initialize(const StarryEngine::RHI::RHIInitConfig& config);

        StarryEngine::RHI::PipelineHandle createGraphicsPipeline(StarryEngine::RHI::GraphicsPipelineDesc pipelineDesc) {
            

            return StarryEngine::RHI::PipelineHandle::Null();
        }
        
        void clear();

    private:
        Instance::Ptr mInstance;
        Device::Ptr mDevice;
        SwapChain::Ptr mSwapChain;
        FrameContext::Ptr mFrameContext;
        VkSurfaceKHR mSurface = VK_NULL_HANDLE;

        

        std::vector<RHI_VK_Pipeline> mPipelinePools;
    };
}
