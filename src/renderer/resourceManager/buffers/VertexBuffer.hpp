#pragma once
#include "Buffer.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace StarryEngine {
    class VertexBuffer : public Buffer {
    public:
        using Ptr = std::shared_ptr<VertexBuffer>;

        static Ptr create(const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool);

        VertexBuffer(const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool);

        void uploadData(const void* data, VkDeviceSize size);

        void loadData(const std::vector<float>& vertices) {
            uploadData(vertices.data(), vertices.size() * sizeof(float));
        }

        template<typename T>
        void loadData(const std::vector<T>& vertices) {
            uploadData(vertices.data(), vertices.size() * sizeof(T));
        }

        template<typename T>
        void loadData(const T* vertices, size_t count) {
            uploadData(vertices, count * sizeof(T));
        }

    private:
        uint32_t mVertexCount = 0;
        size_t mVertexSize = 0;
    };
}