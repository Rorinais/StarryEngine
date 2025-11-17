#pragma once
#include "Resource.hpp"
#include "../../../resources/buffers/VertexBuffer.hpp"

namespace StarryEngine {
    class VertexBufferResource : public Resource {
    public:
        using Ptr = std::shared_ptr<VertexBufferResource>;
        VertexBufferResource(const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool,
            const std::string& name,
            const void* data,
            size_t size);

        VertexBuffer::Ptr getBuffer() const { return mBuffer; }

        void release() override;
        size_t getMemoryUsage() const override;

        void load() override;
        bool isReady() const override;

    private:
        bool mReady = false;
        VertexBuffer::Ptr mBuffer;
    };
}