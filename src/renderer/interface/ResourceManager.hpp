#pragma once
#include "RHI_HANDLES_SYSTEM.hpp"
#include "RHI_TYPES.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <string>

namespace StarryEngine::RHI {

    // ==================== 资源存储基类 ====================

    struct ResourceData {
        virtual ~ResourceData() = default;
        virtual void release() = 0;
        virtual bool isValid() const = 0;
        virtual void* getNativeHandle() const = 0;
        virtual size_t getMemoryUsage() const = 0;
    };

    // ==================== 资源槽位 ====================

    template<typename HandleType>
    class ResourceSlot {
    public:
        using Handle = HandleType;

        struct Entry {
            std::unique_ptr<ResourceData> data;
            std::string name;
            uint32_t generation = 1;
            uint32_t refCount = 0;
            bool allocated = false;

            bool isValid() const { return allocated && data && data->isValid(); }
        };

        ResourceSlot() = default;

        // 创建资源
        Handle create(std::unique_ptr<ResourceData> data, const std::string& name = "") {
            std::lock_guard<std::mutex> lock(mutex_);

            uint32_t index = 0;
            uint32_t generation = 1;

            // 寻找空闲槽位
            if (!freeIndices_.empty()) {
                index = freeIndices_.back();
                freeIndices_.pop_back();

                Entry& entry = entries_[index];
                generation = entry.generation;
                entry = Entry{ std::move(data), name, generation, 1, true };
            }
            else {
                // 添加新槽位
                index = static_cast<uint32_t>(entries_.size());
                entries_.push_back(Entry{ std::move(data), name, 1, 1, true });
                generation = 1;
            }

            // 如果有名称，添加到名称映射
            if (!name.empty()) {
                nameToIndex_[name] = index;
            }

            // 创建句柄
            return Handle::Create(
                static_cast<uint8_t>(Handle::CATEGORY),
                index,
                generation
            );
        }

        // 获取资源数据
        ResourceData* getData(Handle handle) {
            if (!handle.isValid()) return nullptr;

            std::lock_guard<std::mutex> lock(mutex_);
            uint32_t index = handle.getIndex();

            if (index >= entries_.size()) return nullptr;

            Entry& entry = entries_[index];
            if (!entry.allocated || entry.generation != handle.getGeneration()) {
                return nullptr;
            }

            return entry.data.get();
        }

        const ResourceData* getData(Handle handle) const {
            return const_cast<ResourceSlot*>(this)->getData(handle);
        }

        // 按名称查找
        Handle findByName(const std::string& name) const {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = nameToIndex_.find(name);
            if (it != nameToIndex_.end()) {
                uint32_t index = it->second;
                if (index < entries_.size() && entries_[index].allocated) {
                    return Handle::Create(
                        static_cast<uint8_t>(Handle::CATEGORY),
                        index,
                        entries_[index].generation
                    );
                }
            }
            return Handle::Null();
        }

        // 增加引用计数
        bool addRef(Handle handle) {
            if (!handle.isValid()) return false;

            std::lock_guard<std::mutex> lock(mutex_);
            uint32_t index = handle.getIndex();

            if (index >= entries_.size()) return false;

            Entry& entry = entries_[index];
            if (!entry.allocated || entry.generation != handle.getGeneration()) {
                return false;
            }

            entry.refCount++;
            return true;
        }

        // 减少引用计数，如果为0则销毁
        bool release(Handle handle) {
            if (!handle.isValid()) return false;

            std::lock_guard<std::mutex> lock(mutex_);
            uint32_t index = handle.getIndex();

            if (index >= entries_.size()) return false;

            Entry& entry = entries_[index];
            if (!entry.allocated || entry.generation != handle.getGeneration()) {
                return false;
            }

            if (entry.refCount > 0) {
                entry.refCount--;
            }

            // 引用计数为0，销毁资源
            if (entry.refCount == 0) {
                if (entry.data) {
                    entry.data->release();
                }

                // 从名称映射中移除
                if (!entry.name.empty()) {
                    nameToIndex_.erase(entry.name);
                }

                // 标记为未分配，增加世代
                entry.allocated = false;
                entry.generation++;
                entry.name.clear();
                entry.data.reset();

                // 添加到空闲列表
                freeIndices_.push_back(index);
            }

            return true;
        }

        // 强制销毁（忽略引用计数）
        bool destroy(Handle handle) {
            if (!handle.isValid()) return false;

            std::lock_guard<std::mutex> lock(mutex_);
            uint32_t index = handle.getIndex();

            if (index >= entries_.size()) return false;

            Entry& entry = entries_[index];
            if (!entry.allocated || entry.generation != handle.getGeneration()) {
                return false;
            }

            // 释放资源
            if (entry.data) {
                entry.data->release();
            }

            // 从名称映射中移除
            if (!entry.name.empty()) {
                nameToIndex_.erase(entry.name);
            }

            // 标记为未分配，增加世代
            entry.allocated = false;
            entry.generation++;
            entry.name.clear();
            entry.data.reset();
            entry.refCount = 0;

            // 添加到空闲列表
            freeIndices_.push_back(index);

            return true;
        }

        // 清理所有资源
        void clear() {
            std::lock_guard<std::mutex> lock(mutex_);

            for (auto& entry : entries_) {
                if (entry.allocated && entry.data) {
                    entry.data->release();
                }
            }

            entries_.clear();
            freeIndices_.clear();
            nameToIndex_.clear();
        }

        // 统计
        size_t size() const {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t count = 0;
            for (const auto& entry : entries_) {
                if (entry.allocated) count++;
            }
            return count;
        }

        bool empty() const {
            return size() == 0;
        }

    private:
        mutable std::mutex mutex_;
        std::vector<Entry> entries_;
        std::vector<uint32_t> freeIndices_;
        std::unordered_map<std::string, uint32_t> nameToIndex_;
    };

    // ==================== 全局资源管理器 ====================

    class ResourceManager {
    public:
        static ResourceManager& getInstance() {
            static ResourceManager instance;
            return instance;
        }

        // 获取各种资源槽位
        ResourceSlot<BufferHandle>& getBufferSlot() { return bufferSlot_; }
        ResourceSlot<TextureHandle>& getTextureSlot() { return textureSlot_; }
        ResourceSlot<PipelineHandle>& getPipelineSlot() { return pipelineSlot_; }
        ResourceSlot<PipelineLayoutHandle>& getPipelineLayoutSlot() { return pipelineLayoutSlot_; }
        ResourceSlot<ShaderHandle>& getShaderSlot() { return shaderSlot_; }
        ResourceSlot<SamplerHandle>& getSamplerSlot() { return samplerSlot_; }

        // 清理所有资源
        void clearAll() {
            bufferSlot_.clear();
            textureSlot_.clear();
            pipelineSlot_.clear();
            pipelineLayoutSlot_.clear();
            shaderSlot_.clear();
            samplerSlot_.clear();
        }

    private:
        ResourceManager() = default;

        ResourceSlot<BufferHandle> bufferSlot_;
        ResourceSlot<TextureHandle> textureSlot_;
        ResourceSlot<PipelineHandle> pipelineSlot_;
        ResourceSlot<PipelineLayoutHandle> pipelineLayoutSlot_;
        ResourceSlot<ShaderHandle> shaderSlot_;
        ResourceSlot<SamplerHandle> samplerSlot_;
    };

    // ==================== 便捷宏 ====================

#define RHI_BUFFER_SLOT ResourceManager::getInstance().getBufferSlot()
#define RHI_PIPELINE_SLOT ResourceManager::getInstance().getPipelineSlot()
#define RHI_PIPELINE_LAYOUT_SLOT ResourceManager::getInstance().getPipelineLayoutSlot()

} // namespace StarryEngine::RHI