#pragma once

#include <vector>
#include <memory>
#include <stdexcept>

#include <interface/RHIEnums.hpp>
#include <interface/RHIHandles.hpp>
#include <interface/RHIStructs.hpp>
#include <interface/IRHIResources.hpp>

#include <interface/vulkan/VulkanDevice.hpp>
#include <interface/vulkan/VulkanConversion.hpp>


namespace StarryEngine::RHI {

    class VulkanShaderModule : public RHIShaderModule {
    public:
        VulkanShaderModule(std::shared_ptr<VulkanDevice> device, ShaderModuleDesc desc);

        ~VulkanShaderModule() = default;

        void release() override;

        bool isValid() const { return true; }
        void* getNativeHandle() const { return mShaderModule; }
        size_t getMemoryUsage() const { return 0; }

        ShaderStage getStage() const override { return mDesc.stage; }
        const std::string& getEntryPoint() const override { return mDesc.entryPoint; }

        bool hasReflectionData() const override { return false; }
        const void* getReflectionData() const override { return nullptr; }
        void setDefines(const std::vector<std::string>& defines) override{}
        void setIncludePaths(const std::vector<std::string>& includePaths) override{}
        bool recompile(const std::vector<uint8_t>& newBytecode) override { return false; }

    private:
        std::shared_ptr<VulkanDevice> mDevice;
        ShaderModuleDesc mDesc;
        VkShaderModule mShaderModule = VK_NULL_HANDLE;
        shaderc::Compiler mCompiler;
    };

    class VulkanBuffer : public RHIBuffer {
    public:
        VulkanBuffer(std::shared_ptr<VulkanDevice> device, const BufferDesc& desc);
        ~VulkanBuffer() override;

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
        
        std::shared_ptr<VulkanDevice> mDevice;
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


    class VulkanRenderPass : public RHIRenderPass {
    public:
        VulkanRenderPass(
            std::shared_ptr<VulkanDevice> device,
            const RenderPassDesc& desc
        );

        ~VulkanRenderPass() { release(); };

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
            const VulkanRenderPass* vkOther = dynamic_cast<const VulkanRenderPass*>(other);
            if (!vkOther) return false;
            return mDesc == vkOther->mDesc;
        }

    private:
        std::shared_ptr<VulkanDevice> mDevice;
        RenderPassDesc mDesc;

		VkRenderPass mVkRenderPass = VK_NULL_HANDLE; 
    };

    class VulkanPipelineLayout : public RHIPipelineLayout {
    public:
        VulkanPipelineLayout(
            std::shared_ptr<VulkanDevice> device,
            const PipelineLayoutDesc& desc,
            std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts = {}
        );

        ~VulkanPipelineLayout() override { release(); }

        const PipelineLayoutDesc& getDesc() const override { return mDesc; }

        uint32_t getDescriptorSetCount() const override {return static_cast<uint32_t>(mDesc.descriptorSetLayouts.size());}

        uint32_t getPushConstantRangeCount() const override {return static_cast<uint32_t>(mDesc.pushConstants.size());}

        uint32_t getBindingPoint(uint32_t set, uint32_t binding) const override;

        bool isValid() const override { return mPipelineLayout != VK_NULL_HANDLE; }

        void* getNativeHandle() const override { return reinterpret_cast<void*>(mPipelineLayout); }

        void release() override;

        size_t getMemoryUsage() const override;

        VkPipelineLayout getVkPipelineLayout() const { return mPipelineLayout; }

        DescriptorSetLayoutHandle getLayoutHandle(uint32_t setIndex) const {
            if (setIndex < mLayoutHandles.size()) return mLayoutHandles[setIndex];
            return DescriptorSetLayoutHandle::Null();
        }

    private:
        std::shared_ptr<VulkanDevice> mDevice;
        VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
        PipelineLayoutDesc mDesc;
        std::vector<VkDescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<DescriptorSetLayoutHandle> mLayoutHandles;
    };

    class VulkanGraphicPipeline : public RHIPipeline {
    public:
        VulkanGraphicPipeline(
            std::shared_ptr<VulkanDevice> device,
            const GraphicsPipelineDesc& desc,
			std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
			VkPipelineLayout pipelineLayout = VK_NULL_HANDLE,
			VkRenderPass renderPass = VK_NULL_HANDLE
        );

        ~VulkanGraphicPipeline() override { release();}

        PipelineType getType() const override { return mDesc.type; }

        PipelineLayoutHandle getLayout() const override { return mDesc.pipelineLayoutHandle; }

        void* getNativeHandle() const override { return reinterpret_cast<void*>(mPipeline); }

        bool isValid() const override { return mPipeline != VK_NULL_HANDLE; }

        size_t getMemoryUsage() const override {return sizeof(*this) + mDesc.debugName.size();}

        bool isComputePipeline() const override { return mDesc.type == PipelineType::Compute; }

        bool isGraphicsPipeline() const override { return mDesc.type == PipelineType::Graphics; }

        bool isRayTracingPipeline() const override { return mDesc.type == PipelineType::RayTracing; }

        bool canBeReloaded() const override { return false; } 

        bool reload(const void* newPipelineData) override { return false; }

        void release() override;

    private:

        VkStencilOpState convertStencilOpState(const StencilOpState& state);

    private:
        std::shared_ptr<VulkanDevice> mDevice;
        VkPipeline mPipeline = VK_NULL_HANDLE;
        GraphicsPipelineDesc mDesc;
		std::vector<VkPipelineShaderStageCreateInfo> mShaderStages;
		VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
		VkRenderPass mRenderPass = VK_NULL_HANDLE;
    };

    class VulkanComputePipeline : public RHIPipeline {
    public:
        VulkanComputePipeline(
            std::shared_ptr<VulkanDevice> device,
            const ComputePipelineDesc& desc,
            VkPipelineShaderStageCreateInfo shaderStage,   
            VkPipelineLayout pipelineLayout
        );

        ~VulkanComputePipeline() override { release(); }

        PipelineType getType() const override { return PipelineType::Compute; }
        PipelineLayoutHandle getLayout() const override { return mDesc.pipelineLayoutHandle; }
        void* getNativeHandle() const override { return reinterpret_cast<void*>(mPipeline); }
        bool isValid() const override { return mPipeline != VK_NULL_HANDLE; }
        size_t getMemoryUsage() const override { return sizeof(*this) + mDesc.debugName.size(); }
        bool isComputePipeline() const override { return true; }
        bool isGraphicsPipeline() const override { return false; }
        bool isRayTracingPipeline() const override { return false; }
        bool canBeReloaded() const override { return false; }
        bool reload(const void*) override { return false; }
        void release() override;

        VkPipeline getVkPipeline() const { return mPipeline; }

    private:
        std::shared_ptr<VulkanDevice> mDevice;
        VkPipeline mPipeline = VK_NULL_HANDLE;
        ComputePipelineDesc mDesc;
        VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
    };


    class VulkanCommandPool : public RHICommandPool {
    public:

		VulkanCommandPool(std::shared_ptr<VulkanDevice> device, CommandPoolDesc desc);
		~VulkanCommandPool() { release(); }

        void release() override;
		bool isValid() const override { return mCommandPool != VK_NULL_HANDLE; }
		void* getNativeHandle() const override { return reinterpret_cast<void*>(mCommandPool); }
		size_t getMemoryUsage() const override { return sizeof(*this) + mDesc.debugName.size(); }

        std::vector<VkCommandBuffer> allocateCommandBuffers(uint32_t count,CommandBufferLevel level);
        void freeCommandBuffers(const std::vector<VkCommandBuffer>& commandBuffers);

        VkCommandBuffer allocateCommandBuffer(CommandBufferLevel level);
        void freeCommandBuffer(const VkCommandBuffer& commandBuffer);

        void reset(bool releaseResources = false) override;

		QueueType getQueueType() const override { return mDesc.queueType; }

    private:
		std::shared_ptr<VulkanDevice> mDevice;
		CommandPoolDesc mDesc;
		VkCommandPool mCommandPool = VK_NULL_HANDLE;
    };

    class VulkanCommandBuffer : public RHICommandBuffer {
    public:
        VulkanCommandBuffer(std::shared_ptr<VulkanDevice> device, CommandBufferDesc desc, VulkanCommandPool* cmdPool);
        ~VulkanCommandBuffer() override { release(); }

        bool isValid() const override { return mCommandBuffer != VK_NULL_HANDLE; }
        void* getNativeHandle() const override { return reinterpret_cast<void*>(mCommandBuffer); }
        size_t getMemoryUsage() const override { return sizeof(*this) + mDesc.debugName.size(); }
        CommandBufferLevel getLevel() const override { return mDesc.level; }
        bool isOneTimeSubmit() const override { return mDesc.oneTimeSubmit; }
        bool isSimultaneousUse() const override { return mDesc.simultaneousUse; }

        void release() override;

        // 生命周期
        void begin()override;
        void end()override;
        void reset(bool releaseResources = false)override;

    private:
        std::shared_ptr<VulkanDevice> mDevice;
        CommandBufferDesc mDesc;
        VkCommandBuffer mCommandBuffer = VK_NULL_HANDLE;
		VulkanCommandPool* mCommandPool;
    };

    class VulkanFramebuffer : public RHIFramebuffer {
    public:
        VulkanFramebuffer(std::shared_ptr<VulkanDevice> device,
                         RHIRenderPass* renderPass,
                         const std::vector<RHITexture*>& attachments,
                         const FramebufferDesc& desc);
		~VulkanFramebuffer() { release(); }
        void release() override;

		bool isValid() const override { return mFramebuffer != VK_NULL_HANDLE; }
		void* getNativeHandle() const override { return reinterpret_cast<void*>(mFramebuffer); }
		size_t getMemoryUsage() const override { return sizeof(*this) + mDesc.debugName.size(); }

		const FramebufferDesc& getDesc() const override { return mDesc; }
		Extent2D getExtent() const override { return mDesc.extent; }
		uint32_t getLayerCount() const override { return mDesc.layers; }

        uint32_t getAttachmentCount() const override { return mAttachmentCount; }
    private:
        std::shared_ptr<VulkanDevice> mDevice;
		FramebufferDesc mDesc;
        VkFramebuffer mFramebuffer = VK_NULL_HANDLE;

        std::vector<VkImageView> mAttachments;
        uint32_t mAttachmentCount = 0;
    };

    // ==================== 纹理类 ====================
    class VulkanTexture : public RHITexture {
    public:
        VulkanTexture(std::shared_ptr<VulkanDevice> device, const TextureDesc& desc);
        ~VulkanTexture() override;

        // 实现 IResource
        void release() override;
        bool isValid() const override;
        void* getNativeHandle() const override;     
        size_t getMemoryUsage() const override;

        // 实现 RHITexture
        TextureType getType() const override { return mDesc.type; }
        Format getFormat() const override { return mDesc.format; }
        Extent3D getExtent() const override { return mDesc.extent; }
        uint32_t getMipLevels() const override { return mDesc.mipLevels; }
        uint32_t getArrayLayers() const override { return mDesc.arrayLayers; }
        uint32_t getSampleCount() const override { return mDesc.sampleCount; }
        void* getImageHandle() const override {
            return mUsingVMA ? (void*)vmaImage.image : (void*)traditionalImage.image;
        }
        //ImageLayout getCurrentLayout() const override { return mCurrentLayout; }

        VkImageView createVkImageView(const ImageSubresourceRange& range, VkImageViewType viewType);
        void* createView(const ImageSubresourceRange& range, ImageViewType viewType = ImageViewType::Auto) override;
        void destroyView(void* view) override;
        void* getDefaultView() const override;
        void* getSamplingView() const override;   

        void transitionLayout(ImageLayout newLayout,
            PipelineStage srcStage,
            PipelineStage dstStage,
            AccessFlag srcAccess,
            AccessFlag dstAccess,
            const ImageSubresourceRange& range) override;

        void copyFromBuffer(RHIBuffer* srcBuffer, const std::vector<BufferImageCopyRegion>& regions) override;
        void copyToBuffer(RHIBuffer* dstBuffer, const std::vector<BufferImageCopyRegion>& regions) override;
        void readbackAsync(RHIBuffer* dstBuffer, const std::vector<BufferImageCopyRegion>& regions,
                           VkCommandBuffer cmdBuf, VkFence fence);
        void copyFromTexture(RHITexture* srcTexture, const std::vector<ImageCopyRegion>& regions) override;
		void update(const void* data, size_t size, const ImageSubresourceRange& range) override;
        void generateMipmaps() override;
        void* getNativeHandleFromView(void* viewKey) override;
    private:
        void copyFromBuffer(VkBuffer srcBuffer, const std::vector<BufferImageCopyRegion>& regions);

        std::shared_ptr<VulkanDevice> mDevice;
        TextureDesc mDesc;

        // 图像和分配
        bool mUsingVMA;
        union {
            VMAImageFull vmaImage;
            TraditionalImageFull traditionalImage;
        };

        // 视图管理
        struct ViewInfo {
            VkImageView view;
            ImageSubresourceRange range;
        };
        std::unordered_map<uint64_t, ViewInfo> mViews;
        uint64_t mNextViewKey = 1;
        void* mDefaultView = nullptr;
        mutable VkImageView mDepthAspectView = VK_NULL_HANDLE;  // 懒创建：深度采样视图（DEPTH-only）
        RHI::Format mActualFormat;

        // 辅助函数
        void createTexture();

        struct SubresourceKey {
            uint32_t mipLevel;
            uint32_t arrayLayer;
            bool operator==(const SubresourceKey& other) const {
                return mipLevel == other.mipLevel && arrayLayer == other.arrayLayer;
            }
        };

        struct SubresourceKeyHash {
            size_t operator()(const SubresourceKey& key) const {
                return ((size_t)key.mipLevel << 32) | key.arrayLayer;
            }
        };

        std::unordered_map<SubresourceKey, ImageLayout, SubresourceKeyHash> m_subresourceLayouts;

        ImageLayout getSubresourceLayout(uint32_t mipLevel, uint32_t arrayLayer) const;

        void setSubresourceLayout(uint32_t mipLevel, uint32_t arrayLayer, ImageLayout layout);

        void forEachSubresource(const ImageSubresourceRange& range,
            std::function<void(uint32_t, uint32_t)> func);
    };

    // ==================== 采样器类 ====================
    class VulkanSampler : public RHISampler {
    public:
        VulkanSampler(std::shared_ptr<VulkanDevice> device, const SamplerDesc& desc);
        ~VulkanSampler() override;

        // 实现 IResource
        void release() override;
        bool isValid() const override;
        void* getNativeHandle() const override;
        size_t getMemoryUsage() const override;

        // 实现 RHISampler
        const SamplerDesc& getDesc() const override { return mDesc; }

    private:
        std::shared_ptr<VulkanDevice> mDevice;
        SamplerDesc mDesc;
        VkSampler mSampler = VK_NULL_HANDLE;

        void createSampler();
        VkBool32 toVkBool(bool b) { return b ? VK_TRUE : VK_FALSE; }
    };


    // VulkanDescriptorSetLayout.hpp
    class VulkanDescriptorSetLayout : public RHIDescriptorSetLayout {
    public:
        VulkanDescriptorSetLayout(std::shared_ptr<VulkanDevice> device, const DescriptorSetLayoutDesc& desc);
        ~VulkanDescriptorSetLayout() override;

        void release() override;
        bool isValid() const override { return mLayout != VK_NULL_HANDLE; }
        void* getNativeHandle() const override { return reinterpret_cast<void*>(mLayout); }
        size_t getMemoryUsage() const override { return 0; } // 布局本身内存很小，不计入

        const std::vector<DescriptorSetLayoutBinding>& getBindings() const override { return mDesc.bindings; }
        uint32_t getBindingCount() const override { return static_cast<uint32_t>(mDesc.bindings.size()); }
        bool isCompatibleWith(const RHIDescriptorSetLayout* other) const override;
        const DescriptorSetLayoutDesc& getDesc() const { return mDesc; }

    private:
        std::shared_ptr<VulkanDevice> mDevice;
        VkDescriptorSetLayout mLayout = VK_NULL_HANDLE;
        DescriptorSetLayoutDesc mDesc;
        std::vector<VkSampler> mImmutableSamplers;
    };

    class VulkanDescriptorPool : public RHIDescriptorPool {
    public:
        VulkanDescriptorPool(std::shared_ptr<VulkanDevice> device, const DescriptorPoolDesc& desc);
        ~VulkanDescriptorPool() override { release(); }

        void release() override;
        bool isValid() const override { return mPool != VK_NULL_HANDLE; }
        void* getNativeHandle() const override { return reinterpret_cast<void*>(mPool); }
        size_t getMemoryUsage() const override { return sizeof(*this); }

        std::vector<std::unique_ptr<RHIDescriptorSet>> allocateDescriptorSets(
            const std::vector<RHIDescriptorSetLayout*>& layouts) override;
        void reset() override;
        uint32_t getMaxSets() const override { return mDesc.maxSets; }
        uint32_t getRemainingSets() const override;

    private:
        std::shared_ptr<VulkanDevice> mDevice;
        VkDescriptorPool mPool = VK_NULL_HANDLE;
        DescriptorPoolDesc mDesc;
        std::atomic<uint32_t> mAllocatedSets{ 0 };
    };

    class VulkanDescriptorSet : public RHIDescriptorSet {
    public:
        VulkanDescriptorSet(std::shared_ptr<VulkanDevice> device, VkDescriptorSet set,
            VulkanDescriptorPool* pool,
            RHIDescriptorSetLayout* layout);
        ~VulkanDescriptorSet() override;

        void release() override;
        bool isValid() const override { return mSet != VK_NULL_HANDLE; }
        void* getNativeHandle() const override { return reinterpret_cast<void*>(mSet); }
        size_t getMemoryUsage() const override { return 0; }

        // 写入方法
        void writeBuffer(uint32_t binding, uint32_t arrayElement,
            RHIBuffer* buffer, uint64_t offset = 0, uint64_t range = 0) override;
        void writeTexture(uint32_t binding, uint32_t arrayElement,
            RHITexture* texture, RHISampler* sampler = nullptr,
            ImageLayout layout = ImageLayout::ShaderReadOnly) override;
        void writeSampler(uint32_t binding, uint32_t arrayElement,
            RHISampler* sampler) override;
        void update() override;
        void copyFrom(const RHIDescriptorSet* src, const std::vector<DescriptorCopy>& copies) override;
        void writeTextureCustomView(uint32_t binding, uint32_t arrayElement,void* imageView,RHISampler* sampler,ImageLayout layout)override;

        void writeInputAttachment(uint32_t binding, uint32_t arrayElement,
            RHITexture* texture, ImageLayout layout) override;

    private:
        VkDescriptorType getBindingDescriptorType(uint32_t binding) const;

        std::shared_ptr<VulkanDevice> mDevice;
        VkDescriptorSet mSet = VK_NULL_HANDLE;
        VulkanDescriptorPool* mPool;        // 所属池（用于释放判断）
        RHIDescriptorSetLayout* mLayout;      // 布局，用于查询绑定信息

        std::vector<VkDescriptorBufferInfo> mBufferInfos;   // 确保指针有效
        std::vector<VkDescriptorImageInfo> mImageInfos;
        std::vector<VkWriteDescriptorSetAccelerationStructureKHR> mAccelStructs; // 可选

        struct PendingTextureWrite {
            uint32_t binding;
            uint32_t arrayElement;
            uint32_t imageInfoIndex;
        };
        std::vector<PendingTextureWrite> mPendingTextureWrites;

        struct PendingBufferWrite {
            uint32_t binding;
            uint32_t arrayElement;
            uint32_t bufferInfoIndex;
        };
        std::vector<PendingBufferWrite> mPendingBufferWrites;
    };

} // namespace StarryEngine::RHI