#pragma once
#include <string>
#include <memory>
#include <functional>
#include <vulkan/vulkan.h>
namespace StarryEngine {

    enum class ResourceType {
        UniformBuffer,
        Texture,
        VertexBuffer,
        IndexBuffer,
        VertexArray,
        StorageBuffer
    };

    class Resource {
    public:
        using Ptr = std::shared_ptr<Resource>;

        Resource(ResourceType type, const std::string& name)
            : mType(type), mName(name) {
        }

        virtual ~Resource() = default;

        ResourceType getType() const { return mType; }
        const std::string& getName() const { return mName; }

        virtual void release() = 0;
        virtual void load() = 0;
        virtual bool isReady() const { return true; }
        virtual size_t getMemoryUsage() const = 0;

        // 添加交换链重建处理方法
        virtual void onSwapchainRecreated(VkExtent2D newExtent) {}

        // 异步加载支持
        using LoadCallback = std::function<void(Resource*)>;
        virtual void loadAsync(LoadCallback callback = nullptr) {
            if (callback) callback(this);
        }

    protected:
        ResourceType mType;
        std::string mName;
    };
}