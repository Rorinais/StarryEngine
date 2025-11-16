#include "ResourceManager.hpp"
#include "UniformBufferResource.hpp"
#include "TextureResource.hpp"
#include "VertexBufferResource.hpp"
#include "IndexBufferResource.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

namespace StarryEngine {

    ResourceManager::ResourceManager(const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool)
        : mLogicalDevice(logicalDevice), mCommandPool(commandPool)
    {
        startAsyncLoader();
    }

    ResourceManager::~ResourceManager() {
        stopAsyncLoader();
        releaseAllResources();
    }

    Resource::Ptr ResourceManager::createUniformBuffer(const std::string& name, size_t size) {
        if (auto it = mResources.find(name); it != mResources.end()) {
            return it->second;
        }

        auto resource = std::make_shared<UniformBufferResource>(
            mLogicalDevice, mCommandPool, name, size
        );

        mResources[name] = resource;
        mTotalMemoryUsage += resource->getMemoryUsage();

        return resource;
    }

    Resource::Ptr ResourceManager::createTexture(const std::string& name, const char* imagePath) {
        // 移除锁
        if (auto it = mResources.find(name); it != mResources.end()) {
            return it->second;
        }

        auto resource = std::make_shared<TextureResource>(
            mLogicalDevice, mCommandPool, name, imagePath
        );

        mResources[name] = resource;
        return resource;
    }

    Resource::Ptr ResourceManager::createVertexBuffer(const std::string& name, const void* data, size_t size) {
        // 移除锁
        if (auto it = mResources.find(name); it != mResources.end()) {
            return it->second;
        }

        auto resource = std::make_shared<VertexBufferResource>(
            mLogicalDevice, mCommandPool, name, data, size
        );

        mResources[name] = resource;
        if (resource->isReady()) {
            mTotalMemoryUsage += resource->getMemoryUsage();
        }

        return resource;
    }

    Resource::Ptr ResourceManager::createIndexBuffer(const std::string& name, const std::vector<uint32_t>& indices) {
        // 移除锁
        if (auto it = mResources.find(name); it != mResources.end()) {
            return it->second;
        }

        auto resource = std::make_shared<IndexBufferResource>(
            mLogicalDevice, mCommandPool, name, indices
        );

        mResources[name] = resource;
        mTotalMemoryUsage += resource->getMemoryUsage();

        return resource;
    }

    Resource::Ptr ResourceManager::getResource(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mResourceMutex);
        if (auto it = mResources.find(name); it != mResources.end()) {
            return it->second;
        }
        return nullptr;
    }

    void ResourceManager::releaseResource(const std::string& name) {
        std::lock_guard<std::mutex> lock(mResourceMutex);
        if (auto it = mResources.find(name); it != mResources.end()) {
            mTotalMemoryUsage -= it->second->getMemoryUsage();
            it->second->release();
            mResources.erase(it);
        }
    }

    void ResourceManager::releaseAllResources() {
        std::lock_guard<std::mutex> lock(mResourceMutex);

        // 清理描述符资源
        if (mDescriptorPool) {
            vkDestroyDescriptorPool(mLogicalDevice->getHandle(), mDescriptorPool, nullptr);
            mDescriptorPool = VK_NULL_HANDLE;
        }

        if (mDescriptorSetLayout) {
            vkDestroyDescriptorSetLayout(mLogicalDevice->getHandle(), mDescriptorSetLayout, nullptr);
            mDescriptorSetLayout = VK_NULL_HANDLE;
        }

        // 清理其他资源
        for (auto& [name, resource] : mResources) {
            resource->release();
        }
        mResources.clear();
        mTotalMemoryUsage = 0;

        // 清理Uniform Buffer组
        mUniformBufferGroups.clear();
    }

    void ResourceManager::startAsyncLoader(uint32_t threadCount) {
        mLoaderRunning = true;
        for (uint32_t i = 0; i < threadCount; ++i) {
            mLoaderThreads.emplace_back(&ResourceManager::asyncLoaderThread, this);
        }
    }

    void ResourceManager::stopAsyncLoader() {
        mLoaderRunning = false;
        mQueueCV.notify_all();

        for (auto& thread : mLoaderThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        mLoaderThreads.clear();
    }

    void ResourceManager::queueAsyncLoad(Resource::Ptr resource,
        Resource::LoadCallback callback) {
        if (!resource) return;

        std::lock_guard<std::mutex> lock(mQueueMutex);
        mLoadQueue.push({ resource, callback });
        mQueueCV.notify_one();
    }

    void ResourceManager::asyncLoaderThread() {
        while (mLoaderRunning) {
            LoadTask task;

            {
                std::unique_lock<std::mutex> lock(mQueueMutex);
                mQueueCV.wait(lock, [&] {
                    return !mLoadQueue.empty() || !mLoaderRunning;
                    });

                if (!mLoaderRunning) break;

                if (mLoadQueue.empty()) continue;

                task = mLoadQueue.front();
                mLoadQueue.pop();
            }

            // 执行异步加载
            task.resource->loadAsync([=](Resource* res) {
                if (task.callback) {
                    task.callback(res);
                }

                // 更新内存使用统计
                if (res->isReady()) {
                    std::lock_guard<std::mutex> lock(mResourceMutex);
                    mTotalMemoryUsage += res->getMemoryUsage();
                }
                });
        }
    }

    size_t ResourceManager::getTotalMemoryUsage() const {
        std::lock_guard<std::mutex> lock(mResourceMutex);
        return mTotalMemoryUsage;
    }

    void ResourceManager::setMemoryBudget(size_t budget) {
        mMemoryBudget = budget;
    }

    void ResourceManager::enforceMemoryBudget() {
        if (mMemoryBudget == 0) return;

        std::lock_guard<std::mutex> lock(mResourceMutex);
        while (mTotalMemoryUsage > mMemoryBudget && !mResources.empty()) {
            // 找到最近最少使用的资源
            auto lruIt = mResources.begin();
            size_t minTimestamp = std::numeric_limits<size_t>::max();

            for (auto it = mResources.begin(); it != mResources.end(); ++it) {
                // 这里简化实现，实际应记录最后访问时间
                if (it->second->getMemoryUsage() > 0) {
                    minTimestamp = 0; // 简化处理
                    lruIt = it;
                    break;
                }
            }

            // 释放资源
            if (lruIt != mResources.end()) {
                mTotalMemoryUsage -= lruIt->second->getMemoryUsage();
                lruIt->second->release();
                mResources.erase(lruIt);
            }
        }
    }

    void ResourceManager::printResourceStats() const {
        std::lock_guard<std::mutex> lock(mResourceMutex);

        std::cout << "===== Resource Manager Stats =====" << std::endl;
        std::cout << "Total Resources: " << mResources.size() << std::endl;
        std::cout << "Total Memory Usage: "
            << (getTotalMemoryUsage() / (1024 * 1024))
            << " MB" << std::endl;

        size_t textureCount = 0;
        size_t bufferCount = 0;

        for (const auto& [name, resource] : mResources) {
            switch (resource->getType()) {
            case ResourceType::Texture: textureCount++; break;
            case ResourceType::UniformBuffer:
            case ResourceType::VertexBuffer:
            case ResourceType::IndexBuffer:
            case ResourceType::StorageBuffer:
                bufferCount++;
                break;
            default: break;
            }
        }

        std::cout << "Textures: " << textureCount << std::endl;
        std::cout << "Buffers: " << bufferCount << std::endl;
        std::cout << "=================================" << std::endl;
    }

    void ResourceManager::buildResources() {
        std::lock_guard<std::mutex> lock(mResourceMutex);
        for (auto& [name, resource] : mResources) {
            if (!resource->isReady()) {
                resource->load();
            }
        }
    }

    void ResourceManager::onSwapchainRecreated() {
        if (mSwapchain) {
            mSwapchain->recreate();
            mSwapchainImageViews = mSwapchain->getImageViews();
        }

        if (mDepthTexture) {
            VkExtent2D extent = {
                mSwapchain->getExtent().width,
                mSwapchain->getExtent().height
            };
            // 调用深度纹理的重建方法
            mDepthTexture->recreate(extent);
        }

        // 通知所有资源交换链已重建
        VkExtent2D newExtent = {
            mSwapchain->getExtent().width,
            mSwapchain->getExtent().height
        };

        for (auto& [name, resource] : mResources) {
            resource->onSwapchainRecreated(newExtent);
        }
    }

    CommandPool::Ptr ResourceManager::getCommandPool() const {
        return mCommandPool;
    }


    void ResourceManager::registerSwapchainResources(
        SwapChain::Ptr swapchain,
        const std::vector<VkImageView>& swapchainImageViews,
        Texture::Ptr depthTexture
    ) {
        mSwapchain = swapchain;
        mSwapchainImageViews = swapchainImageViews;
        mDepthTexture = depthTexture;
    }

    void ResourceManager::registerUniformBuffer(const std::string& name, size_t size, uint32_t frameCount) {
        std::lock_guard<std::mutex> lock(mResourceMutex);

        // 添加日志
        std::cout << "Registering Uniform Buffer: " << name
            << ", Size: " << size
            << ", Frames: " << frameCount << std::endl;

        // 检查名称冲突
        if (mUniformBufferGroups.find(name) != mUniformBufferGroups.end()) {
            std::cerr << "Error: Uniform buffer group already registered: " << name << std::endl;
            throw std::runtime_error("Uniform buffer group already registered: " + name);
        }

        try {
            // 创建组
            UniformBufferGroup group;
            group.baseName = name;
            group.size = size;

            // 为每帧创建独立的 Uniform Buffer
            for (uint32_t i = 0; i < frameCount; i++) {
                std::string frameName = name + "_Frame" + std::to_string(i);
                std::cout << "Creating frame buffer: " << frameName << std::endl;

                auto resource = createUniformBuffer(frameName, size);
                if (!resource) {
                    throw std::runtime_error("Failed to create uniform buffer: " + frameName);
                }
                group.buffers.push_back(resource);
            }

            mUniformBufferGroups[name] = group;

            // 添加描述符绑定信息
            DescriptorBinding binding{
                .binding = static_cast<uint32_t>(mDescriptorBindings.size()),
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            };

            mDescriptorBindings.push_back(binding);

            std::cout << "Added descriptor binding: binding=" << binding.binding
                << ", type=" << binding.type
                << ", stageFlags=" << binding.stageFlags << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Error in registerUniformBuffer: " << e.what() << std::endl;
            throw;
        }
    }

    void ResourceManager::updateUniformBuffer(const std::string& baseName, uint32_t frameIndex, const void* data, size_t size) {
        // 查找 Uniform Buffer 组
        auto it = mUniformBufferGroups.find(baseName);
        if (it == mUniformBufferGroups.end()) {
            throw std::runtime_error("Uniform buffer group not found: " + baseName);
        }

        auto& group = it->second;

        // 检查帧索引有效性
        if (frameIndex >= group.buffers.size()) {
            throw std::runtime_error("Frame index out of range for uniform buffer group: " + baseName);
        }

        // 获取资源并更新
        auto bufferResource = group.buffers[frameIndex];
        if (auto buffer = std::dynamic_pointer_cast<UniformBufferResource>(bufferResource)) {
            buffer->update(data, size);
        }
        else {
            throw std::runtime_error("Resource is not a uniform buffer: " + group.baseName);
        }
    }

    void ResourceManager::buildDescriptorResources() {
        // 确保有绑定信息
        if (mDescriptorBindings.empty()) {
            throw std::runtime_error("No descriptor bindings registered");
        }

        // 1. 创建描述符布局
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
        for (const auto& binding : mDescriptorBindings) {
            VkDescriptorSetLayoutBinding layoutBinding{};
            layoutBinding.binding = binding.binding;
            layoutBinding.descriptorType = binding.type;
            layoutBinding.descriptorCount = 1;
            layoutBinding.stageFlags = binding.stageFlags;
            layoutBindings.push_back(layoutBinding);
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
        layoutInfo.pBindings = layoutBindings.data();

        VkResult layoutResult = vkCreateDescriptorSetLayout(
            mLogicalDevice->getHandle(),
            &layoutInfo,
            nullptr,
            &mDescriptorSetLayout
        );

        if (layoutResult != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout! Error: " + std::to_string(layoutResult));
        }

        // 2. 创建描述符池（改进的池大小计算）
        std::unordered_map<VkDescriptorType, uint32_t> typeCounts;
        for (const auto& binding : mDescriptorBindings) {
            typeCounts[binding.type] += MAX_FRAMES_IN_FLIGHT;
        }

        std::vector<VkDescriptorPoolSize> poolSizes;
        for (const auto& [type, count] : typeCounts) {
            poolSizes.push_back(VkDescriptorPoolSize{
                .type = type,
                .descriptorCount = count
                });
        }

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkResult poolResult = vkCreateDescriptorPool(
            mLogicalDevice->getHandle(),
            &poolInfo,
            nullptr,
            &mDescriptorPool
        );

        if (poolResult != VK_SUCCESS) {
            vkDestroyDescriptorSetLayout(mLogicalDevice->getHandle(), mDescriptorSetLayout, nullptr);
            mDescriptorSetLayout = VK_NULL_HANDLE;

            std::string errorMsg = "Failed to create descriptor pool! Error: ";
            // 添加详细的错误处理...
            throw std::runtime_error(errorMsg);
        }

        // 3. 分配描述符集（改进错误处理）
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, mDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mDescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        mDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        VkResult allocResult = vkAllocateDescriptorSets(
            mLogicalDevice->getHandle(),
            &allocInfo,
            mDescriptorSets.data()
        );

        if (allocResult != VK_SUCCESS) {
            vkDestroyDescriptorPool(mLogicalDevice->getHandle(), mDescriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(mLogicalDevice->getHandle(), mDescriptorSetLayout, nullptr);
            mDescriptorPool = VK_NULL_HANDLE;
            mDescriptorSetLayout = VK_NULL_HANDLE;

            std::string errorMsg = "Failed to allocate descriptor sets! Error: ";
            // 添加详细的错误处理...
            throw std::runtime_error(errorMsg);
        }

        // 4. 关联Uniform Buffer到描述符集（添加资源验证）
        for (size_t frameIndex = 0; frameIndex < MAX_FRAMES_IN_FLIGHT; frameIndex++) {
            std::vector<VkWriteDescriptorSet> descriptorWrites;

            for (auto& [groupName, group] : mUniformBufferGroups) {
                if (frameIndex >= group.buffers.size()) {
                    continue;
                }

                auto bufferResource = group.buffers[frameIndex];
                if (!bufferResource) {
                    throw std::runtime_error("Buffer resource is null: " + groupName);
                }

                if (auto buffer = std::dynamic_pointer_cast<UniformBufferResource>(bufferResource)) {
                    auto uniformBuffer = buffer->getBuffer();
                    if (!uniformBuffer) {
                        throw std::runtime_error("UniformBuffer is null: " + groupName);
                    }

                    //if (!uniformBuffer->isValid()) {
                    //    throw std::runtime_error("UniformBuffer is invalid: " + groupName);
                    //}

                    VkDescriptorBufferInfo bufferInfo{};
                    bufferInfo.buffer = uniformBuffer->getBuffer();
                    bufferInfo.offset = 0;
                    bufferInfo.range = uniformBuffer->getSize();

                    VkWriteDescriptorSet descriptorWrite{};
                    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrite.dstSet = mDescriptorSets[frameIndex];
                    descriptorWrite.dstBinding = 0; // 注意：这里需要与注册时的binding一致
                    descriptorWrite.dstArrayElement = 0;
                    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    descriptorWrite.descriptorCount = 1;
                    descriptorWrite.pBufferInfo = &bufferInfo;

                    descriptorWrites.push_back(descriptorWrite);
                }
            }

            if (!descriptorWrites.empty()) {
                vkUpdateDescriptorSets(
                    mLogicalDevice->getHandle(),
                    static_cast<uint32_t>(descriptorWrites.size()),
                    descriptorWrites.data(),
                    0, nullptr
                );
            }
        }
    }
}
