#pragma once
#include "../backend/vulkan/Instance.hpp"
#include "../backend/vulkan/QueueHandles.hpp"
#include "../backend/vulkan/Device.hpp"
#include "../backend/vulkan/Swapchain.hpp"
#include "../backend/vulkan/FrameContext.hpp"
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
        bool update(const void* data, uint64_t size, uint64_t offset = 0) override;
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
        
        // 辅助函数
        VkBufferUsageFlags getBufferUsageFlags() const;
        void uploadInitialData();
        bool updateDataViaStagingBuffer(const void* data, uint64_t size, uint64_t offset);
        bool updateDataViaDirectMapping(const void* data, uint64_t size, uint64_t offset);
        
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

        DescriptorSetLayoutHandle getLayoutHandle(uint32_t setIndex) const {
            if (setIndex < mLayoutHandles.size()) return mLayoutHandles[setIndex];
            return DescriptorSetLayoutHandle::Null();
        }

    private:
        Device::Ptr mDevice;
        VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
        PipelineLayoutDesc mDesc;
        std::vector<VkDescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<DescriptorSetLayoutHandle> mLayoutHandles;
    };

    class RHI_VK_Pipeline : public RHIPipeline {
    public:
        RHI_VK_Pipeline(
            Device::Ptr device,
            const GraphicsPipelineDesc& desc,
			std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
			VkPipelineLayout pipelineLayout = VK_NULL_HANDLE,
			VkRenderPass renderPass = VK_NULL_HANDLE
        );

        ~RHI_VK_Pipeline() override { release();}

        PipelineType getType() const override { return mDesc.type; }

        PipelineLayoutHandle getLayout() const override { return mDesc.pipelineLayoutHandle; }

        void setLayout(RHI_VK_PipelineLayout* layout) {
			
		}

        void* getNativeHandle() const override { return reinterpret_cast<void*>(mPipeline); }

        bool isValid() const override { return mPipeline != VK_NULL_HANDLE; }

        size_t getMemoryUsage() const override {return sizeof(*this) + mDesc.debugName.size();}

        const char* getTypeName() const override {return mDesc.type == PipelineType::Graphics ? "VK_GraphicsPipeline" : "VK_ComputePipeline";}

        bool isComputePipeline() const override { return mDesc.type == PipelineType::Compute; }

        bool isGraphicsPipeline() const override { return mDesc.type == PipelineType::Graphics; }

        bool isRayTracingPipeline() const override { return mDesc.type == PipelineType::RayTracing; }

        bool canBeReloaded() const override { return false; } 

        bool reload(const void* newPipelineData) override { return false; }

        void release() override;

    private:

        VkStencilOpState convertStencilOpState(const StencilOpState& state);

    private:
        Device::Ptr mDevice;
        VkPipeline mPipeline = VK_NULL_HANDLE;
        GraphicsPipelineDesc mDesc;
		std::vector<VkPipelineShaderStageCreateInfo> mShaderStages;
		VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
		VkRenderPass mRenderPass = VK_NULL_HANDLE;
    };


    class RHI_VK_CommandPool : public RHICommandPool {
    public:

		RHI_VK_CommandPool(Device::Ptr device, CommandPoolDesc desc);
		~RHI_VK_CommandPool() { release(); }

        void release() override;
		bool isValid() const override { return mCommandPool != VK_NULL_HANDLE; }
		void* getNativeHandle() const override { return reinterpret_cast<void*>(mCommandPool); }
		size_t getMemoryUsage() const override { return sizeof(*this) + mDesc.debugName.size(); }
		const char* getTypeName() const override { return "VK_CommandPool"; }

        std::vector<VkCommandBuffer> allocateCommandBuffers(uint32_t count,CommandBufferLevel level);
        void freeCommandBuffers(const std::vector<VkCommandBuffer>& commandBuffers);

        VkCommandBuffer allocateCommandBuffer(CommandBufferLevel level);
        void freeCommandBuffer(const VkCommandBuffer& commandBuffer);

        void reset(bool releaseResources = false) override;

		QueueType getQueueType() const override { return mDesc.queueType; }

    private:
		Device::Ptr mDevice;
		CommandPoolDesc mDesc;
		VkCommandPool mCommandPool = VK_NULL_HANDLE;
    };

    class RHI_VK_CommandBuffer : public RHICommandBuffer {
    public:
        RHI_VK_CommandBuffer(Device::Ptr device, CommandBufferDesc desc, RHI_VK_CommandPool* cmdPool);
        ~RHI_VK_CommandBuffer() override { release(); }

        bool isValid() const override { return mCommandBuffer != VK_NULL_HANDLE; }
        void* getNativeHandle() const override { return reinterpret_cast<void*>(mCommandBuffer); }
        size_t getMemoryUsage() const override { return sizeof(*this) + mDesc.debugName.size(); }
        const char* getTypeName() const override { return "VK_CommandBuffer"; }
        CommandBufferLevel getLevel() const override { return mDesc.level; }
        CommandBufferType getType() const override { return mDesc.type; }
        bool isOneTimeSubmit() const override { return mDesc.oneTimeSubmit; }
        bool isSimultaneousUse() const override { return mDesc.simultaneousUse; }

        void release() override;

        // 生命周期
        void begin()override;
        void end()override;
        void reset(bool releaseResources = false)override;

    private:
        Device::Ptr mDevice;
        CommandBufferDesc mDesc;
        VkCommandBuffer mCommandBuffer = VK_NULL_HANDLE;
		RHI_VK_CommandPool* mCommandPool;
    };

    class RHI_VK_Framebuffer : public RHIFramebuffer {
    public:
        RHI_VK_Framebuffer(Device::Ptr device, const FramebufferDesc& desc);
		~RHI_VK_Framebuffer() { release(); }
        void release() override;

		bool isValid() const override { return mFramebuffer != VK_NULL_HANDLE; }
		void* getNativeHandle() const override { return reinterpret_cast<void*>(mFramebuffer); }
		size_t getMemoryUsage() const override { return sizeof(*this) + mDesc.debugName.size(); }
		const char* getTypeName() const override { return "VK_Framebuffer"; }

		const FramebufferDesc& getDesc() const override { return mDesc; }
		Extent2D getExtent() const override { return mDesc.extent; }
		uint32_t getLayerCount() const override { return mDesc.layers; }

        uint32_t getAttachmentCount() const override {return static_cast<uint32_t>(mDesc.attachments.size());}
    private:
        Device::Ptr mDevice;
		FramebufferDesc mDesc;
        VkFramebuffer mFramebuffer = VK_NULL_HANDLE;

        std::vector<VkImageView> mAttachments;
    };

    // ==================== 纹理类 ====================
    class RHI_VK_Texture : public RHITexture {
    public:
        RHI_VK_Texture(Device::Ptr device, const TextureDesc& desc);
        ~RHI_VK_Texture() override;

        // 实现 IResource
        void release() override;
        bool isValid() const override;
        void* getNativeHandle() const override;      // 返回默认图像视图
        size_t getMemoryUsage() const override;
        const char* getTypeName() const override { return "VK_Texture"; }

        // 实现 RHITexture
        TextureType getType() const override { return mDesc.type; }
        Format getFormat() const override { return mDesc.format; }
        Extent3D getExtent() const override { return mDesc.extent; }
        uint32_t getMipLevels() const override { return mDesc.mipLevels; }
        uint32_t getArrayLayers() const override { return mDesc.arrayLayers; }
        uint32_t getSampleCount() const override { return mDesc.sampleCount; }
        ImageLayout getCurrentLayout() const override { return mCurrentLayout; }

        VkImageView createVkImageView(const ImageSubresourceRange& range, VkImageViewType viewType);
        void* createView(const ImageSubresourceRange& range, ImageViewType viewType = ImageViewType::Auto) override;
        void destroyView(void* view) override;
        void* getDefaultView() const override;

        void transitionLayout(ImageLayout newLayout,
            PipelineStage srcStage,
            PipelineStage dstStage,
            AccessFlags srcAccess,
            AccessFlags dstAccess,
            const ImageSubresourceRange& range) override;

        void copyFromBuffer(RHIBuffer* srcBuffer, const std::vector<BufferImageCopyRegion>& regions) override;
        void copyToBuffer(RHIBuffer* dstBuffer, const std::vector<BufferImageCopyRegion>& regions) override;
        void copyFromTexture(RHITexture* srcTexture, const std::vector<ImageCopyRegion>& regions) override;
		void update(const void* data, size_t size, const ImageSubresourceRange& range) override;
        void generateMipmaps() override;

    private:
        void copyFromBuffer(VkBuffer srcBuffer, const std::vector<BufferImageCopyRegion>& regions);

        Device::Ptr mDevice;
        TextureDesc mDesc;

        // 图像和分配
        bool mUsingVMA;
        union {
            VMAImageFull vmaImage;
            TraditionalImageFull traditionalImage;
        };
        ImageLayout mCurrentLayout = ImageLayout::Undefined;

        // 视图管理
        struct ViewInfo {
            VkImageView view;
            ImageSubresourceRange range;
        };
        std::unordered_map<uint64_t, ViewInfo> mViews;
        uint64_t mNextViewKey = 1;
        void* mDefaultView = nullptr;
        RHI::Format mActualFormat;

        // 辅助函数
        void createTexture();
    };

    // ==================== 采样器类 ====================
    class RHI_VK_Sampler : public RHISampler {
    public:
        RHI_VK_Sampler(Device::Ptr device, const SamplerDesc& desc);
        ~RHI_VK_Sampler() override;

        // 实现 IResource
        void release() override;
        bool isValid() const override;
        void* getNativeHandle() const override;
        size_t getMemoryUsage() const override;
        const char* getTypeName() const override { return "VK_Sampler"; }

        // 实现 RHISampler
        const SamplerDesc& getDesc() const override { return mDesc; }

    private:
        Device::Ptr mDevice;
        SamplerDesc mDesc;
        VkSampler mSampler = VK_NULL_HANDLE;

        void createSampler();
        VkBool32 toVkBool(bool b) { return b ? VK_TRUE : VK_FALSE; }
    };


    // RHI_VK_DescriptorSetLayout.hpp
    class RHI_VK_DescriptorSetLayout : public RHIDescriptorSetLayout {
    public:
        RHI_VK_DescriptorSetLayout(Device::Ptr device, const DescriptorSetLayoutDesc& desc);
        ~RHI_VK_DescriptorSetLayout() override;

        void release() override;
        bool isValid() const override { return mLayout != VK_NULL_HANDLE; }
        void* getNativeHandle() const override { return reinterpret_cast<void*>(mLayout); }
        size_t getMemoryUsage() const override { return 0; } // 布局本身内存很小，不计入
        const char* getTypeName() const override { return "Vulkan_DescriptorSetLayout"; }

        const std::vector<DescriptorSetLayoutBinding>& getBindings() const override { return mDesc.bindings; }
        uint32_t getBindingCount() const override { return static_cast<uint32_t>(mDesc.bindings.size()); }
        bool isCompatibleWith(const RHIDescriptorSetLayout* other) const override;
        const DescriptorSetLayoutDesc& getDesc() const { return mDesc; }

    private:
        Device::Ptr mDevice;
        VkDescriptorSetLayout mLayout = VK_NULL_HANDLE;
        DescriptorSetLayoutDesc mDesc;
        std::vector<VkSampler> mImmutableSamplers;
    };

    class RHI_VK_DescriptorPool : public RHIDescriptorPool {
    public:
        RHI_VK_DescriptorPool(Device::Ptr device, const DescriptorPoolDesc& desc);
        ~RHI_VK_DescriptorPool() override { release(); }

        void release() override;
        bool isValid() const override { return mPool != VK_NULL_HANDLE; }
        void* getNativeHandle() const override { return reinterpret_cast<void*>(mPool); }
        size_t getMemoryUsage() const override { return sizeof(*this); }
        const char* getTypeName() const override { return "Vulkan_DescriptorPool"; }

        std::vector<std::unique_ptr<RHIDescriptorSet>> allocateDescriptorSets(
            const std::vector<RHIDescriptorSetLayout*>& layouts) override;
        void freeDescriptorSets(const std::vector<DescriptorSetHandle>& descriptorSets) override;
        void reset() override;
        uint32_t getMaxSets() const override { return mDesc.maxSets; }
        uint32_t getRemainingSets() const override;

    private:
        Device::Ptr mDevice;
        VkDescriptorPool mPool = VK_NULL_HANDLE;
        DescriptorPoolDesc mDesc;
        std::atomic<uint32_t> mAllocatedSets{ 0 };
    };

    class RHI_VK_DescriptorSet : public RHIDescriptorSet {
    public:
        RHI_VK_DescriptorSet(Device::Ptr device, VkDescriptorSet set,
            RHI_VK_DescriptorPool* pool,
            RHIDescriptorSetLayout* layout);
        ~RHI_VK_DescriptorSet() override;

        void release() override;
        bool isValid() const override { return mSet != VK_NULL_HANDLE; }
        void* getNativeHandle() const override { return reinterpret_cast<void*>(mSet); }
        size_t getMemoryUsage() const override { return 0; }
        const char* getTypeName() const override { return "Vulkan_DescriptorSet"; }

        // 写入方法
        void writeBuffer(uint32_t binding, uint32_t arrayElement,
            RHIBuffer* buffer, uint64_t offset = 0, uint64_t range = 0) override;
        void writeTexture(uint32_t binding, uint32_t arrayElement,
            RHITexture* texture, RHISampler* sampler = nullptr,
            ImageLayout layout = ImageLayout::ShaderReadOnly) override;
        void writeSampler(uint32_t binding, uint32_t arrayElement,
            RHISampler* sampler) override;
        void writeAccelerationStructure(uint32_t binding, uint32_t arrayElement,
            RHIAccelerationStructure* accelerationStructure) override;
        void writeInlineUniformBlock(uint32_t binding, uint32_t offset,
            uint32_t size, const void* data) override;
        void update() override;
        void copyFrom(const RHIDescriptorSet* src, const std::vector<DescriptorCopy>& copies) override;

        void writeInputAttachment(uint32_t binding, uint32_t arrayElement,
            RHITexture* texture, ImageLayout layout) override;

    private:
        VkDescriptorType getBindingDescriptorType(uint32_t binding) const {
            if (!mLayout) return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // fallback
            const auto& bindings = mLayout->getBindings();
            for (const auto& b : bindings) {
                if (b.binding == binding) {
                    return FUNC::RHI_TO_VK_DescriptorType(b.type);
                }
            }
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // fallback
        }


        Device::Ptr mDevice;
        VkDescriptorSet mSet = VK_NULL_HANDLE;
        RHI_VK_DescriptorPool* mPool;        // 所属池（用于释放判断）
        RHIDescriptorSetLayout* mLayout;      // 布局，用于查询绑定信息

        std::vector<VkWriteDescriptorSet> mPendingWrites;
        std::vector<VkDescriptorBufferInfo> mBufferInfos;   // 确保指针有效
        std::vector<VkDescriptorImageInfo> mImageInfos;
        std::vector<VkWriteDescriptorSetAccelerationStructureKHR> mAccelStructs; // 可选
    };

} // namespace StarryEngine::RHI