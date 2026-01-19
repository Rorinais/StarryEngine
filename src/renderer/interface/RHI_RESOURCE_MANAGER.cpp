#include "RHI_RESOURCE_MANAGER.hpp"

namespace StarryEngine::RHI {

    // ==================== ResourceManager 构造函数/析构函数 ====================
    ResourceManager::ResourceManager(std::unique_ptr<IResourceFactory> factory)
        : factory_(std::move(factory)) {
        assert(factory_ != nullptr && "Resource factory must be provided");
        initStatistics();
    }

    ResourceManager::~ResourceManager() {
        clearAll();
    }

    // ==================== 基础资源创建方法 ====================
    BufferHandle ResourceManager::createBuffer(const BufferDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createBuffer(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create buffer: " << name << std::endl;
            }
            return BufferHandle::Null();
        }
        logResourceCreation(ResourceCategory::Buffer, name);
        return buffers_.create(std::move(resource), name, debugTag);
    }

    TextureHandle ResourceManager::createTexture(const TextureDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createTexture(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create texture: " << name << std::endl;
            }
            return TextureHandle::Null();
        }
        logResourceCreation(ResourceCategory::Texture, name);
        return textures_.create(std::move(resource), name, debugTag);
    }

    PipelineHandle ResourceManager::createGraphicsPipeline(const GraphicsPipelineDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createPipeline(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create graphics pipeline: " << name << std::endl;
            }
            return PipelineHandle::Null();
        }
        logResourceCreation(ResourceCategory::Pipeline, name);
        return pipelines_.create(std::move(resource), name, debugTag);
    }

    PipelineHandle ResourceManager::createComputePipeline(const ComputePipelineDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createComputePipeline(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create compute pipeline: " << name << std::endl;
            }
            return PipelineHandle::Null();
        }
        logResourceCreation(ResourceCategory::Pipeline, name);
        return pipelines_.create(std::move(resource), name, debugTag);
    }

    PipelineLayoutHandle ResourceManager::createPipelineLayout(const PipelineLayoutDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createPipelineLayout(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create pipeline layout: " << name << std::endl;
            }
            return PipelineLayoutHandle::Null();
        }
        logResourceCreation(ResourceCategory::PipelineLayout, name);
        return pipelineLayouts_.create(std::move(resource), name, debugTag);
    }

    ShaderHandle ResourceManager::createShader(const ShaderModuleDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createShader(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create shader: " << name << std::endl;
            }
            return ShaderHandle::Null();
        }
        logResourceCreation(ResourceCategory::Shader, name);
        return shaders_.create(std::move(resource), name, debugTag);
    }

    SamplerHandle ResourceManager::createSampler(const SamplerDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createSampler(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create sampler: " << name << std::endl;
            }
            return SamplerHandle::Null();
        }
        logResourceCreation(ResourceCategory::Sampler, name);
        return samplers_.create(std::move(resource), name, debugTag);
    }

    RenderPassHandle ResourceManager::createRenderPass(const RenderPassDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createRenderPass(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create render pass: " << name << std::endl;
            }
            return RenderPassHandle::Null();
        }
        logResourceCreation(ResourceCategory::RenderPass, name);
        return renderPasses_.create(std::move(resource), name, debugTag);
    }

    FramebufferHandle ResourceManager::createFramebuffer(const FramebufferDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createFramebuffer(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create framebuffer: " << name << std::endl;
            }
            return FramebufferHandle::Null();
        }
        logResourceCreation(ResourceCategory::Framebuffer, name);
        return framebuffers_.create(std::move(resource), name, debugTag);
    }

    // ==================== 其他资源创建方法 ====================
    DescriptorSetHandle ResourceManager::createDescriptorSet(const DescriptorSetDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createDescriptorSet(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create descriptor set: " << name << std::endl;
            }
            return DescriptorSetHandle::Null();
        }
        logResourceCreation(ResourceCategory::DescriptorSet, name);
        return descriptorSets_.create(std::move(resource), name, debugTag);
    }

    DescriptorPoolHandle ResourceManager::createDescriptorPool(const DescriptorPoolDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createDescriptorPool(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create descriptor pool: " << name << std::endl;
            }
            return DescriptorPoolHandle::Null();
        }
        logResourceCreation(ResourceCategory::DescriptorPool, name);
        return descriptorPools_.create(std::move(resource), name, debugTag);
    }

    DescriptorSetLayoutHandle ResourceManager::createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createDescriptorSetLayout(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create descriptor set layout: " << name << std::endl;
            }
            return DescriptorSetLayoutHandle::Null();
        }
        logResourceCreation(ResourceCategory::DescriptorSetLayout, name);
        return descriptorSetLayouts_.create(std::move(resource), name, debugTag);
    }

    CommandBufferHandle ResourceManager::createCommandBuffer(const CommandBufferDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createCommandBuffer(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create command buffer: " << name << std::endl;
            }
            return CommandBufferHandle::Null();
        }
        logResourceCreation(ResourceCategory::CommandBuffer, name);
        return commandBuffers_.create(std::move(resource), name, debugTag);
    }

    CommandPoolHandle ResourceManager::createCommandPool(const CommandPoolDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createCommandPool(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create command pool: " << name << std::endl;
            }
            return CommandPoolHandle::Null();
        }
        logResourceCreation(ResourceCategory::CommandPool, name);
        return commandPools_.create(std::move(resource), name, debugTag);
    }

    FenceHandle ResourceManager::createFence(const FenceDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createFence(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create fence: " << name << std::endl;
            }
            return FenceHandle::Null();
        }
        logResourceCreation(ResourceCategory::Fence, name);
        return fences_.create(std::move(resource), name, debugTag);
    }

    SemaphoreHandle ResourceManager::createSemaphore(const SemaphoreDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createSemaphore(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create semaphore: " << name << std::endl;
            }
            return SemaphoreHandle::Null();
        }
        logResourceCreation(ResourceCategory::Semaphore, name);
        return semaphores_.create(std::move(resource), name, debugTag);
    }

    EventHandle ResourceManager::createEvent(const EventDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createEvent(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create event: " << name << std::endl;
            }
            return EventHandle::Null();
        }
        logResourceCreation(ResourceCategory::Event, name);
        return events_.create(std::move(resource), name, debugTag);
    }

    QueryPoolHandle ResourceManager::createQueryPool(const QueryPoolDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createQueryPool(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create query pool: " << name << std::endl;
            }
            return QueryPoolHandle::Null();
        }
        logResourceCreation(ResourceCategory::QueryPool, name);
        return queryPools_.create(std::move(resource), name, debugTag);
    }

    AccelerationStructureHandle ResourceManager::createAccelerationStructure(const AccelerationStructureDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createAccelerationStructure(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create acceleration structure: " << name << std::endl;
            }
            return AccelerationStructureHandle::Null();
        }
        logResourceCreation(ResourceCategory::AccelerationStructure, name);
        return accelerationStructures_.create(std::move(resource), name, debugTag);
    }

    SwapChainHandle ResourceManager::createSwapChain(const SwapChainDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createSwapChain(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create swap chain: " << name << std::endl;
            }
            return SwapChainHandle::Null();
        }
        logResourceCreation(ResourceCategory::SwapChain, name);
        return swapChains_.create(std::move(resource), name, debugTag);
    }

    QueueHandle ResourceManager::createQueue(const QueueDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto resource = factory_->createQueue(desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create queue: " << name << std::endl;
            }
            return QueueHandle::Null();
        }
        logResourceCreation(ResourceCategory::Queue, name);
        return queues_.create(std::move(resource), name, debugTag);
    }

    // ==================== 批量创建方法 ====================
    std::vector<CommandBufferHandle> ResourceManager::createCommandBuffers(uint32_t count,
        const CommandBufferDesc& desc,
        const std::string& baseName) {
        std::vector<CommandBufferHandle> handles;
        handles.reserve(count);

        for (uint32_t i = 0; i < count; ++i) {
            std::string name = baseName.empty() ? "" : baseName + "_" + std::to_string(i);
            auto handle = createCommandBuffer(desc, name);
            if (handle.isValid()) {
                handles.push_back(handle);
            }
            else {
                // 如果创建失败，释放已创建的资源
                for (auto& h : handles) {
                    destroy(h);
                }
                return {};
            }
        }

        return handles;
    }

    std::vector<DescriptorSetHandle> ResourceManager::createDescriptorSets(uint32_t count,
        const DescriptorSetDesc& desc,
        const std::string& baseName) {
        std::vector<DescriptorSetHandle> handles;
        handles.reserve(count);

        for (uint32_t i = 0; i < count; ++i) {
            std::string name = baseName.empty() ? "" : baseName + "_" + std::to_string(i);
            auto handle = createDescriptorSet(desc, name);
            if (handle.isValid()) {
                handles.push_back(handle);
            }
            else {
                // 如果创建失败，释放已创建的资源
                for (auto& h : handles) {
                    destroy(h);
                }
                return {};
            }
        }

        return handles;
    }

    // ==================== 资源获取方法 ====================
    RHIBuffer* ResourceManager::getBuffer(BufferHandle handle) {
        return buffers_.getData(handle);
    }

    const RHIBuffer* ResourceManager::getBuffer(BufferHandle handle) const {
        return buffers_.getData(handle);
    }

    RHITexture* ResourceManager::getTexture(TextureHandle handle) {
        return textures_.getData(handle);
    }

    const RHITexture* ResourceManager::getTexture(TextureHandle handle) const {
        return textures_.getData(handle);
    }

    RHIPipeline* ResourceManager::getPipeline(PipelineHandle handle) {
        return pipelines_.getData(handle);
    }

    const RHIPipeline* ResourceManager::getPipeline(PipelineHandle handle) const {
        return pipelines_.getData(handle);
    }

    RHIPipelineLayout* ResourceManager::getPipelineLayout(PipelineLayoutHandle handle) {
        return pipelineLayouts_.getData(handle);
    }

    const RHIPipelineLayout* ResourceManager::getPipelineLayout(PipelineLayoutHandle handle) const {
        return pipelineLayouts_.getData(handle);
    }

    RHIShaderModule* ResourceManager::getShader(ShaderHandle handle) {
        return shaders_.getData(handle);
    }

    const RHIShaderModule* ResourceManager::getShader(ShaderHandle handle) const {
        return shaders_.getData(handle);
    }

    RHISampler* ResourceManager::getSampler(SamplerHandle handle) {
        return samplers_.getData(handle);
    }

    const RHISampler* ResourceManager::getSampler(SamplerHandle handle) const {
        return samplers_.getData(handle);
    }

    RHIRenderPass* ResourceManager::getRenderPass(RenderPassHandle handle) {
        return renderPasses_.getData(handle);
    }

    const RHIRenderPass* ResourceManager::getRenderPass(RenderPassHandle handle) const {
        return renderPasses_.getData(handle);
    }

    RHIFramebuffer* ResourceManager::getFramebuffer(FramebufferHandle handle) {
        return framebuffers_.getData(handle);
    }

    const RHIFramebuffer* ResourceManager::getFramebuffer(FramebufferHandle handle) const {
        return framebuffers_.getData(handle);
    }

    RHIDescriptorSet* ResourceManager::getDescriptorSet(DescriptorSetHandle handle) {
        return descriptorSets_.getData(handle);
    }

    const RHIDescriptorSet* ResourceManager::getDescriptorSet(DescriptorSetHandle handle) const {
        return descriptorSets_.getData(handle);
    }

    RHIDescriptorPool* ResourceManager::getDescriptorPool(DescriptorPoolHandle handle) {
        return descriptorPools_.getData(handle);
    }

    const RHIDescriptorPool* ResourceManager::getDescriptorPool(DescriptorPoolHandle handle) const {
        return descriptorPools_.getData(handle);
    }

    RHIDescriptorSetLayout* ResourceManager::getDescriptorSetLayout(DescriptorSetLayoutHandle handle) {
        return descriptorSetLayouts_.getData(handle);
    }

    const RHIDescriptorSetLayout* ResourceManager::getDescriptorSetLayout(DescriptorSetLayoutHandle handle) const {
        return descriptorSetLayouts_.getData(handle);
    }

    RHICommandBuffer* ResourceManager::getCommandBuffer(CommandBufferHandle handle) {
        return commandBuffers_.getData(handle);
    }

    const RHICommandBuffer* ResourceManager::getCommandBuffer(CommandBufferHandle handle) const {
        return commandBuffers_.getData(handle);
    }

    RHICommandPool* ResourceManager::getCommandPool(CommandPoolHandle handle) {
        return commandPools_.getData(handle);
    }

    const RHICommandPool* ResourceManager::getCommandPool(CommandPoolHandle handle) const {
        return commandPools_.getData(handle);
    }

    RHIFence* ResourceManager::getFence(FenceHandle handle) {
        return fences_.getData(handle);
    }

    const RHIFence* ResourceManager::getFence(FenceHandle handle) const {
        return fences_.getData(handle);
    }

    RHISemaphore* ResourceManager::getSemaphore(SemaphoreHandle handle) {
        return semaphores_.getData(handle);
    }

    const RHISemaphore* ResourceManager::getSemaphore(SemaphoreHandle handle) const {
        return semaphores_.getData(handle);
    }

    RHIEvent* ResourceManager::getEvent(EventHandle handle) {
        return events_.getData(handle);
    }

    const RHIEvent* ResourceManager::getEvent(EventHandle handle) const {
        return events_.getData(handle);
    }

    RHIQueryPool* ResourceManager::getQueryPool(QueryPoolHandle handle) {
        return queryPools_.getData(handle);
    }

    const RHIQueryPool* ResourceManager::getQueryPool(QueryPoolHandle handle) const {
        return queryPools_.getData(handle);
    }

    RHIAccelerationStructure* ResourceManager::getAccelerationStructure(AccelerationStructureHandle handle) {
        return accelerationStructures_.getData(handle);
    }

    const RHIAccelerationStructure* ResourceManager::getAccelerationStructure(AccelerationStructureHandle handle) const {
        return accelerationStructures_.getData(handle);
    }

    RHISwapChain* ResourceManager::getSwapChain(SwapChainHandle handle) {
        return swapChains_.getData(handle);
    }

    const RHISwapChain* ResourceManager::getSwapChain(SwapChainHandle handle) const {
        return swapChains_.getData(handle);
    }

    RHIQueue* ResourceManager::getQueue(QueueHandle handle) {
        return queues_.getData(handle);
    }

    const RHIQueue* ResourceManager::getQueue(QueueHandle handle) const {
        return queues_.getData(handle);
    }

    // ==================== 按名称查找方法 ====================
    BufferHandle ResourceManager::findBufferByName(const std::string& name) {
        return buffers_.findByName(name);
    }

    TextureHandle ResourceManager::findTextureByName(const std::string& name) {
        return textures_.findByName(name);
    }

    PipelineHandle ResourceManager::findPipelineByName(const std::string& name) {
        return pipelines_.findByName(name);
    }

    ShaderHandle ResourceManager::findShaderByName(const std::string& name) {
        return shaders_.findByName(name);
    }

    SamplerHandle ResourceManager::findSamplerByName(const std::string& name) {
        return samplers_.findByName(name);
    }

    RenderPassHandle ResourceManager::findRenderPassByName(const std::string& name) {
        return renderPasses_.findByName(name);
    }

    FramebufferHandle ResourceManager::findFramebufferByName(const std::string& name) {
        return framebuffers_.findByName(name);
    }

    DescriptorSetHandle ResourceManager::findDescriptorSetByName(const std::string& name) {
        return descriptorSets_.findByName(name);
    }

    DescriptorPoolHandle ResourceManager::findDescriptorPoolByName(const std::string& name) {
        return descriptorPools_.findByName(name);
    }

    DescriptorSetLayoutHandle ResourceManager::findDescriptorSetLayoutByName(const std::string& name) {
        return descriptorSetLayouts_.findByName(name);
    }

    CommandBufferHandle ResourceManager::findCommandBufferByName(const std::string& name) {
        return commandBuffers_.findByName(name);
    }

    CommandPoolHandle ResourceManager::findCommandPoolByName(const std::string& name) {
        return commandPools_.findByName(name);
    }

    FenceHandle ResourceManager::findFenceByName(const std::string& name) {
        return fences_.findByName(name);
    }

    SemaphoreHandle ResourceManager::findSemaphoreByName(const std::string& name) {
        return semaphores_.findByName(name);
    }

    EventHandle ResourceManager::findEventByName(const std::string& name) {
        return events_.findByName(name);
    }

    QueryPoolHandle ResourceManager::findQueryPoolByName(const std::string& name) {
        return queryPools_.findByName(name);
    }

    AccelerationStructureHandle ResourceManager::findAccelerationStructureByName(const std::string& name) {
        return accelerationStructures_.findByName(name);
    }

    SwapChainHandle ResourceManager::findSwapChainByName(const std::string& name) {
        return swapChains_.findByName(name);
    }

    QueueHandle ResourceManager::findQueueByName(const std::string& name) {
        return queues_.findByName(name);
    }

    // ==================== 按调试标签查找方法 ====================
    BufferHandle ResourceManager::findBufferByDebugTag(const std::string& debugTag) {
        return buffers_.findByDebugTag(debugTag);
    }

    TextureHandle ResourceManager::findTextureByDebugTag(const std::string& debugTag) {
        return textures_.findByDebugTag(debugTag);
    }

    PipelineHandle ResourceManager::findPipelineByDebugTag(const std::string& debugTag) {
        return pipelines_.findByDebugTag(debugTag);
    }

    ShaderHandle ResourceManager::findShaderByDebugTag(const std::string& debugTag) {
        return shaders_.findByDebugTag(debugTag);
    }

    SamplerHandle ResourceManager::findSamplerByDebugTag(const std::string& debugTag) {
        return samplers_.findByDebugTag(debugTag);
    }

    RenderPassHandle ResourceManager::findRenderPassByDebugTag(const std::string& debugTag) {
        return renderPasses_.findByDebugTag(debugTag);
    }

    FramebufferHandle ResourceManager::findFramebufferByDebugTag(const std::string& debugTag) {
        return framebuffers_.findByDebugTag(debugTag);
    }

    DescriptorSetHandle ResourceManager::findDescriptorSetByDebugTag(const std::string& debugTag) {
        return descriptorSets_.findByDebugTag(debugTag);
    }

    DescriptorPoolHandle ResourceManager::findDescriptorPoolByDebugTag(const std::string& debugTag) {
        return descriptorPools_.findByDebugTag(debugTag);
    }

    DescriptorSetLayoutHandle ResourceManager::findDescriptorSetLayoutByDebugTag(const std::string& debugTag) {
        return descriptorSetLayouts_.findByDebugTag(debugTag);
    }

    CommandBufferHandle ResourceManager::findCommandBufferByDebugTag(const std::string& debugTag) {
        return commandBuffers_.findByDebugTag(debugTag);
    }

    CommandPoolHandle ResourceManager::findCommandPoolByDebugTag(const std::string& debugTag) {
        return commandPools_.findByDebugTag(debugTag);
    }

    FenceHandle ResourceManager::findFenceByDebugTag(const std::string& debugTag) {
        return fences_.findByDebugTag(debugTag);
    }

    SemaphoreHandle ResourceManager::findSemaphoreByDebugTag(const std::string& debugTag) {
        return semaphores_.findByDebugTag(debugTag);
    }

    EventHandle ResourceManager::findEventByDebugTag(const std::string& debugTag) {
        return events_.findByDebugTag(debugTag);
    }

    QueryPoolHandle ResourceManager::findQueryPoolByDebugTag(const std::string& debugTag) {
        return queryPools_.findByDebugTag(debugTag);
    }

    AccelerationStructureHandle ResourceManager::findAccelerationStructureByDebugTag(const std::string& debugTag) {
        return accelerationStructures_.findByDebugTag(debugTag);
    }

    SwapChainHandle ResourceManager::findSwapChainByDebugTag(const std::string& debugTag) {
        return swapChains_.findByDebugTag(debugTag);
    }

    QueueHandle ResourceManager::findQueueByDebugTag(const std::string& debugTag) {
        return queues_.findByDebugTag(debugTag);
    }

    // ==================== 资源引用计数管理 ====================
    bool ResourceManager::addRef(BufferHandle handle) {
        return buffers_.addRef(handle);
    }

    bool ResourceManager::release(BufferHandle handle) {
        return buffers_.release(handle);
    }

    bool ResourceManager::destroy(BufferHandle handle) {
        return buffers_.destroy(handle);
    }

    bool ResourceManager::addRef(TextureHandle handle) {
        return textures_.addRef(handle);
    }

    bool ResourceManager::release(TextureHandle handle) {
        return textures_.release(handle);
    }

    bool ResourceManager::destroy(TextureHandle handle) {
        return textures_.destroy(handle);
    }

    bool ResourceManager::addRef(PipelineHandle handle) {
        return pipelines_.addRef(handle);
    }

    bool ResourceManager::release(PipelineHandle handle) {
        return pipelines_.release(handle);
    }

    bool ResourceManager::destroy(PipelineHandle handle) {
        return pipelines_.destroy(handle);
    }

    bool ResourceManager::addRef(PipelineLayoutHandle handle) {
        return pipelineLayouts_.addRef(handle);
    }

    bool ResourceManager::release(PipelineLayoutHandle handle) {
        return pipelineLayouts_.release(handle);
    }

    bool ResourceManager::destroy(PipelineLayoutHandle handle) {
        return pipelineLayouts_.destroy(handle);
    }

    bool ResourceManager::addRef(ShaderHandle handle) {
        return shaders_.addRef(handle);
    }

    bool ResourceManager::release(ShaderHandle handle) {
        return shaders_.release(handle);
    }

    bool ResourceManager::destroy(ShaderHandle handle) {
        return shaders_.destroy(handle);
    }

    bool ResourceManager::addRef(SamplerHandle handle) {
        return samplers_.addRef(handle);
    }

    bool ResourceManager::release(SamplerHandle handle) {
        return samplers_.release(handle);
    }

    bool ResourceManager::destroy(SamplerHandle handle) {
        return samplers_.destroy(handle);
    }

    bool ResourceManager::addRef(RenderPassHandle handle) {
        return renderPasses_.addRef(handle);
    }

    bool ResourceManager::release(RenderPassHandle handle) {
        return renderPasses_.release(handle);
    }

    bool ResourceManager::destroy(RenderPassHandle handle) {
        return renderPasses_.destroy(handle);
    }

    bool ResourceManager::addRef(FramebufferHandle handle) {
        return framebuffers_.addRef(handle);
    }

    bool ResourceManager::release(FramebufferHandle handle) {
        return framebuffers_.release(handle);
    }

    bool ResourceManager::destroy(FramebufferHandle handle) {
        return framebuffers_.destroy(handle);
    }

    bool ResourceManager::addRef(DescriptorSetHandle handle) {
        return descriptorSets_.addRef(handle);
    }

    bool ResourceManager::release(DescriptorSetHandle handle) {
        return descriptorSets_.release(handle);
    }

    bool ResourceManager::destroy(DescriptorSetHandle handle) {
        return descriptorSets_.destroy(handle);
    }

    bool ResourceManager::addRef(DescriptorPoolHandle handle) {
        return descriptorPools_.addRef(handle);
    }

    bool ResourceManager::release(DescriptorPoolHandle handle) {
        return descriptorPools_.release(handle);
    }

    bool ResourceManager::destroy(DescriptorPoolHandle handle) {
        return descriptorPools_.destroy(handle);
    }

    bool ResourceManager::addRef(DescriptorSetLayoutHandle handle) {
        return descriptorSetLayouts_.addRef(handle);
    }

    bool ResourceManager::release(DescriptorSetLayoutHandle handle) {
        return descriptorSetLayouts_.release(handle);
    }

    bool ResourceManager::destroy(DescriptorSetLayoutHandle handle) {
        return descriptorSetLayouts_.destroy(handle);
    }

    bool ResourceManager::addRef(CommandBufferHandle handle) {
        return commandBuffers_.addRef(handle);
    }

    bool ResourceManager::release(CommandBufferHandle handle) {
        return commandBuffers_.release(handle);
    }

    bool ResourceManager::destroy(CommandBufferHandle handle) {
        return commandBuffers_.destroy(handle);
    }

    bool ResourceManager::addRef(CommandPoolHandle handle) {
        return commandPools_.addRef(handle);
    }

    bool ResourceManager::release(CommandPoolHandle handle) {
        return commandPools_.release(handle);
    }

    bool ResourceManager::destroy(CommandPoolHandle handle) {
        return commandPools_.destroy(handle);
    }

    bool ResourceManager::addRef(FenceHandle handle) {
        return fences_.addRef(handle);
    }

    bool ResourceManager::release(FenceHandle handle) {
        return fences_.release(handle);
    }

    bool ResourceManager::destroy(FenceHandle handle) {
        return fences_.destroy(handle);
    }

    bool ResourceManager::addRef(SemaphoreHandle handle) {
        return semaphores_.addRef(handle);
    }

    bool ResourceManager::release(SemaphoreHandle handle) {
        return semaphores_.release(handle);
    }

    bool ResourceManager::destroy(SemaphoreHandle handle) {
        return semaphores_.destroy(handle);
    }

    bool ResourceManager::addRef(EventHandle handle) {
        return events_.addRef(handle);
    }

    bool ResourceManager::release(EventHandle handle) {
        return events_.release(handle);
    }

    bool ResourceManager::destroy(EventHandle handle) {
        return events_.destroy(handle);
    }

    bool ResourceManager::addRef(QueryPoolHandle handle) {
        return queryPools_.addRef(handle);
    }

    bool ResourceManager::release(QueryPoolHandle handle) {
        return queryPools_.release(handle);
    }

    bool ResourceManager::destroy(QueryPoolHandle handle) {
        return queryPools_.destroy(handle);
    }

    bool ResourceManager::addRef(AccelerationStructureHandle handle) {
        return accelerationStructures_.addRef(handle);
    }

    bool ResourceManager::release(AccelerationStructureHandle handle) {
        return accelerationStructures_.release(handle);
    }

    bool ResourceManager::destroy(AccelerationStructureHandle handle) {
        return accelerationStructures_.destroy(handle);
    }

    bool ResourceManager::addRef(SwapChainHandle handle) {
        return swapChains_.addRef(handle);
    }

    bool ResourceManager::release(SwapChainHandle handle) {
        return swapChains_.release(handle);
    }

    bool ResourceManager::destroy(SwapChainHandle handle) {
        return swapChains_.destroy(handle);
    }

    bool ResourceManager::addRef(QueueHandle handle) {
        return queues_.addRef(handle);
    }

    bool ResourceManager::release(QueueHandle handle) {
        return queues_.release(handle);
    }

    bool ResourceManager::destroy(QueueHandle handle) {
        return queues_.destroy(handle);
    }

    // ==================== 资源存储访问 ====================
    TypedResourceStorage<BufferHandle, RHIBuffer>& ResourceManager::getBufferStorage() {
        return buffers_;
    }

    TypedResourceStorage<TextureHandle, RHITexture>& ResourceManager::getTextureStorage() {
        return textures_;
    }

    TypedResourceStorage<PipelineHandle, RHIPipeline>& ResourceManager::getPipelineStorage() {
        return pipelines_;
    }

    TypedResourceStorage<PipelineLayoutHandle, RHIPipelineLayout>& ResourceManager::getPipelineLayoutStorage() {
        return pipelineLayouts_;
    }

    TypedResourceStorage<ShaderHandle, RHIShaderModule>& ResourceManager::getShaderStorage() {
        return shaders_;
    }

    TypedResourceStorage<SamplerHandle, RHISampler>& ResourceManager::getSamplerStorage() {
        return samplers_;
    }

    TypedResourceStorage<RenderPassHandle, RHIRenderPass>& ResourceManager::getRenderPassStorage() {
        return renderPasses_;
    }

    TypedResourceStorage<FramebufferHandle, RHIFramebuffer>& ResourceManager::getFramebufferStorage() {
        return framebuffers_;
    }

    TypedResourceStorage<DescriptorSetHandle, RHIDescriptorSet>& ResourceManager::getDescriptorSetStorage() {
        return descriptorSets_;
    }

    TypedResourceStorage<DescriptorPoolHandle, RHIDescriptorPool>& ResourceManager::getDescriptorPoolStorage() {
        return descriptorPools_;
    }

    TypedResourceStorage<DescriptorSetLayoutHandle, RHIDescriptorSetLayout>& ResourceManager::getDescriptorSetLayoutStorage() {
        return descriptorSetLayouts_;
    }

    TypedResourceStorage<CommandBufferHandle, RHICommandBuffer>& ResourceManager::getCommandBufferStorage() {
        return commandBuffers_;
    }

    TypedResourceStorage<CommandPoolHandle, RHICommandPool>& ResourceManager::getCommandPoolStorage() {
        return commandPools_;
    }

    TypedResourceStorage<FenceHandle, RHIFence>& ResourceManager::getFenceStorage() {
        return fences_;
    }

    TypedResourceStorage<SemaphoreHandle, RHISemaphore>& ResourceManager::getSemaphoreStorage() {
        return semaphores_;
    }

    TypedResourceStorage<EventHandle, RHIEvent>& ResourceManager::getEventStorage() {
        return events_;
    }

    TypedResourceStorage<QueryPoolHandle, RHIQueryPool>& ResourceManager::getQueryPoolStorage() {
        return queryPools_;
    }

    TypedResourceStorage<AccelerationStructureHandle, RHIAccelerationStructure>& ResourceManager::getAccelerationStructureStorage() {
        return accelerationStructures_;
    }

    TypedResourceStorage<SwapChainHandle, RHISwapChain>& ResourceManager::getSwapChainStorage() {
        return swapChains_;
    }

    TypedResourceStorage<QueueHandle, RHIQueue>& ResourceManager::getQueueStorage() {
        return queues_;
    }

    // ==================== 管理功能 ====================
    void ResourceManager::clearAll() {
        buffers_.clear();
        textures_.clear();
        pipelines_.clear();
        pipelineLayouts_.clear();
        shaders_.clear();
        samplers_.clear();
        renderPasses_.clear();
        framebuffers_.clear();

        descriptorSets_.clear();
        descriptorPools_.clear();
        descriptorSetLayouts_.clear();
        commandBuffers_.clear();
        commandPools_.clear();
        fences_.clear();
        semaphores_.clear();
        events_.clear();
        queryPools_.clear();
        accelerationStructures_.clear();
        swapChains_.clear();
        queues_.clear();
    }

    // ==================== ResourceManager::getStatistics() 方法修复 ====================
    ResourceManager::Statistics ResourceManager::getStatistics() const {
        std::lock_guard<std::mutex> lock(statsMutex_);

        Statistics stats = stats_;
        stats.bufferCount = buffers_.size();
        stats.textureCount = textures_.size();
        stats.pipelineCount = pipelines_.size();
        stats.pipelineLayoutCount = pipelineLayouts_.size();
        stats.shaderCount = shaders_.size();
        stats.samplerCount = samplers_.size();
        stats.renderPassCount = renderPasses_.size();
        stats.framebufferCount = framebuffers_.size();
        stats.descriptorSetCount = descriptorSets_.size();
        stats.descriptorPoolCount = descriptorPools_.size();
        stats.descriptorSetLayoutCount = descriptorSetLayouts_.size();
        stats.commandBufferCount = commandBuffers_.size();
        stats.commandPoolCount = commandPools_.size();
        stats.fenceCount = fences_.size();
        stats.semaphoreCount = semaphores_.size();
        stats.eventCount = events_.size();
        stats.queryPoolCount = queryPools_.size();
        stats.accelerationStructureCount = accelerationStructures_.size();
        stats.swapChainCount = swapChains_.size();
        stats.queueCount = queues_.size();

        stats.totalResources = stats.bufferCount + stats.textureCount + stats.pipelineCount +
            stats.pipelineLayoutCount + stats.shaderCount + stats.samplerCount +
            stats.renderPassCount + stats.framebufferCount + stats.descriptorSetCount +
            stats.descriptorPoolCount + stats.descriptorSetLayoutCount + stats.commandBufferCount +
            stats.commandPoolCount + stats.fenceCount + stats.semaphoreCount + stats.eventCount +
            stats.queryPoolCount + stats.accelerationStructureCount + stats.swapChainCount +
            stats.queueCount;

        stats.bufferMemoryUsage = buffers_.getTotalMemoryUsage();
        stats.textureMemoryUsage = textures_.getTotalMemoryUsage();
        stats.accelerationStructureMemoryUsage = accelerationStructures_.getTotalMemoryUsage();
        stats.totalMemoryUsage = stats.bufferMemoryUsage + stats.textureMemoryUsage +
            stats.accelerationStructureMemoryUsage;

        // 修复原子变量的比较和赋值
        size_t currentPeakResourceCount = peakResourceCount_.load(std::memory_order_relaxed);
        size_t currentPeakMemoryUsage = peakMemoryUsage_.load(std::memory_order_relaxed);

        if (stats.totalResources > currentPeakResourceCount) {
            peakResourceCount_.store(stats.totalResources, std::memory_order_relaxed);
            currentPeakResourceCount = stats.totalResources;
        }
        if (stats.totalMemoryUsage > currentPeakMemoryUsage) {
            peakMemoryUsage_.store(stats.totalMemoryUsage, std::memory_order_relaxed);
            currentPeakMemoryUsage = stats.totalMemoryUsage;
        }

        stats.peakResourceCount = currentPeakResourceCount;
        stats.peakMemoryUsage = currentPeakMemoryUsage;

        return stats;
    }

    void ResourceManager::dumpStatistics() const {
        Statistics stats = getStatistics();

        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - startTime_);

        std::cout << "\n=== RHI Resource Manager Statistics ===" << std::endl;
        std::cout << "Uptime: " << uptime.count() << " seconds" << std::endl;
        std::cout << "Total Resources: " << stats.totalResources << std::endl;
        std::cout << "Peak Resources: " << stats.peakResourceCount << std::endl;
        std::cout << "\n--- Resource Breakdown ---" << std::endl;
        std::cout << "Buffers: " << stats.bufferCount << " ("
            << stats.bufferMemoryUsage / 1024.0 / 1024.0 << " MB)" << std::endl;
        std::cout << "Textures: " << stats.textureCount << " ("
            << stats.textureMemoryUsage / 1024.0 / 1024.0 << " MB)" << std::endl;
        std::cout << "Pipelines: " << stats.pipelineCount << std::endl;
        std::cout << "Pipeline Layouts: " << stats.pipelineLayoutCount << std::endl;
        std::cout << "Shaders: " << stats.shaderCount << std::endl;
        std::cout << "Samplers: " << stats.samplerCount << std::endl;
        std::cout << "Render Passes: " << stats.renderPassCount << std::endl;
        std::cout << "Framebuffers: " << stats.framebufferCount << std::endl;
        std::cout << "Descriptor Sets: " << stats.descriptorSetCount << std::endl;
        std::cout << "Descriptor Pools: " << stats.descriptorPoolCount << std::endl;
        std::cout << "Descriptor Set Layouts: " << stats.descriptorSetLayoutCount << std::endl;
        std::cout << "Command Buffers: " << stats.commandBufferCount << std::endl;
        std::cout << "Command Pools: " << stats.commandPoolCount << std::endl;
        std::cout << "Acceleration Structures: " << stats.accelerationStructureCount << " ("
            << stats.accelerationStructureMemoryUsage / 1024.0 / 1024.0 << " MB)" << std::endl;
        std::cout << "Swap Chains: " << stats.swapChainCount << std::endl;
        std::cout << "Queues: " << stats.queueCount << std::endl;
        std::cout << "\n--- Memory Usage ---" << std::endl;
        std::cout << "Total Memory: " << stats.totalMemoryUsage / 1024.0 / 1024.0 << " MB" << std::endl;
        std::cout << "Peak Memory: " << stats.peakMemoryUsage / 1024.0 / 1024.0 << " MB" << std::endl;
        std::cout << "====================================\n" << std::endl;
    }

    std::string ResourceManager::getDetailedReport() const {
        Statistics stats = getStatistics();
        std::string report;

        report += "=== RHI Resource Manager Detailed Report ===\n";
        report += "Total Resources: " + std::to_string(stats.totalResources) + "\n";
        report += "Peak Resources: " + std::to_string(stats.peakResourceCount) + "\n";
        report += "Total Memory Usage: " + std::to_string(stats.totalMemoryUsage / (1024 * 1024)) + " MB\n";
        report += "Peak Memory Usage: " + std::to_string(stats.peakMemoryUsage / (1024 * 1024)) + " MB\n";
        report += "\nResource Details:\n";

        // 添加各种资源的详细统计
        report += "Buffers: " + std::to_string(stats.bufferCount) + "\n";
        report += "Textures: " + std::to_string(stats.textureCount) + "\n";
        report += "Pipelines: " + std::to_string(stats.pipelineCount) + "\n";
        report += "Shaders: " + std::to_string(stats.shaderCount) + "\n";
        report += "Descriptor Sets: " + std::to_string(stats.descriptorSetCount) + "\n";

        return report;
    }

    void ResourceManager::setMemoryWarningThreshold(size_t threshold) {
        buffers_.setMemoryWarningThreshold(threshold);
        textures_.setMemoryWarningThreshold(threshold);
        accelerationStructures_.setMemoryWarningThreshold(threshold);
    }

    bool ResourceManager::checkMemoryUsage() const {
        return buffers_.isMemoryUsageExceeded() ||
            textures_.isMemoryUsageExceeded() ||
            accelerationStructures_.isMemoryUsageExceeded();
    }

    size_t ResourceManager::tryReleaseUnusedResources() {
        size_t releasedCount = 0;

        // 为每种资源类型尝试释放未使用的资源
        auto tryReleaseForStorage = [&releasedCount](auto& storage) {
            auto oldest = storage.getOldestResource();
            if (oldest) {
                if (storage.destroy(*oldest)) {
                    releasedCount++;
                }
            }
            };

        tryReleaseForStorage(buffers_);
        tryReleaseForStorage(textures_);
        tryReleaseForStorage(pipelines_);
        tryReleaseForStorage(shaders_);
        tryReleaseForStorage(descriptorSets_);

        return releasedCount;
    }

    IResourceFactory* ResourceManager::getFactory() const {
        return factory_.get();
    }

    void ResourceManager::setDebugMode(bool enabled) {
        debugMode_ = enabled;
    }

    bool ResourceManager::isDebugMode() const {
        return debugMode_;
    }

    bool ResourceManager::validateResources() const {
        bool allValid = true;

        // 验证所有资源是否有效
        auto validateStorage = [&allValid](const auto& storage, const std::string& typeName) {
            storage.forEach([&allValid, &typeName](auto handle, const auto& resource) {
                if (!resource.isValid()) {
                    std::cerr << "[ResourceManager] Invalid resource found: "
                        << typeName << " handle: " << handle.toString() << std::endl;
                    allValid = false;
                }
                });
            };

        validateStorage(buffers_, "Buffer");
        validateStorage(textures_, "Texture");
        validateStorage(pipelines_, "Pipeline");
        validateStorage(shaders_, "Shader");
        validateStorage(descriptorSets_, "DescriptorSet");
        validateStorage(descriptorPools_, "DescriptorPool");
        validateStorage(commandBuffers_, "CommandBuffer");
        validateStorage(commandPools_, "CommandPool");
        validateStorage(fences_, "Fence");
        validateStorage(semaphores_, "Semaphore");
        validateStorage(events_, "Event");
        validateStorage(queryPools_, "QueryPool");
        validateStorage(accelerationStructures_, "AccelerationStructure");
        validateStorage(swapChains_, "SwapChain");
        validateStorage(queues_, "Queue");

        return allValid;
    }

    std::vector<std::string> ResourceManager::getResourceLeakReport() const {
        std::vector<std::string> leaks;

        auto checkLeaksForStorage = [&leaks](const auto& storage, const std::string& typeName) {
            auto stats = storage.getStatistics();
            if (stats.currentCount > 0) {
                leaks.push_back(typeName + ": " + std::to_string(stats.currentCount) + " resources");
            }
            };

        checkLeaksForStorage(buffers_, "Buffers");
        checkLeaksForStorage(textures_, "Textures");
        checkLeaksForStorage(pipelines_, "Pipelines");
        checkLeaksForStorage(shaders_, "Shaders");
        checkLeaksForStorage(descriptorSets_, "DescriptorSets");

        return leaks;
    }

    // ==================== 私有方法实现 ====================
    void ResourceManager::initStatistics() {
        startTime_ = std::chrono::steady_clock::now();
        stats_ = Statistics{};
    }

    void ResourceManager::updateStatistics() {
        // 更新统计信息
        auto stats = getStatistics();
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_ = stats;
    }

    void ResourceManager::logResourceCreation(ResourceCategory category, const std::string& name) {
        if (!debugMode_) return;

        std::lock_guard<std::mutex> lock(statsMutex_);
        std::cout << "[ResourceManager] Created resource: Category="
            << static_cast<int>(category)
            << ", Name='" << name << "'"
            << std::endl;
    }

    void ResourceManager::logResourceDestruction(ResourceCategory category, const std::string& name) {
        if (!debugMode_) return;

        std::lock_guard<std::mutex> lock(statsMutex_);
        std::cout << "[ResourceManager] Destroyed resource: Category="
            << static_cast<int>(category)
            << ", Name='" << name << "'"
            << std::endl;
    }

} // namespace StarryEngine::RHI