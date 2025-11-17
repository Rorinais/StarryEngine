#pragma once
#include "Buffer.hpp"
#include <vector>
namespace StarryEngine {
    class IndexBuffer : public Buffer {
    public:
        using Ptr = std::shared_ptr<IndexBuffer>;
        static Ptr create(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool);

        IndexBuffer(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool);

        void loadData(const std::vector<uint32_t>& indices);

        uint32_t getIndexCount() const noexcept { return mIndexCount; }

    private:
        uint32_t mIndexCount;
    };
}