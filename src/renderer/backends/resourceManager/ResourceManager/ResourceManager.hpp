#pragma once
#include "Resource.hpp"
#include "../../WindowContext/WindowContext.hpp"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <atomic>

//需求：
//资源类型：
//物理资源：uniformbuffer，vertexbuffer，indexbuffer，texture，ShaderModule
//逻辑资源：结构树：
//                                  |——camera
//          |——NonRenderableObject——|——light
//          |                       |——emptyObject
//  scene—— |
//          |                    |——background——|cube
//          |——RenderableObject——|
//                               |                                    |VertexInputStages
//                               |                                    |renderPass
//                               |                       |——pipeline——|ShaderStages——|shadermodel*N
//                               |                       |            |pipelineLayout
//                               |          |——material——|
//                               |          |            |               |VertexShader——|vs_spv->vs_str
//                               |          |            |——shaderModel——|FragmentShader——|fs_spv->fs_str
//                               |          |            |               |CompeteShader——|cs_spv->cs_str
//                               |          |            |               |。。。。。             
//                               |          |            |
//                               |          |            |                 |uniformbuffer（物理资源）
//                               |          |            |                 |texture（物理资源）
//                               |          |            |——descriptorSet——|constantbuffer（物理资源）
//                               |          |                              |。。。。
//                               |          |                             
//                               |          |
//                               |——Mesh*N——|
//                                          |
//                                          |            |vertexbuffer（物理资源）
//                                          |——geometry——|
//                                                       |indexbuffer（物理资源）

//资源管理器重构，资源管理器组件系统，设想：内存分配器，资源池，资源描述符，生命周期管理，描述符管理，着色器管理，渲染管线管理
namespace StarryEngine {
    class ResourceManager {
    public:
        struct DescriptorBinding {
            uint32_t binding;
            VkDescriptorType type;
            VkShaderStageFlags stageFlags;
        };

        using Ptr = std::shared_ptr<ResourceManager>;
        static Ptr create(const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool) {
            return std::make_shared<ResourceManager>(logicalDevice, commandPool);
        }

        ResourceManager(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool);
        ~ResourceManager();

        // 资源创建接口
        Resource::Ptr createUniformBuffer(const std::string& name, size_t size);
        Resource::Ptr createTexture(const std::string& name, const char* imagePath);
        Resource::Ptr createVertexBuffer(const std::string& name, const void* data, size_t size);
        Resource::Ptr createIndexBuffer(const std::string& name, const std::vector<uint32_t>& indices);

        // 资源获取
        Resource::Ptr getResource(const std::string& name) const;

        // 资源池管理
        void releaseResource(const std::string& name);
        void releaseAllResources();

        void buildResources();
        void onSwapchainRecreated();
        CommandPool::Ptr getCommandPool() const;

        // 异步加载支持
        void startAsyncLoader(uint32_t threadCount = 2);
        void stopAsyncLoader();
        void queueAsyncLoad(Resource::Ptr resource, Resource::LoadCallback callback = nullptr);

        // 内存管理
        size_t getTotalMemoryUsage() const;
        void setMemoryBudget(size_t budget);
        void enforceMemoryBudget();

        // 调试信息
        void printResourceStats() const;

        // 添加交换链资源管理
        void registerSwapchainResources(
            SwapChain::Ptr swapchain,
            const std::vector<VkImageView>& swapchainImageViews,
            Texture::Ptr depthTexture
        );

        SwapChain::Ptr getSwapchain() const { return mSwapchain; }
        const std::vector<VkImageView>& getSwapchainImageViews() const { return mSwapchainImageViews; }
        Texture::Ptr getDepthTexture() const { return mDepthTexture; }

        void registerUniformBuffer(const std::string& name, size_t size, uint32_t frameCount);
        void updateUniformBuffer(const std::string& baseName, uint32_t frameIndex, const void* data, size_t size);
        void buildDescriptorResources();
        VkDescriptorSetLayout getDescriptorSetLayout() const { return mDescriptorSetLayout; }
        const std::vector<VkDescriptorSet>& getDescriptorSets() const { return mDescriptorSets; }

    private:
        void asyncLoaderThread();

        struct LoadTask {
            Resource::Ptr resource;
            Resource::LoadCallback callback;
        };

        LogicalDevice::Ptr mLogicalDevice;
        CommandPool::Ptr mCommandPool;

        std::unordered_map<std::string, Resource::Ptr> mResources;
        mutable std::mutex mResourceMutex;

        // 异步加载系统
        std::vector<std::thread> mLoaderThreads;
        std::queue<LoadTask> mLoadQueue;
        std::mutex mQueueMutex;
        std::condition_variable mQueueCV;
        std::atomic<bool> mLoaderRunning{ false };

        // 内存管理
        size_t mTotalMemoryUsage = 0;
        size_t mMemoryBudget = 0;

        // 交换链资源
        SwapChain::Ptr mSwapchain;
        std::vector<VkImageView> mSwapchainImageViews;
        Texture::Ptr mDepthTexture;

        std::vector<DescriptorBinding> mDescriptorBindings;
        VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> mDescriptorSets;

        // Uniform Buffer 组管理
        struct UniformBufferGroup {
            std::string baseName;
            size_t size;
            std::vector<Resource::Ptr> buffers; // 每帧的缓冲区
        };
        std::unordered_map<std::string, UniformBufferGroup> mUniformBufferGroups;
    };
}
