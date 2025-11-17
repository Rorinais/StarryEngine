#pragma once
#include "Resource.hpp"
#include "../../../resources/buffers/UniformBuffer.hpp"
namespace StarryEngine {
    class UniformBufferResource : public Resource {
    public:
        using Ptr = std::shared_ptr<UniformBufferResource>;

        UniformBufferResource(
            const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool,
            const std::string& name,
            size_t size);

        void update(const void* data, size_t size, size_t offset = 0);
        UniformBuffer::Ptr getBuffer() const { return mBuffer; }

        void release() override;
        size_t getMemoryUsage() const override;
        void load() override;
        bool isReady() const override;

        // 实现交换链重建方法（空实现）
        void onSwapchainRecreated(VkExtent2D newExtent) override {}

    private:
        bool mReady = false;
        UniformBuffer::Ptr mBuffer;
    };
}
