#include "IndexBufferResource.hpp"
namespace StarryEngine {

    IndexBufferResource::IndexBufferResource(
        const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool,
        const std::string& name,
        const std::vector<uint32_t>& indices)
        : Resource(ResourceType::IndexBuffer, name),
        mBuffer(IndexBuffer::create(logicalDevice, commandPool))
    {
        if (!indices.empty()) {
            mBuffer->loadData(indices);
            mReady = true; // 标记为已加载
        }
    }

    void IndexBufferResource::release() {
        if (mBuffer) {
            mBuffer->cleanup();
            mBuffer.reset();
            mReady = false; // 重置就绪状态
        }
    }

    size_t IndexBufferResource::getMemoryUsage() const {
        return mBuffer ? mBuffer->getSize() : 0;
    }

    uint32_t IndexBufferResource::getIndexCount() const {
        return mBuffer ? mBuffer->getIndexCount() : 0;
    }

    void IndexBufferResource::load() {
        if (!mReady) {
            if (mBuffer->getSize() > 0) {
                std::vector<uint32_t> zeroIndices(mBuffer->getSize() / sizeof(uint32_t), 0);
                mBuffer->loadData(zeroIndices);
            }
            mReady = true;
        }
    }

    bool IndexBufferResource::isReady() const {
        return mReady;
    }
}