#pragma once
#include "../backend/Instance.hpp"
#include "../backend/QueueHandles.hpp"
#include "../backend/Device.hpp"
#include "../backend/Swapchain.hpp"
#include "../backend/FrameContext.hpp"
#include "RHI_STRUCTS_RESOURCE.hpp"
#include "RHI_TO_VK_FUNC.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <stdexcept>

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

    class RHI_VK_Buffer : public RHIBuffer {
    public:
        RHI_VK_Buffer(std::shared_ptr<Device> device, const BufferDesc& desc);
        ~RHI_VK_Buffer() override;

        // 映射/取消映射
        void* map(uint64_t offset = 0, uint64_t size = 0) override;
        void unmap() override;

        // 数据更新
        void update(const void* data, uint64_t size, uint64_t offset = 0) override;
        void flush(uint64_t offset = 0, uint64_t size = 0) override;
        void invalidate(uint64_t offset = 0, uint64_t size = 0) override;

        void release() override;
        bool isValid() const override;
        void* getNativeHandle() const override;
        size_t getMemoryUsage() const override;
        const char* getTypeName() const override { return "Vulkan_Buffer"; }

        // 信息查询
        BufferType getType() const override { return mDesc.type; }
        uint64_t getSize() const override { return mDesc.size; }
        uint64_t getAlignment() const override { return mDesc.alignment; }
        bool isCPUVisible() const override;
        bool isGPUOnly() const override;
        bool isPersistentMapped() const override { return mDesc.persistentMapped; }

        // 视图创建
        void* createView(Format format, uint64_t offset = 0, uint64_t size = 0) override;
        void destroyView(void* view) override;

        // 屏障
        void transitionState(AccessFlag newAccess, PipelineStage newStage) override{
            
        }

    private:
        void createBuffer();
        void destroyBuffer();
        
        // 辅助函数
        VkBufferUsageFlags getBufferUsageFlags() const;
        void uploadInitialData();
        void updateDataViaStagingBuffer(const void* data, uint64_t size, uint64_t offset);
        void updateDataViaDirectMapping(const void* data, uint64_t size, uint64_t offset);
        
        // 内存映射管理
        void ensureMapped();
        void syncMappedMemory(bool flush);
        
        // 视图管理
        struct BufferViewInfo {
            VkBufferView view;
            Format format;
            uint64_t offset;
            uint64_t size;
        };
        
        std::shared_ptr<Device> mDevice;
        BufferDesc mDesc;
        
        VkBuffer mBuffer = VK_NULL_HANDLE;
        VmaAllocation mVmaAllocation = VK_NULL_HANDLE;      
        VkDeviceMemory mTraditionalMemory = VK_NULL_HANDLE; 
        bool mUsingVMA = false;
        
        void* mMappedPointer = nullptr;
        bool mIsMapped = false;
        bool mPersistentlyMapped = false;
        
        std::unordered_map<uint64_t, BufferViewInfo> mViews; 
        uint64_t mNextViewKey = 1;
        
        AccessFlag mCurrentAccess = AccessFlag::None;
        PipelineStage mCurrentStage = PipelineStage::TopOfPipe;
    };


    class RHI_VK_RenderPass : public RHIRenderPass {
    public:
        RHI_VK_RenderPass(
            Device::Ptr device,
            const RenderPassDesc& desc
        );

        ~RHI_VK_RenderPass() { release(); };

        void release() override;

        bool isValid() const override {
            return mVkRenderPass != VK_NULL_HANDLE;
        }

        void* getNativeHandle() const override {
            return reinterpret_cast<void*>(mVkRenderPass);
        }

        size_t getMemoryUsage() const override {
            // 粗略估计：对象本身大小 + 描述字符串大小 + Vulkan 对象占用
            size_t usage = sizeof(*this) + mDesc.debugName.size();
            usage += mVkRenderPass != VK_NULL_HANDLE ? 1024 : 0;
            return usage;
        }

        const char* getTypeName() const override {
			return "VK_RenderPass";
        }

        const RenderPassDesc& getDesc() const override {
			return mDesc;
        }

        uint32_t getAttachmentCount() const override {
			return static_cast<uint32_t>(mDesc.attachments.size());
        }

        uint32_t getSubpassCount() const override {
			return static_cast<uint32_t>(mDesc.subpasses.size());
        }
        
        bool isCompatibleWith(const RHIRenderPass* other) const override {
            // 简单实现：比较描述结构体是否完全相等
            const RHI_VK_RenderPass* vkOther = dynamic_cast<const RHI_VK_RenderPass*>(other);
            if (!vkOther) return false;
            return mDesc == vkOther->mDesc;
        }

    private:
        Device::Ptr mDevice;
        RenderPassDesc mDesc;

		VkRenderPass mVkRenderPass = VK_NULL_HANDLE; 
    };

    class RHI_VK_PipelineLayout : public RHIPipelineLayout {
    public:
        RHI_VK_PipelineLayout(
            Device::Ptr device,
            const PipelineLayoutDesc& desc,
            std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts = {}
        );

        ~RHI_VK_PipelineLayout() override { release(); }

        const PipelineLayoutDesc& getDesc() const override { return mDesc; }

        uint32_t getDescriptorSetCount() const override {return static_cast<uint32_t>(mDesc.descriptorSetLayouts.size());}

        uint32_t getPushConstantRangeCount() const override {return static_cast<uint32_t>(mDesc.pushConstants.size());}

        uint32_t getBindingPoint(uint32_t set, uint32_t binding) const override;

        bool isValid() const override { return mPipelineLayout != VK_NULL_HANDLE; }

        void* getNativeHandle() const override { return reinterpret_cast<void*>(mPipelineLayout); }

        void release() override;

        size_t getMemoryUsage() const override;

        const char* getTypeName() const override { return "VK_PipelineLayout"; }

        VkPipelineLayout getVkPipelineLayout() const { return mPipelineLayout; }
    private:
        Device::Ptr mDevice;
        VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
        PipelineLayoutDesc mDesc;
        std::vector<VkDescriptorSetLayout> mDescriptorSetLayouts;
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