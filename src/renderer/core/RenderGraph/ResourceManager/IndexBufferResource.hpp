#pragma once
#include "Resource.hpp"
#include "../../../resources/buffers/IndexBuffer.hpp"
namespace StarryEngine {
    class IndexBufferResource : public Resource {
    public:
        using Ptr = std::shared_ptr<IndexBufferResource>;
        IndexBufferResource(const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool,
            const std::string& name,
            const std::vector<uint32_t>& indices);

        IndexBuffer::Ptr getBuffer() const { return mBuffer; }

        void release() override;
        size_t getMemoryUsage() const override;
        uint32_t getIndexCount() const;

        void load() override;
        bool isReady() const override;

    private:
        bool mReady = false;
        IndexBuffer::Ptr mBuffer;
    };
}