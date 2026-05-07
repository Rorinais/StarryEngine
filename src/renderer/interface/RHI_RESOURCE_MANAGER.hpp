#pragma once
#include <unordered_map>
#include <iostream>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <string>
#include <array>
#include <functional>
#include <cassert>
#include <optional>
#include <chrono>
#include <thread>
#include <cstdint>  

#include "RHI_HANDLES_SYSTEM.hpp"
#include "RHI_STRUCTS_DESC.hpp"
#include "RHI_STRUCTS_RESOURCE.hpp"
#include "RHI_RESOURCE_FACTORY.hpp"

namespace StarryEngine::RHI {
    template<typename HandleType, typename ResourceType>
    class TypedResourceStorage {
        static_assert(std::is_base_of<IResource, ResourceType>::value,"ResourceType must inherit from IResource");
    public:
        using Handle = HandleType;
        using ResourcePtr = std::unique_ptr<ResourceType>;

        struct Entry {
            ResourcePtr data;
            std::string name;
            uint32_t generation = 1;
            uint32_t refCount = 0;
            bool alive = false;
            std::chrono::steady_clock::time_point createTime;

            // 调试信息
            size_t memoryUsage = 0;
            std::string debugTag;

            bool isValid() const { return alive && data && data->isValid(); }
        };

        TypedResourceStorage() = default;
        ~TypedResourceStorage() { clear(); }

        // 禁止拷贝和移动
        TypedResourceStorage(const TypedResourceStorage&) = delete;
        TypedResourceStorage& operator=(const TypedResourceStorage&) = delete;
        TypedResourceStorage(TypedResourceStorage&&) = delete;
        TypedResourceStorage& operator=(TypedResourceStorage&&) = delete;

        // 创建资源
        Handle create(ResourcePtr data, const std::string& name = "",const std::string& debugTag = "");

        // 批量创建资源
        template<typename... Args>
        std::vector<Handle> createMultiple(uint32_t count, Args&&... args);

        // 获取资源数据
        ResourceType* getData(Handle handle);
        const ResourceType* getData(Handle handle) const;
        IResource* getResource(Handle handle);
        const IResource* getResource(Handle handle) const;

        // 安全获取资源（带错误检查）
        std::optional<std::reference_wrapper<ResourceType>> tryGetData(Handle handle);

        // 清除所有资源
        void clear();

        // 获取资源数量
        size_t size() const;
        bool empty() const;

        // 按名称查找
        Handle findByName(const std::string& name) const;

        // 按调试标签查找
        Handle findByDebugTag(const std::string& debugTag) const;

        // 资源引用计数管理
        bool addRef(Handle handle);
        bool release(Handle handle);
        bool destroy(Handle handle);

        template<typename Func>
        void forEach(Func&& func);

        // 添加const版本
        template<typename Func>
        void forEach(Func&& func) const;

        // 带过滤器的遍历
        template<typename Func, typename Filter>
        void forEachFiltered(Func&& func, Filter&& filter);

        // 获取内存使用情况
        size_t getTotalMemoryUsage() const;

        // 获取所有活跃句柄
        std::vector<Handle> getAllHandles() const;

        // 获取资源信息
        struct EntryInfo {
            Handle handle;
            std::string name;
            std::string debugTag;
            uint32_t refCount;
            size_t memoryUsage;
            std::chrono::steady_clock::time_point createTime;
            ResourceType* resource;
        };

        std::optional<EntryInfo> getEntryInfo(Handle handle) const;

        // 获取统计信息
        struct Statistics {
            size_t totalCreated = 0;
            size_t totalDestroyed = 0;
            size_t currentCount = 0;
            size_t totalMemoryUsage = 0;
            size_t maxMemoryUsage = 0;
            size_t chunkCount = 0;
        };

        Statistics getStatistics() const;

        // 设置内存预警阈值
        void setMemoryWarningThreshold(size_t threshold);

        // 检查内存使用是否超过阈值
        bool isMemoryUsageExceeded() const;

        // 获取最老资源（用于LRU缓存）
        std::optional<Handle> getOldestResource() const;

        // 获取内部entry指针
        Entry* getEntry(Handle handle);
        const Entry* getEntry(Handle handle) const;
        ResourceType* getEntryData(Handle handle);

    private:
        static constexpr size_t CHUNK_SIZE = 256;
        struct Chunk {
            std::array<Entry, CHUNK_SIZE> entries;
        };

        // 根据索引获取句柄
        Handle getHandleFromIndex(uint32_t globalIdx) const;

        // 销毁entry
        void destroyEntry(Entry* entry);

        // 尝试收缩内存
        void tryShrink();

    private:
        bool debugMode_ = false;
        //mutable std::mutex mutex_;
        std::vector<Chunk> chunks_;
        std::unordered_map<std::string, uint32_t> nameToIndex_;
        std::unordered_map<std::string, uint32_t> debugTagToIndex_;

        // 统计信息
        std::atomic<size_t> totalCreated_{ 0 };
        std::atomic<size_t> totalDestroyed_{ 0 };
        std::atomic<size_t> maxMemoryUsage_{ 0 };
        size_t memoryWarningThreshold_ = 0;
		std::atomic<size_t> currentMemoryUsage_{ 0 };
    };

    class IResourceManager {
    public:
        virtual ~IResourceManager() = default;
        virtual TextureHandle createTexture(const TextureDesc& desc,const std::string& name = "",const std::string& debugTag = "") = 0;
        virtual BufferHandle createBuffer(const BufferDesc& desc,const std::string& name = "",const std::string& debugTag = "") = 0;
        virtual bool destroy(TextureHandle handle) = 0;
        virtual bool destroy(BufferHandle handle) = 0;
    };


    // ==================== 全局资源管理器 ====================
    class ResourceManager:IResourceManager {
    public:
        struct Statistics {
            size_t totalResources = 0;
            size_t bufferCount = 0;
            size_t textureCount = 0;
            size_t pipelineCount = 0;
            size_t pipelineLayoutCount = 0;
            size_t shaderCount = 0;
            size_t samplerCount = 0;
            size_t renderPassCount = 0;
            size_t framebufferCount = 0;
            size_t descriptorSetCount = 0;
            size_t descriptorPoolCount = 0;
            size_t descriptorSetLayoutCount = 0;
            size_t commandBufferCount = 0;
            size_t commandPoolCount = 0;
            size_t fenceCount = 0;
            size_t semaphoreCount = 0;
            size_t eventCount = 0;
            size_t queryPoolCount = 0;
            size_t accelerationStructureCount = 0;
            size_t swapChainCount = 0;
            size_t queueCount = 0;

            size_t totalMemoryUsage = 0;
            size_t bufferMemoryUsage = 0;
            size_t textureMemoryUsage = 0;
            size_t accelerationStructureMemoryUsage = 0;

            // 性能统计
            uint64_t totalCreateTime = 0;
            uint64_t totalDestroyTime = 0;
            size_t peakResourceCount = 0;
            size_t peakMemoryUsage = 0;
        };

        ResourceManager(std::shared_ptr<IResourceFactory> factory);
        ~ResourceManager();

        // 禁止拷贝和移动
        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;
        ResourceManager(ResourceManager&&) = delete;
        ResourceManager& operator=(ResourceManager&&) = delete;

        // ========== 资源创建接口 ==========

        // 缓冲区
        BufferHandle createBuffer(const BufferDesc& desc,
            const std::string& name = "",
            const std::string& debugTag = "") override;

        // 纹理
        TextureHandle createTexture(const TextureDesc& desc,
            const std::string& name = "",
            const std::string& debugTag = "") override;

        // 图形管线
        PipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc,
            const std::string& name = "",
            const std::string& debugTag = "");

        // 计算管线
        PipelineHandle createComputePipeline(const ComputePipelineDesc& desc,
            const std::string& name = "",
            const std::string& debugTag = "");

        // 管线布局
        PipelineLayoutHandle createPipelineLayout(const PipelineLayoutDesc& desc,
            const std::string& name = "",
            const std::string& debugTag = "");

        // 着色器模块
        ShaderHandle createShader(const ShaderModuleDesc& desc,
            const std::string& name = "",
            const std::string& debugTag = "");

        // 采样器
        SamplerHandle createSampler(const SamplerDesc& desc,
            const std::string& name = "",
            const std::string& debugTag = "");

        // 渲染通道
        RenderPassHandle createRenderPass(const RenderPassDesc& desc,
            const std::string& name = "",
            const std::string& debugTag = "");

        // 帧缓冲
        FramebufferHandle createFramebuffer(const FramebufferDesc& desc,
            const std::string& name = "",
            const std::string& debugTag = "");

        // 描述符集
        DescriptorSetHandle createDescriptorSet(const DescriptorSetDesc& desc,
            const std::string& name = "",
            const std::string& debugTag = "");

        // 描述符池
        DescriptorPoolHandle createDescriptorPool(const DescriptorPoolDesc& desc,
            const std::string& name = "",
            const std::string& debugTag = "");

        // 描述符集布局
        DescriptorSetLayoutHandle createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc,
            const std::string& name = "",
            const std::string& debugTag = "");

        //// 命令缓冲区
        //CommandBufferHandle createCommandBuffer(const CommandBufferDesc& desc,
        //    const std::string& name = "",
        //    const std::string& debugTag = "");

        //// 命令池
        //CommandPoolHandle createCommandPool(const CommandPoolDesc& desc,
        //    const std::string& name = "",
        //    const std::string& debugTag = "");

        //// 栅栏
        //FenceHandle createFence(const FenceDesc& desc,
        //    const std::string& name = "",
        //    const std::string& debugTag = "");

        //// 信号量
        //SemaphoreHandle createSemaphore(const SemaphoreDesc& desc,
        //    const std::string& name = "",
        //    const std::string& debugTag = "");

        //// 事件
        //EventHandle createEvent(const EventDesc& desc,
        //    const std::string& name = "",
        //    const std::string& debugTag = "");

        //// 查询池
        //QueryPoolHandle createQueryPool(const QueryPoolDesc& desc,
        //    const std::string& name = "",
        //    const std::string& debugTag = "");

        //// 加速结构
        //AccelerationStructureHandle createAccelerationStructure(const AccelerationStructureDesc& desc,
        //    const std::string& name = "",
        //    const std::string& debugTag = "");

        //// 交换链
        //SwapChainHandle createSwapChain(const SwapChainDesc& desc,
        //    const std::string& name = "",
        //    const std::string& debugTag = "");

        //// 队列
        //QueueHandle createQueue(const QueueDesc& desc,
        //    const std::string& name = "",
        //    const std::string& debugTag = "");

        // ========== 批量创建接口 ==========

        //std::vector<CommandBufferHandle> createCommandBuffers(uint32_t count,
        //    const CommandBufferDesc& desc,
        //    const std::string& baseName = "");

        std::vector<DescriptorSetHandle> createDescriptorSets(uint32_t count,
            const DescriptorSetDesc& desc,
            const std::string& baseName = "");

        // ========== 资源获取接口 ==========

        RHIBuffer* getBuffer(BufferHandle handle);
        const RHIBuffer* getBuffer(BufferHandle handle) const;

        RHITexture* getTexture(TextureHandle handle);
        const RHITexture* getTexture(TextureHandle handle) const;

        RHIPipeline* getPipeline(PipelineHandle handle);
        const RHIPipeline* getPipeline(PipelineHandle handle) const;

        RHIPipelineLayout* getPipelineLayout(PipelineLayoutHandle handle);
        const RHIPipelineLayout* getPipelineLayout(PipelineLayoutHandle handle) const;

        RHIShaderModule* getShader(ShaderHandle handle);
        const RHIShaderModule* getShader(ShaderHandle handle) const;

        RHISampler* getSampler(SamplerHandle handle);
        const RHISampler* getSampler(SamplerHandle handle) const;

        RHIRenderPass* getRenderPass(RenderPassHandle handle);
        const RHIRenderPass* getRenderPass(RenderPassHandle handle) const;

        RHIFramebuffer* getFramebuffer(FramebufferHandle handle);
        const RHIFramebuffer* getFramebuffer(FramebufferHandle handle) const;

        RHIDescriptorSet* getDescriptorSet(DescriptorSetHandle handle);
        const RHIDescriptorSet* getDescriptorSet(DescriptorSetHandle handle) const;

        RHIDescriptorPool* getDescriptorPool(DescriptorPoolHandle handle);
        const RHIDescriptorPool* getDescriptorPool(DescriptorPoolHandle handle) const;

        RHIDescriptorSetLayout* getDescriptorSetLayout(DescriptorSetLayoutHandle handle);
        const RHIDescriptorSetLayout* getDescriptorSetLayout(DescriptorSetLayoutHandle handle) const;

        //RHICommandBuffer* getCommandBuffer(CommandBufferHandle handle);
        //const RHICommandBuffer* getCommandBuffer(CommandBufferHandle handle) const;

        //RHICommandPool* getCommandPool(CommandPoolHandle handle);
        //const RHICommandPool* getCommandPool(CommandPoolHandle handle) const;

        //RHIFence* getFence(FenceHandle handle);
        //const RHIFence* getFence(FenceHandle handle) const;

        //RHISemaphore* getSemaphore(SemaphoreHandle handle);
        //const RHISemaphore* getSemaphore(SemaphoreHandle handle) const;

        //RHIEvent* getEvent(EventHandle handle);
        //const RHIEvent* getEvent(EventHandle handle) const;

        //RHIQueryPool* getQueryPool(QueryPoolHandle handle);
        //const RHIQueryPool* getQueryPool(QueryPoolHandle handle) const;

        //RHIAccelerationStructure* getAccelerationStructure(AccelerationStructureHandle handle);
        //const RHIAccelerationStructure* getAccelerationStructure(AccelerationStructureHandle handle) const;

        //RHISwapChain* getSwapChain(SwapChainHandle handle);
        //const RHISwapChain* getSwapChain(SwapChainHandle handle) const;

        //RHIQueue* getQueue(QueueHandle handle);
        //const RHIQueue* getQueue(QueueHandle handle) const;

        // ========== 按名称查找 ==========

        BufferHandle findBufferByName(const std::string& name);
        TextureHandle findTextureByName(const std::string& name);
        PipelineHandle findPipelineByName(const std::string& name);
        ShaderHandle findShaderByName(const std::string& name);
        SamplerHandle findSamplerByName(const std::string& name);
        RenderPassHandle findRenderPassByName(const std::string& name);
        FramebufferHandle findFramebufferByName(const std::string& name);
        DescriptorSetHandle findDescriptorSetByName(const std::string& name);
        DescriptorPoolHandle findDescriptorPoolByName(const std::string& name);
        DescriptorSetLayoutHandle findDescriptorSetLayoutByName(const std::string& name);
        //CommandBufferHandle findCommandBufferByName(const std::string& name);
        //CommandPoolHandle findCommandPoolByName(const std::string& name);
        //FenceHandle findFenceByName(const std::string& name);
        //SemaphoreHandle findSemaphoreByName(const std::string& name);
        //EventHandle findEventByName(const std::string& name);
        //QueryPoolHandle findQueryPoolByName(const std::string& name);
        //AccelerationStructureHandle findAccelerationStructureByName(const std::string& name);
        //SwapChainHandle findSwapChainByName(const std::string& name);
        //QueueHandle findQueueByName(const std::string& name);

        // ========== 按调试标签查找 ==========

        BufferHandle findBufferByDebugTag(const std::string& debugTag);
        TextureHandle findTextureByDebugTag(const std::string& debugTag);
        PipelineHandle findPipelineByDebugTag(const std::string& debugTag);
        ShaderHandle findShaderByDebugTag(const std::string& debugTag);
        SamplerHandle findSamplerByDebugTag(const std::string& debugTag);
        RenderPassHandle findRenderPassByDebugTag(const std::string& debugTag);
        FramebufferHandle findFramebufferByDebugTag(const std::string& debugTag);
        DescriptorSetHandle findDescriptorSetByDebugTag(const std::string& debugTag);
        DescriptorPoolHandle findDescriptorPoolByDebugTag(const std::string& debugTag);
        DescriptorSetLayoutHandle findDescriptorSetLayoutByDebugTag(const std::string& debugTag);
        //CommandBufferHandle findCommandBufferByDebugTag(const std::string& debugTag);
        //CommandPoolHandle findCommandPoolByDebugTag(const std::string& debugTag);
        //FenceHandle findFenceByDebugTag(const std::string& debugTag);
        //SemaphoreHandle findSemaphoreByDebugTag(const std::string& debugTag);
        //EventHandle findEventByDebugTag(const std::string& debugTag);
        //QueryPoolHandle findQueryPoolByDebugTag(const std::string& debugTag);
        //AccelerationStructureHandle findAccelerationStructureByDebugTag(const std::string& debugTag);
        //SwapChainHandle findSwapChainByDebugTag(const std::string& debugTag);
        //QueueHandle findQueueByDebugTag(const std::string& debugTag);

        // ========== 资源引用计数管理 ==========

        bool addRef(BufferHandle handle);
        bool release(BufferHandle handle);
        bool destroy(BufferHandle handle) override;

        bool addRef(TextureHandle handle);
        bool release(TextureHandle handle);
        bool destroy(TextureHandle handle) override;

        bool addRef(PipelineHandle handle);
        bool release(PipelineHandle handle);
        bool destroy(PipelineHandle handle);

        bool addRef(PipelineLayoutHandle handle);
        bool release(PipelineLayoutHandle handle);
        bool destroy(PipelineLayoutHandle handle);

        bool addRef(ShaderHandle handle);
        bool release(ShaderHandle handle);
        bool destroy(ShaderHandle handle);

        bool addRef(SamplerHandle handle);
        bool release(SamplerHandle handle);
        bool destroy(SamplerHandle handle);

        bool addRef(RenderPassHandle handle);
        bool release(RenderPassHandle handle);
        bool destroy(RenderPassHandle handle);

        bool addRef(FramebufferHandle handle);
        bool release(FramebufferHandle handle);
        bool destroy(FramebufferHandle handle);

        bool addRef(DescriptorSetHandle handle);
        bool release(DescriptorSetHandle handle);
        bool destroy(DescriptorSetHandle handle);

        bool addRef(DescriptorPoolHandle handle);
        bool release(DescriptorPoolHandle handle);
        bool destroy(DescriptorPoolHandle handle);

        bool addRef(DescriptorSetLayoutHandle handle);
        bool release(DescriptorSetLayoutHandle handle);
        bool destroy(DescriptorSetLayoutHandle handle);

        //bool addRef(CommandBufferHandle handle);
        //bool release(CommandBufferHandle handle);
        //bool destroy(CommandBufferHandle handle);

        //bool addRef(CommandPoolHandle handle);
        //bool release(CommandPoolHandle handle);
        //bool destroy(CommandPoolHandle handle);

        //bool addRef(FenceHandle handle);
        //bool release(FenceHandle handle);
        //bool destroy(FenceHandle handle);

        //bool addRef(SemaphoreHandle handle);
        //bool release(SemaphoreHandle handle);
        //bool destroy(SemaphoreHandle handle);

        //bool addRef(EventHandle handle);
        //bool release(EventHandle handle);
        //bool destroy(EventHandle handle);

        //bool addRef(QueryPoolHandle handle);
        //bool release(QueryPoolHandle handle);
        //bool destroy(QueryPoolHandle handle);

        //bool addRef(AccelerationStructureHandle handle);
        //bool release(AccelerationStructureHandle handle);
        //bool destroy(AccelerationStructureHandle handle);

        //bool addRef(SwapChainHandle handle);
        //bool release(SwapChainHandle handle);
        //bool destroy(SwapChainHandle handle);

        //bool addRef(QueueHandle handle);
        //bool release(QueueHandle handle);
        //bool destroy(QueueHandle handle);

        // ========== 资源存储访问 ==========

        TypedResourceStorage<BufferHandle, RHIBuffer>& getBufferStorage();
        TypedResourceStorage<TextureHandle, RHITexture>& getTextureStorage();
        TypedResourceStorage<PipelineHandle, RHIPipeline>& getPipelineStorage();
        TypedResourceStorage<PipelineLayoutHandle, RHIPipelineLayout>& getPipelineLayoutStorage();
        TypedResourceStorage<ShaderHandle, RHIShaderModule>& getShaderStorage();
        TypedResourceStorage<SamplerHandle, RHISampler>& getSamplerStorage();
        TypedResourceStorage<RenderPassHandle, RHIRenderPass>& getRenderPassStorage();
        TypedResourceStorage<FramebufferHandle, RHIFramebuffer>& getFramebufferStorage();
        TypedResourceStorage<DescriptorSetHandle, RHIDescriptorSet>& getDescriptorSetStorage();
        TypedResourceStorage<DescriptorPoolHandle, RHIDescriptorPool>& getDescriptorPoolStorage();
        TypedResourceStorage<DescriptorSetLayoutHandle, RHIDescriptorSetLayout>& getDescriptorSetLayoutStorage();
        //TypedResourceStorage<CommandBufferHandle, RHICommandBuffer>& getCommandBufferStorage();
        //TypedResourceStorage<CommandPoolHandle, RHICommandPool>& getCommandPoolStorage();
        //TypedResourceStorage<FenceHandle, RHIFence>& getFenceStorage();
        //TypedResourceStorage<SemaphoreHandle, RHISemaphore>& getSemaphoreStorage();
        //TypedResourceStorage<EventHandle, RHIEvent>& getEventStorage();
        //TypedResourceStorage<QueryPoolHandle, RHIQueryPool>& getQueryPoolStorage();
        //TypedResourceStorage<AccelerationStructureHandle, RHIAccelerationStructure>& getAccelerationStructureStorage();
        //TypedResourceStorage<SwapChainHandle, RHISwapChain>& getSwapChainStorage();
        //TypedResourceStorage<QueueHandle, RHIQueue>& getQueueStorage();

        // ========== 管理功能 ==========

        // 清除所有资源
        void clearAll();

        // 获取统计信息
        Statistics getStatistics() const;

        // 打印统计信息
        void dumpStatistics() const;

        // 获取详细统计报告
        std::string getDetailedReport() const;

        // 设置内存预警
        void setMemoryWarningThreshold(size_t threshold);

        // 检查内存使用
        bool checkMemoryUsage() const;

        // 尝试释放未使用资源
        size_t tryReleaseUnusedResources();

        // 获取工厂
        IResourceFactory* getFactory() const;

        // 设置调试模式
        void setDebugMode(bool enabled);
        bool isDebugMode() const;

        // 验证资源完整性
        bool validateResources() const;

        // 获取资源泄露报告
        std::vector<std::string> getResourceLeakReport() const;

        void scheduleDestroy(std::function<void()> destructor, uint32_t framesToWait = 2);
        void tickFrame(uint64_t currentFrame = 0);

    private:
        struct DeferredDestruction {
            uint64_t targetFrame;
            std::function<void()> destructor;
        };
        std::vector<DeferredDestruction> m_deferredDestroys;
        uint64_t m_currentFrame = 0;
    private:
        void initStatistics();
        void updateStatistics();
        void logResourceCreation(ResourceCategory category, const std::string& name);
        void logResourceDestruction(ResourceCategory category, const std::string& name);
        const char* ResourceCategoryToString(ResourceCategory category);

        // 性能测量
        struct TimingInfo {
            std::chrono::steady_clock::time_point startTime;
            std::string operation;
            ResourceCategory category;
        };

        std::unordered_map<std::thread::id, TimingInfo> activeOperations_;

    private:
        std::shared_ptr<IResourceFactory> factory_;
        bool debugMode_ = false;

        // 分类型存储
        TypedResourceStorage<BufferHandle, RHIBuffer> buffers_;
        TypedResourceStorage<TextureHandle, RHITexture> textures_;
        TypedResourceStorage<PipelineHandle, RHIPipeline> pipelines_;
        TypedResourceStorage<PipelineLayoutHandle, RHIPipelineLayout> pipelineLayouts_;
        TypedResourceStorage<ShaderHandle, RHIShaderModule> shaders_;
        TypedResourceStorage<SamplerHandle, RHISampler> samplers_;
        TypedResourceStorage<RenderPassHandle, RHIRenderPass> renderPasses_;
        TypedResourceStorage<FramebufferHandle, RHIFramebuffer> framebuffers_;
        TypedResourceStorage<DescriptorSetHandle, RHIDescriptorSet> descriptorSets_;
        TypedResourceStorage<DescriptorPoolHandle, RHIDescriptorPool> descriptorPools_;
        TypedResourceStorage<DescriptorSetLayoutHandle, RHIDescriptorSetLayout> descriptorSetLayouts_;
        //TypedResourceStorage<CommandBufferHandle, RHICommandBuffer> commandBuffers_;
        //TypedResourceStorage<CommandPoolHandle, RHICommandPool> commandPools_;
        //TypedResourceStorage<FenceHandle, RHIFence> fences_;
        //TypedResourceStorage<SemaphoreHandle, RHISemaphore> semaphores_;
        //TypedResourceStorage<EventHandle, RHIEvent> events_;
        //TypedResourceStorage<QueryPoolHandle, RHIQueryPool> queryPools_;
        //TypedResourceStorage<AccelerationStructureHandle, RHIAccelerationStructure> accelerationStructures_;
        //TypedResourceStorage<SwapChainHandle, RHISwapChain> swapChains_;
        //TypedResourceStorage<QueueHandle, RHIQueue> queues_;

        // 统计信息
       // mutable std::mutex statsMutex_;
        Statistics stats_;
        mutable std::atomic<size_t> peakResourceCount_{ 0 };  // 添加 mutable
        mutable std::atomic<size_t> peakMemoryUsage_{ 0 };    // 添加 mutable
        std::chrono::steady_clock::time_point startTime_;
    };

    // ==================== 模板方法实现 ====================
    template<typename HandleType, typename ResourceType>
    typename TypedResourceStorage<HandleType, ResourceType>::Handle
        TypedResourceStorage<HandleType, ResourceType>::create(ResourcePtr data,
            const std::string& name,
            const std::string& debugTag) {
        // 1. 名称唯一性检查（与原有逻辑一致）
        if (!name.empty()) {
            if (nameToIndex_.find(name) != nameToIndex_.end()) {
                if (debugMode_) {
                    std::cerr << "[ResourceStorage] Resource with name '"
                        << name << "' already exists!" << std::endl;
                }
                return Handle::Null();
            }
        }

        // 2. 遍历现有块，寻找空闲槽位
        for (size_t chunkIdx = 0; chunkIdx < chunks_.size(); ++chunkIdx) {
            auto& chunk = chunks_[chunkIdx];
            for (uint32_t i = 0; i < CHUNK_SIZE; ++i) {
                if (!chunk.entries[i].alive) {
                    uint32_t globalIdx = static_cast<uint32_t>(chunkIdx * CHUNK_SIZE + i);
                    Entry& entry = chunk.entries[i];

                    // 填充条目数据
                    entry.data = std::move(data);
                    entry.name = name;
                    entry.debugTag = debugTag;
                    entry.generation++;
                    entry.refCount = 1;
                    entry.alive = true;
                    entry.createTime = std::chrono::steady_clock::now();
                    entry.memoryUsage = entry.data ? entry.data->getMemoryUsage() : 0;

                    // 更新索引映射（此时 name 已保证不重复）
                    if (!name.empty()) {
                        nameToIndex_[name] = globalIdx;
                    }
                    if (!debugTag.empty()) {
                        debugTagToIndex_[debugTag] = globalIdx;
                    }

                    // 统计信息：总创建数、内存使用
                    totalCreated_++;
                    size_t memIncrease = entry.memoryUsage;
                    size_t newTotal = currentMemoryUsage_.fetch_add(memIncrease) + memIncrease;
                    size_t oldMax = maxMemoryUsage_.load();
                    while (newTotal > oldMax) {
                        if (maxMemoryUsage_.compare_exchange_weak(oldMax, newTotal)) {
                            break;
                        }
                    }

                    return Handle::Create(globalIdx, entry.generation);
                }
            }
        }

        // 3. 无空闲槽位 → 添加新块，直接分配第 0 个槽位
        chunks_.emplace_back();          // 修改点
        Chunk& newChunk = chunks_.back();
        uint32_t globalIdx = static_cast<uint32_t>((chunks_.size() - 1) * CHUNK_SIZE);
        Entry& entry = newChunk.entries[0];

        // 填充条目（与上面分支完全一致）
        entry.data = std::move(data);
        entry.name = name;
        entry.debugTag = debugTag;
        entry.generation++;
        entry.refCount = 1;
        entry.alive = true;
        entry.createTime = std::chrono::steady_clock::now();
        entry.memoryUsage = entry.data ? entry.data->getMemoryUsage() : 0;

        if (!name.empty()) {
            nameToIndex_[name] = globalIdx;
        }
        if (!debugTag.empty()) {
            debugTagToIndex_[debugTag] = globalIdx;
        }

        totalCreated_++;
        size_t memIncrease = entry.memoryUsage;
        size_t newTotal = currentMemoryUsage_.fetch_add(memIncrease) + memIncrease;
        size_t oldMax = maxMemoryUsage_.load();
        while (newTotal > oldMax) {
            if (maxMemoryUsage_.compare_exchange_weak(oldMax, newTotal)) {
                break;
            }
        }
        return Handle::Create(globalIdx, entry.generation);
    }

    template<typename HandleType, typename ResourceType>
    template<typename... Args>
    std::vector<typename TypedResourceStorage<HandleType, ResourceType>::Handle>
        TypedResourceStorage<HandleType, ResourceType>::createMultiple(uint32_t count, Args&&... args) {
        std::vector<Handle> handles;
        handles.reserve(count);

        for (uint32_t i = 0; i < count; ++i) {
            handles.push_back(create(std::forward<Args>(args)...));
        }

        return handles;
    }

    template<typename HandleType, typename ResourceType>
    ResourceType* TypedResourceStorage<HandleType, ResourceType>::getData(Handle handle) {
        if (!handle.isValid()) return nullptr;

        //std::lock_guard<std::mutex> lock(mutex_);
        return getEntryData(handle);
    }

    template<typename HandleType, typename ResourceType>
    const ResourceType* TypedResourceStorage<HandleType, ResourceType>::getData(Handle handle) const {
        return const_cast<TypedResourceStorage*>(this)->getData(handle);
    }

    template<typename HandleType, typename ResourceType>
    IResource* TypedResourceStorage<HandleType, ResourceType>::getResource(Handle handle) {
        return static_cast<IResource*>(getData(handle));
    }

    template<typename HandleType, typename ResourceType>
    const IResource* TypedResourceStorage<HandleType, ResourceType>::getResource(Handle handle) const {
        return static_cast<const IResource*>(getData(handle));
    }

    template<typename HandleType, typename ResourceType>
    std::optional<std::reference_wrapper<ResourceType>>
        TypedResourceStorage<HandleType, ResourceType>::tryGetData(Handle handle) {
        auto* data = getData(handle);
        if (data) {
            return std::ref(*data);
        }
        return std::nullopt;
    }

    template<typename HandleType, typename ResourceType>
    void TypedResourceStorage<HandleType, ResourceType>::clear() {
        for (auto& chunk : chunks_) {
            for (auto& entry : chunk.entries) {
                if (entry.alive && entry.data) {
                    entry.data->release();  // 释放 GPU 资源
                    entry.data.reset();     // 销毁对象，避免析构时再次调用 release
                }
            }
        }
        chunks_.clear();
        nameToIndex_.clear();
        debugTagToIndex_.clear();
        totalCreated_ = 0;
        totalDestroyed_ = 0;
        maxMemoryUsage_ = 0;
    }

    template<typename HandleType, typename ResourceType>
    size_t TypedResourceStorage<HandleType, ResourceType>::size() const {
        //std::lock_guard<std::mutex> lock(mutex_);
        return totalCreated_ - totalDestroyed_;
    }

    template<typename HandleType, typename ResourceType>
    bool TypedResourceStorage<HandleType, ResourceType>::empty() const {
        return size() == 0;
    }

    template<typename HandleType, typename ResourceType>
    typename TypedResourceStorage<HandleType, ResourceType>::Handle
        TypedResourceStorage<HandleType, ResourceType>::findByName(const std::string& name) const {
        //std::lock_guard<std::mutex> lock(mutex_);

        auto it = nameToIndex_.find(name);
        if (it == nameToIndex_.end()) return Handle::Null();

        return getHandleFromIndex(it->second);
    }

    template<typename HandleType, typename ResourceType>
    typename TypedResourceStorage<HandleType, ResourceType>::Handle
        TypedResourceStorage<HandleType, ResourceType>::findByDebugTag(const std::string& debugTag) const {
        //std::lock_guard<std::mutex> lock(mutex_);

        auto it = debugTagToIndex_.find(debugTag);
        if (it == debugTagToIndex_.end()) return Handle::Null();

        return getHandleFromIndex(it->second);
    }

    template<typename HandleType, typename ResourceType>
    bool TypedResourceStorage<HandleType, ResourceType>::addRef(Handle handle) {
        if (!handle.isValid()) return false;

        //std::lock_guard<std::mutex> lock(mutex_);

        Entry* entry = getEntry(handle);
        if (!entry) return false;

        entry->refCount++;
        return true;
    }

    template<typename HandleType, typename ResourceType>
    bool TypedResourceStorage<HandleType, ResourceType>::release(Handle handle) {
        if (!handle.isValid()) return false;

        //std::lock_guard<std::mutex> lock(mutex_);

        Entry* entry = getEntry(handle);
        if (!entry) return false;

        if (entry->refCount > 0) {
            entry->refCount--;
        }

        // 引用计数为0，销毁资源
        if (entry->refCount == 0) {
            destroyEntry(entry);
        }

        return true;
    }

    template<typename HandleType, typename ResourceType>
    bool TypedResourceStorage<HandleType, ResourceType>::destroy(Handle handle) {
        if (!handle.isValid()) return false;

        //std::lock_guard<std::mutex> lock(mutex_);

        Entry* entry = getEntry(handle);
        if (!entry) return false;

        destroyEntry(entry);
        return true;
    }

    template<typename HandleType, typename ResourceType>
    template<typename Func>
    void TypedResourceStorage<HandleType, ResourceType>::forEach(Func&& func) {
        //std::lock_guard<std::mutex> lock(mutex_);

        for (size_t chunkIdx = 0; chunkIdx < chunks_.size(); ++chunkIdx) {
            auto& chunk = chunks_[chunkIdx];
            for (uint32_t i = 0; i < CHUNK_SIZE; ++i) {
                Entry& entry = chunk.entries[i];
                if (entry.alive && entry.data) {
                    uint32_t globalIdx = static_cast<uint32_t>(chunkIdx * CHUNK_SIZE + i);
                    Handle handle = getHandleFromIndex(globalIdx);
                    func(handle, *entry.data);
                }
            }
        }
    }

    template<typename HandleType, typename ResourceType>
    template<typename Func>
    void TypedResourceStorage<HandleType, ResourceType>::forEach(Func&& func) const {
        //std::lock_guard<std::mutex> lock(mutex_);

        for (size_t chunkIdx = 0; chunkIdx < chunks_.size(); ++chunkIdx) {
            const auto& chunk = chunks_[chunkIdx];
            for (uint32_t i = 0; i < CHUNK_SIZE; ++i) {
                const Entry& entry = chunk.entries[i];
                if (entry.alive && entry.data) {
                    uint32_t globalIdx = static_cast<uint32_t>(chunkIdx * CHUNK_SIZE + i);
                    Handle handle = getHandleFromIndex(globalIdx);
                    func(handle, *entry.data);
                }
            }
        }
    }

    template<typename HandleType, typename ResourceType>
    template<typename Func, typename Filter>
    void TypedResourceStorage<HandleType, ResourceType>::forEachFiltered(Func&& func, Filter&& filter) {
        //std::lock_guard<std::mutex> lock(mutex_);

        for (size_t chunkIdx = 0; chunkIdx < chunks_.size(); ++chunkIdx) {
            auto& chunk = chunks_[chunkIdx];
            for (uint32_t i = 0; i < CHUNK_SIZE; ++i) {
                Entry& entry = chunk.entries[i];
                if (entry.alive && entry.data) {
                    uint32_t globalIdx = static_cast<uint32_t>(chunkIdx * CHUNK_SIZE + i);
                    Handle handle = getHandleFromIndex(globalIdx);
                    if (filter(handle, entry)) {
                        func(handle, *entry.data);
                    }
                }
            }
        }
    }

    template<typename HandleType, typename ResourceType>
    size_t TypedResourceStorage<HandleType, ResourceType>::getTotalMemoryUsage() const {
        //std::lock_guard<std::mutex> lock(mutex_);

        size_t total = 0;
        for (const auto& chunk : chunks_) {
            for (const auto& entry : chunk.entries) {
                if (entry.alive && entry.data) {
                    total += entry.memoryUsage;
                }
            }
        }
        return total;
    }

    template<typename HandleType, typename ResourceType>
    std::vector<typename TypedResourceStorage<HandleType, ResourceType>::Handle>
        TypedResourceStorage<HandleType, ResourceType>::getAllHandles() const {
        //std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Handle> handles;

        for (size_t chunkIdx = 0; chunkIdx < chunks_.size(); ++chunkIdx) {
            auto& chunk = chunks_[chunkIdx];
            for (uint32_t i = 0; i < CHUNK_SIZE; ++i) {
                const Entry& entry = chunk.entries[i];
                if (entry.alive && entry.data) {
                    uint32_t globalIdx = static_cast<uint32_t>(chunkIdx * CHUNK_SIZE + i);
                    handles.push_back(getHandleFromIndex(globalIdx));
                }
            }
        }

        return handles;
    }

    template<typename HandleType, typename ResourceType>
    std::optional<typename TypedResourceStorage<HandleType, ResourceType>::EntryInfo>
        TypedResourceStorage<HandleType, ResourceType>::getEntryInfo(Handle handle) const {
        //std::lock_guard<std::mutex> lock(mutex_);

        const Entry* entry = getEntry(handle);
        if (!entry) return std::nullopt;

        return EntryInfo{
            handle,
            entry->name,
            entry->debugTag,
            entry->refCount,
            entry->memoryUsage,
            entry->createTime,
            entry->data.get()
        };
    }

    template<typename HandleType, typename ResourceType>
    typename TypedResourceStorage<HandleType, ResourceType>::Statistics
        TypedResourceStorage<HandleType, ResourceType>::getStatistics() const {
        //std::lock_guard<std::mutex> lock(mutex_);

        Statistics stats;
        stats.totalCreated = totalCreated_;
        stats.totalDestroyed = totalDestroyed_;
        stats.currentCount = totalCreated_ - totalDestroyed_;

        // 直接计算内存，避免调用 getTotalMemoryUsage()
        stats.totalMemoryUsage = 0;
        for (const auto& chunk : chunks_) {
            for (const auto& entry : chunk.entries) {
                if (entry.alive && entry.data) {
                    stats.totalMemoryUsage += entry.memoryUsage;
                }
            }
        }

        stats.maxMemoryUsage = maxMemoryUsage_.load();
        stats.chunkCount = chunks_.size();

        return stats;
    }

    template<typename HandleType, typename ResourceType>
    void TypedResourceStorage<HandleType, ResourceType>::setMemoryWarningThreshold(size_t threshold) {
        memoryWarningThreshold_ = threshold;
    }

    template<typename HandleType, typename ResourceType>
    bool TypedResourceStorage<HandleType, ResourceType>::isMemoryUsageExceeded() const {
        return getTotalMemoryUsage() > memoryWarningThreshold_;
    }

    template<typename HandleType, typename ResourceType>
    std::optional<typename TypedResourceStorage<HandleType, ResourceType>::Handle>
        TypedResourceStorage<HandleType, ResourceType>::getOldestResource() const {
        //std::lock_guard<std::mutex> lock(mutex_);

        std::optional<Handle> oldest;
        std::chrono::steady_clock::time_point oldestTime =
            std::chrono::steady_clock::time_point::max();

        for (size_t chunkIdx = 0; chunkIdx < chunks_.size(); ++chunkIdx) {
            auto& chunk = chunks_[chunkIdx];
            for (uint32_t i = 0; i < CHUNK_SIZE; ++i) {
                const Entry& entry = chunk.entries[i];
                if (entry.alive && entry.data && entry.refCount == 0) {
                    if (entry.createTime < oldestTime) {
                        oldestTime = entry.createTime;
                        uint32_t globalIdx = static_cast<uint32_t>(chunkIdx * CHUNK_SIZE + i);
                        oldest = getHandleFromIndex(globalIdx);
                    }
                }
            }
        }

        return oldest;
    }

    // ==================== 私有方法实现 ====================

    template<typename HandleType, typename ResourceType>
    typename TypedResourceStorage<HandleType, ResourceType>::Entry*
        TypedResourceStorage<HandleType, ResourceType>::getEntry(Handle handle) {
        if (!handle.isValid()) return nullptr;

        uint32_t chunkIdx = handle.getIndex() / CHUNK_SIZE;
        uint32_t indexInChunk = handle.getIndex() % CHUNK_SIZE;

        if (chunkIdx >= chunks_.size()) return nullptr;

        Entry& entry = chunks_[chunkIdx].entries[indexInChunk];
        if (!entry.alive || entry.generation != handle.getGeneration()) {
            return nullptr;
        }

        return &entry;
    }

    template<typename HandleType, typename ResourceType>
    const typename TypedResourceStorage<HandleType, ResourceType>::Entry*
        TypedResourceStorage<HandleType, ResourceType>::getEntry(Handle handle) const {
        return const_cast<TypedResourceStorage*>(this)->getEntry(handle);
    }

    template<typename HandleType, typename ResourceType>
    ResourceType* TypedResourceStorage<HandleType, ResourceType>::getEntryData(Handle handle) {
        Entry* entry = getEntry(handle);
        return entry ? entry->data.get() : nullptr;
    }

    template<typename HandleType, typename ResourceType>
    typename TypedResourceStorage<HandleType, ResourceType>::Handle
        TypedResourceStorage<HandleType, ResourceType>::getHandleFromIndex(uint32_t globalIdx) const {
        uint32_t chunkIdx = globalIdx / CHUNK_SIZE;
        uint32_t indexInChunk = globalIdx % CHUNK_SIZE;

        if (chunkIdx >= chunks_.size()) return Handle::Null();

        const Entry& entry = chunks_[chunkIdx].entries[indexInChunk];
        if (!entry.alive) return Handle::Null();

        return Handle::Create(globalIdx, entry.generation);
    }

    template<typename HandleType, typename ResourceType>
    void TypedResourceStorage<HandleType, ResourceType>::destroyEntry(Entry* entry) {
        if (entry->data) {
            // 先获取内存使用量，然后再释放
            size_t memoryToFree = entry->memoryUsage;
            entry->data->release();
        }

        // 从名称映射中移除
        if (!entry->name.empty()) {
            nameToIndex_.erase(entry->name);
        }

        // 从调试标签映射中移除
        if (!entry->debugTag.empty()) {
            debugTagToIndex_.erase(entry->debugTag);
        }

        // 更新内存使用统计
        entry->memoryUsage = 0;

        // 标记为死亡
        entry->alive = false;
        entry->data.reset();
        entry->name.clear();
        entry->debugTag.clear();
        entry->refCount = 0;

        totalDestroyed_++;

        if (chunks_.size() > 1 && totalDestroyed_ > totalCreated_ / 2) {
            while (!chunks_.empty()) {
                auto& lastChunk = chunks_.back();
                bool empty = true;

                for (const auto& e : lastChunk.entries) {
                    if (e.alive) {
                        empty = false;
                        break;
                    }
                }

                if (empty) {
                    chunks_.pop_back();
                }
                else {
                    break;
                }
            }
        }
    }

    template<typename HandleType, typename ResourceType>
    void TypedResourceStorage<HandleType, ResourceType>::tryShrink() {
        // 如果最后一个chunk完全为空，则移除它
        while (!chunks_.empty()) {
            auto& lastChunk = chunks_.back();
            bool empty = true;

            for (const auto& entry : lastChunk.entries) {
                if (entry.alive) {
                    empty = false;
                    break;
                }
            }

            if (empty) {
                chunks_.pop_back();
            }
            else {
                break;
            }
        }
    }

    template<typename Handle>
    struct HandleTraits;

    // BufferHandle
    template<>
    struct HandleTraits<RHI::BufferHandle> {
        using ResourceType = RHI::RHIBuffer;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::BufferHandle handle) {
            return mgr->getBuffer(handle);
        }
        static RHI::BufferHandle create(RHI::ResourceManager* mgr, const RHI::BufferDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createBuffer(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::BufferHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // TextureHandle
    template<>
    struct HandleTraits<RHI::TextureHandle> {
        using ResourceType = RHI::RHITexture;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::TextureHandle handle) {
            return mgr->getTexture(handle);
        }
        static RHI::TextureHandle create(RHI::ResourceManager* mgr, const RHI::TextureDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createTexture(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::TextureHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // PipelineHandle
    template<>
    struct HandleTraits<RHI::PipelineHandle> {
        using ResourceType = RHI::RHIPipeline;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::PipelineHandle handle) {
            return mgr->getPipeline(handle);
        }
        static RHI::PipelineHandle create(RHI::ResourceManager* mgr, const RHI::GraphicsPipelineDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createGraphicsPipeline(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        // 注意：计算管线也返回 PipelineHandle，但描述符类型不同，可以再增加一个 createCompute 特化，或使用 if constexpr
        // 这里为了简化，只提供图形管线的创建，计算管线单独处理（或通过另一个特化）
        static bool destroy(RHI::ResourceManager* mgr, RHI::PipelineHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // PipelineLayoutHandle
    template<>
    struct HandleTraits<RHI::PipelineLayoutHandle> {
        using ResourceType = RHI::RHIPipelineLayout;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::PipelineLayoutHandle handle) {
            return mgr->getPipelineLayout(handle);
        }
        static RHI::PipelineLayoutHandle create(RHI::ResourceManager* mgr, const RHI::PipelineLayoutDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createPipelineLayout(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::PipelineLayoutHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // ShaderHandle
    template<>
    struct HandleTraits<RHI::ShaderHandle> {
        using ResourceType = RHI::RHIShaderModule;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::ShaderHandle handle) {
            return mgr->getShader(handle);
        }
        static RHI::ShaderHandle create(RHI::ResourceManager* mgr, const RHI::ShaderModuleDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createShader(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::ShaderHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // SamplerHandle
    template<>
    struct HandleTraits<RHI::SamplerHandle> {
        using ResourceType = RHI::RHISampler;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::SamplerHandle handle) {
            return mgr->getSampler(handle);
        }
        static RHI::SamplerHandle create(RHI::ResourceManager* mgr, const RHI::SamplerDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createSampler(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::SamplerHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // RenderPassHandle
    template<>
    struct HandleTraits<RHI::RenderPassHandle> {
        using ResourceType = RHI::RHIRenderPass;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::RenderPassHandle handle) {
            return mgr->getRenderPass(handle);
        }
        static RHI::RenderPassHandle create(RHI::ResourceManager* mgr, const RHI::RenderPassDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createRenderPass(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::RenderPassHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // FramebufferHandle
    template<>
    struct HandleTraits<RHI::FramebufferHandle> {
        using ResourceType = RHI::RHIFramebuffer;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::FramebufferHandle handle) {
            return mgr->getFramebuffer(handle);
        }
        static RHI::FramebufferHandle create(RHI::ResourceManager* mgr, const RHI::FramebufferDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createFramebuffer(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::FramebufferHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // DescriptorSetHandle
    template<>
    struct HandleTraits<RHI::DescriptorSetHandle> {
        using ResourceType = RHI::RHIDescriptorSet;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::DescriptorSetHandle handle) {
            return mgr->getDescriptorSet(handle);
        }
        static RHI::DescriptorSetHandle create(RHI::ResourceManager* mgr, const RHI::DescriptorSetDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createDescriptorSet(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::DescriptorSetHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // DescriptorPoolHandle
    template<>
    struct HandleTraits<RHI::DescriptorPoolHandle> {
        using ResourceType = RHI::RHIDescriptorPool;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::DescriptorPoolHandle handle) {
            return mgr->getDescriptorPool(handle);
        }
        static RHI::DescriptorPoolHandle create(RHI::ResourceManager* mgr, const RHI::DescriptorPoolDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createDescriptorPool(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::DescriptorPoolHandle handle) {
            return mgr->destroy(handle);
        }
    };

    // DescriptorSetLayoutHandle
    template<>
    struct HandleTraits<RHI::DescriptorSetLayoutHandle> {
        using ResourceType = RHI::RHIDescriptorSetLayout;
        static ResourceType* get(RHI::ResourceManager* mgr, RHI::DescriptorSetLayoutHandle handle) {
            return mgr->getDescriptorSetLayout(handle);
        }
        static RHI::DescriptorSetLayoutHandle create(RHI::ResourceManager* mgr, const RHI::DescriptorSetLayoutDesc& desc,
            const std::string& name = "", const std::string& debugTag = "") {
            return mgr->createDescriptorSetLayout(desc, name.empty() ? desc.debugName : name, debugTag);
        }
        static bool destroy(RHI::ResourceManager* mgr, RHI::DescriptorSetLayoutHandle handle) {
            return mgr->destroy(handle);
        }
    };

} // namespace StarryEngine::RHI