#include<interface/RHIManager.hpp>
#include<logging/Logger.hpp>

namespace StarryEngine::RHI {

    // ==================== ResourceManager 构造函数/析构函数 ====================
    ResourceManager::ResourceManager(std::shared_ptr<IResourceFactory> factory)
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

        auto handle = buffers_.create(std::move(resource), name, debugTag);
        if (handle.isValid()) {
            logResourceCreation(ResourceCategory::Buffer, desc.debugName);
        }
        else {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to register buffer (name conflict?): " << name << std::endl;
            }
        }
        return handle;
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

        auto handle = textures_.create(std::move(resource), name, debugTag);
        if (handle.isValid()) {
            logResourceCreation(ResourceCategory::Texture, desc.debugName);
        }
        else {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to register texture (name conflict?): " << name << std::endl;
            }
        }
        return handle;
    }

    PipelineHandle ResourceManager::createGraphicsPipeline(const GraphicsPipelineDesc& desc,const std::string& name,const std::string& debugTag) {
        auto* layoutObj = getPipelineLayout(desc.pipelineLayoutHandle);
        auto* vsObj = getShader(desc.vertexShader);
        auto* fsObj = getShader(desc.fragmentShader);
        auto* rpObj = getRenderPass(desc.renderPass);
        auto resource = factory_->createGraphicPipeline(desc, layoutObj, vsObj, fsObj, rpObj);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create graphics pipeline: " << name << std::endl;
            }
            return PipelineHandle::Null();
        }

        auto handle = pipelines_.create(std::move(resource), name, debugTag);
        if (handle.isValid()) {
            logResourceCreation(ResourceCategory::Pipeline, desc.debugName);
        }
        else {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to register  graphics pipeline (name conflict?): " << name << std::endl;
            }
        }
        return handle;
    }

    PipelineHandle ResourceManager::createComputePipeline(const ComputePipelineDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto* shaderObj = getShader(desc.computeShader);
        auto* layoutObj = getPipelineLayout(desc.pipelineLayoutHandle);
        auto resource = factory_->createComputePipeline(desc, shaderObj, layoutObj);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create compute pipeline: " << name << std::endl;
            }
            return PipelineHandle::Null();
        }

        auto handle = pipelines_.create(std::move(resource), name, debugTag);
        if (handle.isValid()) {
            logResourceCreation(ResourceCategory::Pipeline, desc.debugName);
        }
        else {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to register compute pipeline (name conflict?): " << name << std::endl;
            }
        }
        return handle;
    }

    PipelineLayoutHandle ResourceManager::createPipelineLayout(const PipelineLayoutDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        std::vector<RHIDescriptorSetLayout*> layouts;
        layouts.reserve(desc.descriptorSetLayouts.size());
        for (auto handle : desc.descriptorSetLayouts)
            layouts.push_back(getDescriptorSetLayout(handle));
        auto resource = factory_->createPipelineLayout(desc, layouts);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create pipeline layout: " << name << std::endl;
            }
            return PipelineLayoutHandle::Null();
        }
        auto handle = pipelineLayouts_.create(std::move(resource), name, debugTag);
        if (handle.isValid()) {
            logResourceCreation(ResourceCategory::PipelineLayout, desc.debugName);
        }
        else {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to register pipeline layout (name conflict?): " << name << std::endl;
            }
        }
        return handle;
    }


    ShaderHandle ResourceManager::createShader(const ShaderModuleDesc& desc,
        const std::string& name,
        const std::string& debugTag) {

        // 创建资源
        auto resource = factory_->createShader(desc);

        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create shader: "<< name << std::endl;
            }
            return ShaderHandle::Null();
        }

        auto handle = shaders_.create(std::move(resource), name, debugTag);
        if (handle.isValid()) {
            logResourceCreation(ResourceCategory::Shader, desc.debugName);
        }
        else {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to register shader (name conflict?): " << name << std::endl;
            }
        }
        return handle;
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
        auto handle = samplers_.create(std::move(resource), name, debugTag);
        if (handle.isValid()) {
            logResourceCreation(ResourceCategory::Sampler, desc.debugName);
        }
        else {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to register sampler (name conflict?): " << name << std::endl;
            }
        }
        return handle;
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
        auto handle = renderPasses_.create(std::move(resource), name, debugTag);
        if (handle.isValid()) {
            logResourceCreation(ResourceCategory::RenderPass, desc.debugName);
        }
        else {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to register render pass (name conflict?): " << name << std::endl;
            }
        }
        return handle;
    }

    FramebufferHandle ResourceManager::createFramebuffer(
        RenderPassHandle renderPass,
        const std::vector<TextureHandle>& attachmentHandles,
        const FramebufferDesc& desc,
        const std::string& name,
        const std::string& debugTag) {

        auto* rp = getRenderPass(renderPass);
        std::vector<RHITexture*> attachments;
        attachments.reserve(attachmentHandles.size());
        for (auto h : attachmentHandles) {
            if (auto* t = getTexture(h)) attachments.push_back(t);
        }
        auto resource = factory_->createFramebuffer(rp, attachments, desc);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create framebuffer: " << name << std::endl;
            }
            return FramebufferHandle::Null();
        }
        auto handle = framebuffers_.create(std::move(resource), name, debugTag);
        if (handle.isValid()) {
            logResourceCreation(ResourceCategory::Framebuffer, desc.debugName);
        }
        else {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to register framebuffer (name conflict?): " << name << std::endl;
            }
        }
        return handle;
    }

    // ==================== 其他资源创建方法 ====================
    DescriptorSetHandle ResourceManager::createDescriptorSet(const DescriptorSetDesc& desc,
        const std::string& name,
        const std::string& debugTag) {
        auto* pool = getDescriptorPool(desc.descriptorPool);
        RHIDescriptorSetLayout* layout = nullptr;
        if (desc.descriptorSetLayout.isValid()) {
            layout = getDescriptorSetLayout(desc.descriptorSetLayout);
        }
        else if (auto* pipelineLayout = getPipelineLayout(desc.pipelineLayout)) {
            layout = getDescriptorSetLayout(pipelineLayout->getLayoutHandle(desc.setIndex));
        }
        auto resource = factory_->createDescriptorSet(desc, pool, layout);
        if (!resource) {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to create descriptor set: " << name << std::endl;
            }
            return DescriptorSetHandle::Null();
        }
        auto handle = descriptorSets_.create(std::move(resource), name, debugTag);
        if (handle.isValid()) {
            logResourceCreation(ResourceCategory::DescriptorSet, desc.debugName);
        }
        else {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to register buffer (name conflict?): " << name << std::endl;
            }
        }
        return handle;
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
        auto handle = descriptorPools_.create(std::move(resource), name, debugTag);
        if (handle.isValid()) {
            logResourceCreation(ResourceCategory::DescriptorPool, desc.debugName);
        }
        else {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to register buffer (name conflict?): " << name << std::endl;
            }
        }
        return handle;
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
        auto handle = descriptorSetLayouts_.create(std::move(resource), name, debugTag);
        if (handle.isValid()) {
            logResourceCreation(ResourceCategory::DescriptorSetLayout, desc.debugName);
        }
        else {
            if (debugMode_) {
                std::cerr << "[ResourceManager] Failed to register buffer (name conflict?): " << name << std::endl;
            }
        }
        return handle;
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
        auto* entry = buffers_.getEntry(handle);
        if (!entry || !entry->isValid()) {
            LOG_ERROR("Attempt to get invalid buffer handle: index={}, gen={}",
                handle.getIndex(), handle.getGeneration());
            return nullptr;
        }
        return entry->data.get();
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
    }

    ResourceManager::Statistics ResourceManager::getStatistics() const {
        //std::lock_guard<std::mutex> lock(statsMutex_);

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

        stats.totalResources = stats.bufferCount + stats.textureCount + stats.pipelineCount +
            stats.pipelineLayoutCount + stats.shaderCount + stats.samplerCount +
            stats.renderPassCount + stats.framebufferCount + stats.descriptorSetCount +
            stats.descriptorPoolCount + stats.descriptorSetLayoutCount;

        stats.bufferMemoryUsage = buffers_.getTotalMemoryUsage();
        stats.textureMemoryUsage = textures_.getTotalMemoryUsage();
        stats.totalMemoryUsage = stats.bufferMemoryUsage + stats.textureMemoryUsage;

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
    }

    bool ResourceManager::checkMemoryUsage() const {
        return buffers_.isMemoryUsageExceeded() ||
            textures_.isMemoryUsageExceeded();
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
        //std::lock_guard<std::mutex> lock(statsMutex_);
        stats_ = stats;
    }

    void ResourceManager::logResourceCreation(ResourceCategory category, const std::string& name) {
        if (!debugMode_) return;
        LOG_INFO("Created resource: Category={}, ResourceType={}{}",
            static_cast<int>(category),
            ResourceCategoryToString(category),
            name.empty() ? "" : (", Name='" + name + "'"));
    }

    void ResourceManager::logResourceDestruction(ResourceCategory category, const std::string& name) {
        if (!debugMode_) return;

        //std::lock_guard<std::mutex> lock(statsMutex_);
        LOG_INFO("Created resource: Category={}, ResourceType={}{}",
            static_cast<int>(category),
            ResourceCategoryToString(category),
            name.empty() ? "" : (", Name='" + name + "'"));
    }

    const char* ResourceManager::ResourceCategoryToString(ResourceCategory category) {
        switch (category) {
        case ResourceCategory::Buffer:                 return "Buffer";
        case ResourceCategory::Texture:                return "Texture";
        case ResourceCategory::Pipeline:               return "Pipeline";
        case ResourceCategory::PipelineLayout:         return "PipelineLayout";
        case ResourceCategory::Shader:                 return "Shader";
        case ResourceCategory::RenderPass:             return "RenderPass";
        case ResourceCategory::Framebuffer:            return "Framebuffer";
        case ResourceCategory::DescriptorSet:          return "DescriptorSet";
        case ResourceCategory::DescriptorPool:         return "DescriptorPool";
        case ResourceCategory::DescriptorSetLayout:    return "DescriptorSetLayout";
        case ResourceCategory::Sampler:                 return "Sampler";
        case ResourceCategory::QueryPool:               return "QueryPool";
        case ResourceCategory::CommandBuffer:           return "CommandBuffer";
        case ResourceCategory::CommandPool:             return "CommandPool";
        case ResourceCategory::Fence:                   return "Fence";
        case ResourceCategory::Semaphore:               return "Semaphore";
        case ResourceCategory::Event:                   return "Event";
        case ResourceCategory::SwapChain:               return "SwapChain";
        case ResourceCategory::AccelerationStructure:   return "AccelerationStructure";
        case ResourceCategory::Queue:                   return "Queue";
        case ResourceCategory::MAX_CATEGORIES:          return "MAX_CATEGORIES"; 
        default:                                         return "Unknown";
        }
    }

    void ResourceManager::scheduleDestroy(std::function<void()> destructor, uint32_t framesToWait) {
        m_deferredDestroys.push_back({ m_currentFrame + framesToWait, std::move(destructor) });
    }

    void ResourceManager::tickFrame(uint64_t currentFrame) {
        if (currentFrame != 0) {
            m_currentFrame = currentFrame;
        }
        else {
            ++m_currentFrame;
        }
        auto it = std::remove_if(m_deferredDestroys.begin(), m_deferredDestroys.end(),
            [this](const DeferredDestruction& d) {
                if (d.targetFrame <= m_currentFrame) {
                    d.destructor();
                    return true;
                }
                return false;
            });
        m_deferredDestroys.erase(it, m_deferredDestroys.end());
    }

} // namespace StarryEngine::RHI