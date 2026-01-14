#pragma once
#include "RHI_HANDLES_SYSTEM.hpp"
#include "RHI_TYPES.hpp"
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

namespace StarryEngine::RHI {

    // ==================== 资源接口 ====================

    class IResource {
    public:
        virtual ~IResource() = default;

        virtual void release() = 0;
        virtual bool isValid() const = 0;
        virtual void* getNativeHandle() const = 0;
        virtual size_t getMemoryUsage() const = 0;
        virtual const char* getTypeName() const = 0;

        virtual bool operator==(const IResource& other) const {
            return getNativeHandle() == other.getNativeHandle();
        }
    };

    // 具体资源接口定义
    class IBuffer : public IResource {};
    class ITexture : public IResource {};
    class IPipeline : public IResource {};
    class IPipelineLayout : public IResource {};
    class IShader : public IResource {};
    class ISampler : public IResource {};
    class IRenderPass : public IResource {};
    class IFramebuffer : public IResource {};
    class IDescriptorSet : public IResource {};
    class IDescriptorPool : public IResource {};
    class ICommandBuffer : public IResource {};
    class ICommandPool : public IResource {};
    class IFence : public IResource {};
    class ISemaphore : public IResource {};
    class IEvent : public IResource {};
    class IQueryPool : public IResource {};
    class ISwapChain : public IResource {};

    // ==================== 资源工厂接口 ====================

    class IResourceFactory {
    public:
        virtual ~IResourceFactory() = default;

        virtual std::unique_ptr<IBuffer> createBuffer(const BufferDesc& desc) = 0;
        virtual std::unique_ptr<ITexture> createTexture(const TextureDesc& desc) = 0;
        virtual std::unique_ptr<IPipeline> createPipeline(const GraphicsPipelineDesc& desc) = 0;
        virtual std::unique_ptr<IPipelineLayout> createPipelineLayout(const PipelineLayoutDesc& desc) = 0;
        virtual std::unique_ptr<IShader> createShader(const ShaderModuleDesc& desc) = 0;
        virtual std::unique_ptr<ISampler> createSampler(const SamplerDesc& desc) = 0;
        virtual std::unique_ptr<IRenderPass> createRenderPass(const RenderPassDesc& desc) = 0;
        virtual std::unique_ptr<IFramebuffer> createFramebuffer(const FramebufferDesc& desc) = 0;
        virtual std::unique_ptr<IDescriptorSet> createDescriptorSet(const DescriptorSetDesc& desc) = 0;
        virtual std::unique_ptr<IDescriptorPool> createDescriptorPool(const DescriptorPoolDesc& desc) = 0;
        virtual std::unique_ptr<ICommandBuffer> createCommandBuffer(const CommandBufferDesc& desc) = 0;
        virtual std::unique_ptr<ICommandPool> createCommandPool(const CommandPoolDesc& desc) = 0;
        virtual std::unique_ptr<IFence> createFence(const FenceDesc& desc) = 0;
        virtual std::unique_ptr<ISemaphore> createSemaphore(const SemaphoreDesc& desc) = 0;
        virtual std::unique_ptr<IEvent> createEvent(const EventDesc& desc) = 0;
        virtual std::unique_ptr<IQueryPool> createQueryPool(const QueryPoolDesc& desc) = 0;
        virtual std::unique_ptr<ISwapChain> createSwapChain(const SwapChainDesc& desc) = 0;
    };

    // ==================== 分类型资源存储 ====================

    template<typename HandleType, typename ResourceType>
    class TypedResourceStorage {
        static_assert(std::is_base_of<IResource, ResourceType>::value,
            "ResourceType must inherit from IResource");
    public:
        using Handle = HandleType;
        using ResourcePtr = std::unique_ptr<ResourceType>;

        struct Entry {
            ResourcePtr data;
            std::string name;
            uint32_t generation = 1;
            uint32_t refCount = 0;
            bool alive = false;

            bool isValid() const { return alive && data && data->isValid(); }
        };

        TypedResourceStorage() = default;
        ~TypedResourceStorage() { clear(); }

        // 创建资源
        Handle create(ResourcePtr data, const std::string& name = "") {
            std::lock_guard<std::mutex> lock(mutex_);

            // 查找空闲槽位
            for (size_t chunkIdx = 0; chunkIdx < chunks_.size(); ++chunkIdx) {
                auto& chunk = chunks_[chunkIdx];
                for (uint32_t i = 0; i < CHUNK_SIZE; ++i) {
                    if (!chunk.entries[i].alive) {
                        uint32_t globalIdx = static_cast<uint32_t>(chunkIdx * CHUNK_SIZE + i);

                        Entry& entry = chunk.entries[i];
                        entry.data = std::move(data);
                        entry.name = name;
                        entry.generation++;
                        entry.refCount = 1;
                        entry.alive = true;

                        // 添加到名称映射
                        if (!name.empty()) {
                            nameToIndex_[name] = globalIdx;
                        }

                        // 创建句柄
                        return Handle::Create(
                            static_cast<uint8_t>(Handle::CATEGORY),
                            globalIdx,
                            entry.generation
                        );
                    }
                }
            }

            // 没有空闲位置，创建新chunk
            chunks_.push_back(Chunk{});
            return create(std::move(data), name);
        }

        // 获取资源数据
        ResourceType* getData(Handle handle) {
            if (!handle.isValid()) return nullptr;

            std::lock_guard<std::mutex> lock(mutex_);

            uint32_t chunkIdx = handle.getIndex() / CHUNK_SIZE;
            uint32_t indexInChunk = handle.getIndex() % CHUNK_SIZE;

            if (chunkIdx >= chunks_.size()) return nullptr;

            Entry& entry = chunks_[chunkIdx].entries[indexInChunk];
            if (!entry.alive || entry.generation != handle.getGeneration()) {
                return nullptr;
            }

            return entry.data.get();
        }

        const ResourceType* getData(Handle handle) const {
            return const_cast<TypedResourceStorage*>(this)->getData(handle);
        }

        // 获取资源数据（接口版本）
        IResource* getResource(Handle handle) {
            return static_cast<IResource*>(getData(handle));
        }

        const IResource* getResource(Handle handle) const {
            return static_cast<const IResource*>(getData(handle));
        }

        // 按名称查找
        Handle findByName(const std::string& name) const {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = nameToIndex_.find(name);
            if (it == nameToIndex_.end()) return Handle::Null();

            uint32_t globalIdx = it->second;
            uint32_t chunkIdx = globalIdx / CHUNK_SIZE;
            uint32_t indexInChunk = globalIdx % CHUNK_SIZE;

            if (chunkIdx >= chunks_.size()) return Handle::Null();

            const Entry& entry = chunks_[chunkIdx].entries[indexInChunk];
            if (!entry.alive) return Handle::Null();

            return Handle::Create(
                static_cast<uint8_t>(Handle::CATEGORY),
                globalIdx,
                entry.generation
            );
        }

        // 增加引用计数
        bool addRef(Handle handle) {
            if (!handle.isValid()) return false;

            std::lock_guard<std::mutex> lock(mutex_);

            Entry* entry = getEntry(handle);
            if (!entry) return false;

            entry->refCount++;
            return true;
        }

        // 减少引用计数
        bool release(Handle handle) {
            if (!handle.isValid()) return false;

            std::lock_guard<std::mutex> lock(mutex_);

            Entry* entry = getEntry(handle);
            if (!entry) return false;

            if (entry->refCount > 0) {
                entry->refCount--;
            }

            // 引用计数为0，销毁资源
            if (entry->refCount == 0) {
                if (entry->data) {
                    entry->data->release();
                }

                // 从名称映射中移除
                if (!entry->name.empty()) {
                    nameToIndex_.erase(entry->name);
                }

                // 标记为死亡
                entry->alive = false;
                entry->data.reset();
                entry->name.clear();
            }

            return true;
        }

        // 强制销毁（忽略引用计数）
        bool destroy(Handle handle) {
            if (!handle.isValid()) return false;

            std::lock_guard<std::mutex> lock(mutex_);

            Entry* entry = getEntry(handle);
            if (!entry) return false;

            // 释放资源
            if (entry->data) {
                entry->data->release();
            }

            // 从名称映射中移除
            if (!entry->name.empty()) {
                nameToIndex_.erase(entry->name);
            }

            // 标记为死亡
            entry->alive = false;
            entry->data.reset();
            entry->name.clear();
            entry->refCount = 0;

            return true;
        }

        // 遍历所有资源
        template<typename Func>
        void forEach(Func&& func) {
            std::lock_guard<std::mutex> lock(mutex_);

            for (size_t chunkIdx = 0; chunkIdx < chunks_.size(); ++chunkIdx) {
                auto& chunk = chunks_[chunkIdx];
                for (uint32_t i = 0; i < CHUNK_SIZE; ++i) {
                    Entry& entry = chunk.entries[i];
                    if (entry.alive && entry.data) {
                        uint32_t globalIdx = static_cast<uint32_t>(chunkIdx * CHUNK_SIZE + i);
                        Handle handle = Handle::Create(
                            static_cast<uint8_t>(Handle::CATEGORY),
                            globalIdx,
                            entry.generation
                        );
                        func(handle, *entry.data);
                    }
                }
            }
        }

        // 清理所有资源
        void clear() {
            std::lock_guard<std::mutex> lock(mutex_);

            for (auto& chunk : chunks_) {
                for (auto& entry : chunk.entries) {
                    if (entry.alive && entry.data) {
                        entry.data->release();
                    }
                }
            }

            chunks_.clear();
            nameToIndex_.clear();
        }

        // 统计
        size_t size() const {
            std::lock_guard<std::mutex> lock(mutex_);

            size_t count = 0;
            for (const auto& chunk : chunks_) {
                for (const auto& entry : chunk.entries) {
                    if (entry.alive) count++;
                }
            }
            return count;
        }

        bool empty() const {
            return size() == 0;
        }

        // 获取内存使用情况
        size_t getTotalMemoryUsage() const {
            std::lock_guard<std::mutex> lock(mutex_);

            size_t total = 0;
            for (const auto& chunk : chunks_) {
                for (const auto& entry : chunk.entries) {
                    if (entry.alive && entry.data) {
                        total += entry.data->getMemoryUsage();
                    }
                }
            }
            return total;
        }

        // 获取所有活跃句柄
        std::vector<Handle> getAllHandles() const {
            std::lock_guard<std::mutex> lock(mutex_);
            std::vector<Handle> handles;

            for (size_t chunkIdx = 0; chunkIdx < chunks_.size(); ++chunkIdx) {
                auto& chunk = chunks_[chunkIdx];
                for (uint32_t i = 0; i < CHUNK_SIZE; ++i) {
                    const Entry& entry = chunk.entries[i];
                    if (entry.alive && entry.data) {
                        uint32_t globalIdx = static_cast<uint32_t>(chunkIdx * CHUNK_SIZE + i);
                        handles.push_back(Handle::Create(
                            static_cast<uint8_t>(Handle::CATEGORY),
                            globalIdx,
                            entry.generation
                        ));
                    }
                }
            }

            return handles;
        }

    private:
        struct Chunk {
            std::array<Entry, CHUNK_SIZE> entries;
        };

        static constexpr size_t CHUNK_SIZE = 256;

        Entry* getEntry(Handle handle) {
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

    private:
        mutable std::mutex mutex_;
        std::vector<Chunk> chunks_;
        std::unordered_map<std::string, uint32_t> nameToIndex_;
    };

    // ==================== 全局资源管理器 ====================

    class ResourceManager {
    public:
        ResourceManager(std::unique_ptr<IResourceFactory> factory)
            : factory_(std::move(factory)) {
            assert(factory_ != nullptr && "Resource factory must be provided");
        }

        // 禁止拷贝和移动
        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;

        // 创建各种资源
        BufferHandle createBuffer(const BufferDesc& desc, const std::string& name = "") {
            auto resource = factory_->createBuffer(desc);
            return buffers_.create(std::move(resource), name);
        }

        TextureHandle createTexture(const TextureDesc& desc, const std::string& name = "") {
            auto resource = factory_->createTexture(desc);
            return textures_.create(std::move(resource), name);
        }

        PipelineHandle createPipeline(const GraphicsPipelineDesc& desc, const std::string& name = "") {
            auto resource = factory_->createPipeline(desc);
            return pipelines_.create(std::move(resource), name);
        }

        PipelineLayoutHandle createPipelineLayout(const PipelineLayoutDesc& desc, const std::string& name = "") {
            auto resource = factory_->createPipelineLayout(desc);
            return pipelineLayouts_.create(std::move(resource), name);
        }

        ShaderHandle createShader(const ShaderModuleDesc& desc, const std::string& name = "") {
            auto resource = factory_->createShader(desc);
            return shaders_.create(std::move(resource), name);
        }

        SamplerHandle createSampler(const SamplerDesc& desc, const std::string& name = "") {
            auto resource = factory_->createSampler(desc);
            return samplers_.create(std::move(resource), name);
        }

        RenderPassHandle createRenderPass(const RenderPassDesc& desc, const std::string& name = "") {
            auto resource = factory_->createRenderPass(desc);
            return renderPasses_.create(std::move(resource), name);
        }

        FramebufferHandle createFramebuffer(const FramebufferDesc& desc, const std::string& name = "") {
            auto resource = factory_->createFramebuffer(desc);
            return framebuffers_.create(std::move(resource), name);
        }

        // 其他资源的创建方法类似...

        // 获取各种资源存储
        TypedResourceStorage<BufferHandle, IBuffer>& getBufferStorage() { return buffers_; }
        TypedResourceStorage<TextureHandle, ITexture>& getTextureStorage() { return textures_; }
        TypedResourceStorage<PipelineHandle, IPipeline>& getPipelineStorage() { return pipelines_; }
        TypedResourceStorage<PipelineLayoutHandle, IPipelineLayout>& getPipelineLayoutStorage() { return pipelineLayouts_; }
        TypedResourceStorage<ShaderHandle, IShader>& getShaderStorage() { return shaders_; }
        TypedResourceStorage<SamplerHandle, ISampler>& getSamplerStorage() { return samplers_; }
        TypedResourceStorage<RenderPassHandle, IRenderPass>& getRenderPassStorage() { return renderPasses_; }
        TypedResourceStorage<FramebufferHandle, IFramebuffer>& getFramebufferStorage() { return framebuffers_; }

        // 获取资源数据
        IBuffer* getBuffer(BufferHandle handle) { return buffers_.getData(handle); }
        ITexture* getTexture(TextureHandle handle) { return textures_.getData(handle); }
        IPipeline* getPipeline(PipelineHandle handle) { return pipelines_.getData(handle); }
        IPipelineLayout* getPipelineLayout(PipelineLayoutHandle handle) { return pipelineLayouts_.getData(handle); }
        IShader* getShader(ShaderHandle handle) { return shaders_.getData(handle); }
        ISampler* getSampler(SamplerHandle handle) { return samplers_.getData(handle); }
        IRenderPass* getRenderPass(RenderPassHandle handle) { return renderPasses_.getData(handle); }
        IFramebuffer* getFramebuffer(FramebufferHandle handle) { return framebuffers_.getData(handle); }

        // 按名称查找
        BufferHandle findBufferByName(const std::string& name) { return buffers_.findByName(name); }
        TextureHandle findTextureByName(const std::string& name) { return textures_.findByName(name); }
        PipelineHandle findPipelineByName(const std::string& name) { return pipelines_.findByName(name); }

        // 资源引用计数管理
        bool addRef(BufferHandle handle) { return buffers_.addRef(handle); }
        bool release(BufferHandle handle) { return buffers_.release(handle); }
        bool destroy(BufferHandle handle) { return buffers_.destroy(handle); }

        bool addRef(TextureHandle handle) { return textures_.addRef(handle); }
        bool release(TextureHandle handle) { return textures_.release(handle); }
        bool destroy(TextureHandle handle) { return textures_.destroy(handle); }

        // 清理所有资源
        void clearAll() {
            buffers_.clear();
            textures_.clear();
            pipelines_.clear();
            pipelineLayouts_.clear();
            shaders_.clear();
            samplers_.clear();
            renderPasses_.clear();
            framebuffers_.clear();
        }

        // 统计所有资源
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
            size_t totalMemoryUsage = 0;
            size_t bufferMemoryUsage = 0;
            size_t textureMemoryUsage = 0;
        };

        Statistics getStatistics() const {
            Statistics stats;
            stats.bufferCount = buffers_.size();
            stats.textureCount = textures_.size();
            stats.pipelineCount = pipelines_.size();
            stats.pipelineLayoutCount = pipelineLayouts_.size();
            stats.shaderCount = shaders_.size();
            stats.samplerCount = samplers_.size();
            stats.renderPassCount = renderPasses_.size();
            stats.framebufferCount = framebuffers_.size();

            stats.totalResources = stats.bufferCount + stats.textureCount + stats.pipelineCount +
                stats.pipelineLayoutCount + stats.shaderCount + stats.samplerCount +
                stats.renderPassCount + stats.framebufferCount;

            stats.bufferMemoryUsage = buffers_.getTotalMemoryUsage();
            stats.textureMemoryUsage = textures_.getTotalMemoryUsage();
            stats.totalMemoryUsage = stats.bufferMemoryUsage + stats.textureMemoryUsage;

            return stats;
        }

        // 调试输出
        void dumpStatistics() const {
            Statistics stats = getStatistics();

            std::cout << "=== RHI Resource Statistics ===" << std::endl;
            std::cout << "Total Resources: " << stats.totalResources << std::endl;
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
            std::cout << "Total Memory: " << stats.totalMemoryUsage / 1024.0 / 1024.0 << " MB" << std::endl;
        }

        // 获取工厂
        IResourceFactory* getFactory() const { return factory_.get(); }

    private:
        std::unique_ptr<IResourceFactory> factory_;

        // 分类型存储
        TypedResourceStorage<BufferHandle, IBuffer> buffers_;
        TypedResourceStorage<TextureHandle, ITexture> textures_;
        TypedResourceStorage<PipelineHandle, IPipeline> pipelines_;
        TypedResourceStorage<PipelineLayoutHandle, IPipelineLayout> pipelineLayouts_;
        TypedResourceStorage<ShaderHandle, IShader> shaders_;
        TypedResourceStorage<SamplerHandle, ISampler> samplers_;
        TypedResourceStorage<RenderPassHandle, IRenderPass> renderPasses_;
        TypedResourceStorage<FramebufferHandle, IFramebuffer> framebuffers_;
        // 其他资源存储类似...
    };

    // ==================== 便捷访问宏 ====================

#define RHI_BUFFERS ResourceManager::getInstance().getBufferStorage()
#define RHI_TEXTURES ResourceManager::getInstance().getTextureStorage()
#define RHI_PIPELINES ResourceManager::getInstance().getPipelineStorage()
#define RHI_PIPELINE_LAYOUTS ResourceManager::getInstance().getPipelineLayoutStorage()
#define RHI_SHADERS ResourceManager::getInstance().getShaderStorage()
#define RHI_SAMPLERS ResourceManager::getInstance().getSamplerStorage()

} // namespace StarryEngine::RHI