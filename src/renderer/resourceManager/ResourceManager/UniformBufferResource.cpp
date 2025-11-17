#include "UniformBufferResource.hpp"
namespace StarryEngine {
    UniformBufferResource::UniformBufferResource(
        const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool,
        const std::string& name,
        size_t size)
        : Resource(ResourceType::UniformBuffer, name),
        mBuffer(UniformBuffer::create(logicalDevice, commandPool, size))
    {
        mReady = (size > 0);
    }

    void UniformBufferResource::update(const void* data, size_t size, size_t offset) {
        if (mBuffer) {
            mBuffer->uploadData(data, size, offset);
        }
    }

    void UniformBufferResource::release() {
        if (mBuffer) {
            mBuffer->cleanup();
            mBuffer.reset();
        }
    }

    size_t UniformBufferResource::getMemoryUsage() const {
        return mBuffer ? mBuffer->getSize() : 0;
    }

    void UniformBufferResource::load() {
        if (!mReady) {
            if (mBuffer->getSize() > 0) {
                std::vector<uint8_t> zeroData(mBuffer->getSize(), 0);
                mBuffer->uploadData(zeroData.data(), zeroData.size());
            }
            mReady = true;
        }
    }

    bool UniformBufferResource::isReady() const {
        return mReady;
    }
}
