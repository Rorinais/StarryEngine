#pragma once
#include "VertexBuffer.hpp"
#include <vector>
#include <map>
#include <stdexcept>
#include <cstring>
namespace StarryEngine {
    class VertexArrayBuffer {
    public:
        struct AttributeData {
            std::vector<uint8_t> data;  // 原始字节数据
            VkFormat format;            // 数据格式
            uint32_t location;          // 着色器中的location
            uint32_t elementSize;       // 每个元素字节数
        };

        struct BindingInfo {
            uint32_t binding;           // 绑定编号
            uint32_t stride;            // 顶点间距（自动或手动设置）
            std::vector<AttributeData> attributes; // 属性列表
            VertexBuffer::Ptr buffer;   // 对应的顶点缓冲区
        };

        using Ptr = std::shared_ptr<VertexArrayBuffer>;
        static Ptr create(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool);
        VertexArrayBuffer(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool);
        ~VertexArrayBuffer();

        void cleanup();

        void beginBinding(uint32_t binding, uint32_t stride = 0);
        template<typename T>
        void addAttribute(uint32_t location, VkFormat format, const std::vector<T>& data);
        void finishBinding();

        const std::vector<VkVertexInputBindingDescription>& getBindingDescriptions() const { return mBindingDescriptions; }
        const std::vector<VkVertexInputAttributeDescription>& getAttributeDescriptions() const { return mAttributeDescriptions; }
        std::vector<VkBuffer> getBufferHandles() const;
        uint32_t getVertexCount() const;
        std::vector<VkDeviceSize> getOffsets() const;
        uint32_t getFormatSize(VkFormat format);

    private:
        void validateAttributes(const BindingInfo& binding);
        void calculateLayout(BindingInfo& binding);
        std::vector<uint8_t> interleaveData(const BindingInfo& binding);
        void generateVulkanDescriptions(const BindingInfo& binding);

        LogicalDevice::Ptr mLogicalDevice;
        CommandPool::Ptr mCommandPool;

        uint32_t mCurrentBinding = 0;
        std::map<uint32_t, BindingInfo> mBindings;
        std::vector<VkVertexInputBindingDescription> mBindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> mAttributeDescriptions;
    };

    // 添加顶点属性数据
    template<typename T>
    void VertexArrayBuffer::addAttribute(uint32_t location, VkFormat format, const std::vector<T>& data) {
        auto& binding = mBindings[mCurrentBinding];

        // 计算属性元素大小
        const uint32_t elementSize = getFormatSize(format);
        if (elementSize != sizeof(T)) {
            throw std::runtime_error("Data type size does not match format size");
        }

        // 保存属性数据
        AttributeData attr;
        attr.format = format;
        attr.location = location;
        attr.elementSize = elementSize;
        attr.data.resize(data.size() * elementSize);
        memcpy(attr.data.data(), data.data(), attr.data.size());

        binding.attributes.push_back(std::move(attr));
    }
}