#pragma once

#include <interface/RHIEnums.hpp>
#include <interface/RHIHandles.hpp>
#include <interface/RHIStructs.hpp>


namespace StarryEngine::RHI {
    class IResource {
    public:
        virtual ~IResource() = default;

        virtual void release() = 0;
        virtual bool isValid() const = 0;
        virtual void* getNativeHandle() const = 0;
        virtual size_t getMemoryUsage() const = 0;

        virtual bool operator==(const IResource& other) const {
            return getNativeHandle() == other.getNativeHandle();
        }
    };

    class RHIBuffer : public IResource {
    public:
        virtual ~RHIBuffer() = default;

        virtual void* map(uint64_t offset = 0, uint64_t size = 0) = 0;
        virtual void unmap() = 0;

        virtual bool update(const void* data, uint64_t size, uint64_t offset = 0) = 0;
        virtual void flush(uint64_t offset = 0, uint64_t size = 0) = 0;
        virtual void invalidate(uint64_t offset = 0, uint64_t size = 0) = 0;

        virtual BufferType getType() const = 0;
        virtual uint64_t getSize() const = 0;
        virtual uint64_t getAlignment() const = 0;
        virtual bool isCPUVisible() const = 0;
        virtual bool isGPUOnly() const = 0;
        virtual bool isPersistentMapped() const = 0;

        virtual void* createView(Format format, uint64_t offset = 0, uint64_t size = 0) = 0;
        virtual void destroyView(void* view) = 0;
    };

    class RHITexture : public IResource {
    public:
        virtual ~RHITexture() = default;

        virtual TextureType getType() const = 0;
        virtual Format getFormat() const = 0;
        virtual Extent3D getExtent() const = 0;
        virtual uint32_t getMipLevels() const = 0;
        virtual uint32_t getArrayLayers() const = 0;
        virtual uint32_t getSampleCount() const = 0;
        virtual void* getImageHandle() const = 0;
        virtual void* getNativeHandleFromView(void* viewKey) = 0;

        virtual void* createView(const ImageSubresourceRange& range,ImageViewType viewType = ImageViewType::Auto) = 0;
        virtual void* getDefaultView() const = 0;
        virtual void* getSamplingView() const { return getDefaultView(); }
        virtual void destroyView(void* view) = 0;

        virtual void transitionLayout(
            ImageLayout newLayout,
            PipelineStage srcStage,
            PipelineStage dstStage,
            AccessFlag srcAccess,
            AccessFlag dstAccess,
            const ImageSubresourceRange& range) = 0;

        virtual void copyFromBuffer(RHIBuffer* buffer,const std::vector<BufferImageCopyRegion>& regions) = 0;
        virtual void copyToBuffer(RHIBuffer* buffer,const std::vector<BufferImageCopyRegion>& regions) = 0;
        virtual void copyFromTexture(RHITexture* texture,const std::vector<ImageCopyRegion>& regions) = 0;

        virtual void generateMipmaps() = 0;

        virtual void update(const void* data, size_t size, const ImageSubresourceRange& range) = 0;
    };

    class RHISampler : public IResource {
    public:
        virtual ~RHISampler() = default;

        virtual const SamplerDesc& getDesc() const = 0;
    };

    class RHIShaderModule : public IResource {
    public:
        virtual ~RHIShaderModule() = default;

        virtual ShaderStage getStage() const = 0;
        virtual const std::string& getEntryPoint() const = 0;

        virtual void release() = 0;

        virtual bool hasReflectionData() const = 0;
        virtual const void* getReflectionData() const = 0;

        virtual void setDefines(const std::vector<std::string>& defines) = 0;
        virtual void setIncludePaths(const std::vector<std::string>& includePaths) = 0;

        virtual bool recompile(const std::vector<uint8_t>& newBytecode) = 0;
    };

    class RHIPipelineLayout : public IResource {
    public:
        virtual ~RHIPipelineLayout() = default;

        virtual const PipelineLayoutDesc& getDesc() const = 0;

        virtual uint32_t getDescriptorSetCount() const = 0;

        virtual DescriptorSetLayoutHandle getLayoutHandle(uint32_t setIndex) const = 0;

        virtual uint32_t getPushConstantRangeCount() const = 0;

        virtual uint32_t getBindingPoint(uint32_t set, uint32_t binding) const = 0;
    };

    class RHIPipeline : public IResource {
    public:
        virtual ~RHIPipeline() = default;

        virtual PipelineType getType() const = 0;
        virtual PipelineLayoutHandle getLayout() const = 0;

        virtual bool isComputePipeline() const = 0;
        virtual bool isGraphicsPipeline() const = 0;
        virtual bool isRayTracingPipeline() const = 0;
        virtual bool canBeReloaded() const = 0;
        virtual bool reload(const void* newPipelineData) = 0;
    };

    class RHIRenderPass : public IResource {
    public:
        virtual ~RHIRenderPass() = default;

        virtual const RenderPassDesc& getDesc() const = 0;
        virtual uint32_t getAttachmentCount() const = 0;
        virtual uint32_t getSubpassCount() const = 0;

        virtual bool isCompatibleWith(const RHIRenderPass* other) const = 0;
    };

    class RHIFramebuffer : public IResource {
    public:
        virtual ~RHIFramebuffer() = default;

        virtual const FramebufferDesc& getDesc() const = 0;
        virtual Extent2D getExtent() const = 0;
        virtual uint32_t getLayerCount() const = 0;

        virtual uint32_t getAttachmentCount() const = 0;
    };

    class RHICommandBuffer : public IResource {
    public:
        virtual ~RHICommandBuffer() = default;

        virtual CommandBufferLevel getLevel() const = 0;
		virtual bool isOneTimeSubmit() const = 0;
		virtual bool isSimultaneousUse() const = 0;

        virtual void begin() = 0;
        virtual void end() = 0;
        virtual void reset(bool releaseResources = false) = 0;
    };

    class RHICommandPool : public IResource {
    public:
        virtual ~RHICommandPool() = default;
        virtual void reset(bool releaseResources = false) = 0;
        virtual QueueType getQueueType() const = 0;
    };

    class RHIDescriptorSetLayout : public IResource {
    public:
        virtual ~RHIDescriptorSetLayout() = default;

        virtual const std::vector<DescriptorSetLayoutBinding>& getBindings() const = 0;
        virtual uint32_t getBindingCount() const = 0;

        virtual bool isCompatibleWith(const RHIDescriptorSetLayout* other) const = 0;
    };

    class RHIDescriptorSet : public IResource {
    public:
        virtual ~RHIDescriptorSet() = default;

        virtual void writeBuffer(
            uint32_t binding,
            uint32_t arrayElement,
            RHIBuffer* buffer,
            uint64_t offset = 0,
            uint64_t range = 0) = 0;

        virtual void writeTexture(
            uint32_t binding,
            uint32_t arrayElement,
            RHITexture* texture,
            RHISampler* sampler = nullptr,
            ImageLayout layout = ImageLayout::ShaderReadOnly) = 0;

        virtual void writeSampler(
            uint32_t binding,
            uint32_t arrayElement,
            RHISampler* sampler) = 0;


        virtual void writeInputAttachment(
            uint32_t binding,
            uint32_t arrayElement,
            RHITexture* texture,
            ImageLayout layout) = 0;

        virtual void writeTextureCustomView(
            uint32_t binding, uint32_t arrayElement,
            void* imageView,       
            RHISampler* sampler,
            ImageLayout layout) = 0;

        virtual void update() = 0;
        virtual void copyFrom(const RHIDescriptorSet* src, const std::vector<DescriptorCopy>& copies) = 0;
    };


    class RHIDescriptorPool : public IResource {
    public:
        virtual ~RHIDescriptorPool() = default;

        virtual std::vector<std::unique_ptr<RHIDescriptorSet>> allocateDescriptorSets(const std::vector<RHIDescriptorSetLayout*>& layouts) = 0;

        virtual void reset() = 0;
        virtual uint32_t getMaxSets() const = 0;
        virtual uint32_t getRemainingSets() const = 0;
    };
}