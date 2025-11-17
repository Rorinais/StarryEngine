#include "VertexBufferResource.hpp"
namespace StarryEngine {

    VertexBufferResource::VertexBufferResource(
        const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool,
        const std::string& name,
        const void* data,
        size_t size)
        : Resource(ResourceType::VertexBuffer, name),
        mBuffer(VertexBuffer::create(logicalDevice, commandPool))
    {
        mReady = (data != nullptr && size > 0);
        if (mReady) {
            mBuffer->uploadData(data, size);
        }
    }

    void VertexBufferResource::release() {
        if (mBuffer) {
            mBuffer->cleanup();
            mBuffer.reset();
        }
    }

    size_t VertexBufferResource::getMemoryUsage() const {
        return mBuffer ? mBuffer->getSize() : 0;
    }

    void VertexBufferResource::load() {
        if (!mReady) {
            if (mBuffer->getSize() > 0) {
                std::vector<uint8_t> zeroData(mBuffer->getSize(), 0);
                mBuffer->uploadData(zeroData.data(), zeroData.size());
            }
            mReady = true;
        }
    }

    bool VertexBufferResource::isReady() const {
        return mReady;
    }
    
}