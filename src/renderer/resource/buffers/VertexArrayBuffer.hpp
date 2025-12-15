#pragma once
#include "VertexBuffer.hpp"
#include "VertexLayouts.hpp"
#include <vector>
#include <map>
#include <stdexcept>
#include <cstring>
#include <type_traits>
#include <memory>
#include <iostream>
#include <algorithm>

namespace StarryEngine {

    class VertexArrayBuffer {
    public:
        using Ptr = std::shared_ptr<VertexArrayBuffer>;

        // ============ 私有结构体先定义 ============
    private:
        struct BufferData {
            VertexBuffer::Ptr buffer;
            VertexLayout layout;
            BufferMode mode;
            uint32_t vertexCount;

            VkBuffer getHandle() const { return buffer ? buffer->getBuffer() : VK_NULL_HANDLE; }
            bool isValid() const { return buffer != nullptr; }
        };

        struct SeparatedAttribute {
            std::vector<uint8_t> data;
            VkFormat format;
            uint32_t location;
            uint32_t elementSize;
        };

    public:
        static Ptr create(const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool);

        VertexArrayBuffer(const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool);

        ~VertexArrayBuffer();

        void cleanup();

        // ============ 高性能API ============

        // 1. 上传结构体数组（最优性能）
        template<typename VertexType>
        void upload(uint32_t binding, const std::vector<VertexType>& vertices,
            const VertexLayout& layout) {
            uploadInternal(binding, vertices.data(),
                vertices.size() * sizeof(VertexType), layout,
                BufferMode::INTERLEAVED);
        }

        // 2. 自动布局推断版本
        template<typename VertexType>
        void upload(uint32_t binding, const std::vector<VertexType>& vertices) {
            VertexLayout layout = generateLayout<VertexType>(binding);
            upload<VertexType>(binding, vertices, layout);  // 明确指定模板参数
        }

        // 3. 原始数据上传
        void upload(uint32_t binding, const void* data, size_t size,
            const VertexLayout& layout);

        // ============ 兼容性API ============

        // 开始定义分离数据绑定
        void beginSeparated(uint32_t binding);

        // 添加分离属性 - 使用显式特化避免递归
        void addSeparatedAttributeVec3(uint32_t location, VkFormat format,
            const std::vector<glm::vec3>& data);

        void addSeparatedAttributeVec2(uint32_t location, VkFormat format,
            const std::vector<glm::vec2>& data);

        // 完成分离绑定
        void finishSeparated(uint32_t stride = 0);

        // ============ 查询接口 ============

        const std::vector<VkVertexInputBindingDescription>&
            getBindingDescriptions() const;

        const std::vector<VkVertexInputAttributeDescription>&
            getAttributeDescriptions() const;

        std::vector<VkBuffer> getBufferHandles() const;

        std::vector<VkDeviceSize> getOffsets() const;

        uint32_t getVertexCount(uint32_t binding = 0) const;

        std::map<uint32_t, uint32_t> getAllVertexCounts() const;

        // ============ 缓冲区管理 ============

        bool hasBinding(uint32_t binding) const;

        const BufferData* getBufferData(uint32_t binding) const;

        // ============ 工具方法 ============

        static uint32_t getFormatSize(VkFormat format);

        template<typename VertexType>
        static VertexLayout generateLayout(uint32_t binding = 0);

        // ============ 兼容性API（与原接口保持一致） ============

        void beginBinding(uint32_t binding, uint32_t stride = 0);

        // 添加特定类型的属性函数，避免模板递归
        void addAttribute(uint32_t location, VkFormat format,
            const std::vector<glm::vec3>& data) {
            addSeparatedAttributeVec3(location, format, data);
        }

        void addAttribute(uint32_t location, VkFormat format,
            const std::vector<glm::vec2>& data) {
            addSeparatedAttributeVec2(location, format, data);
        }

        void finishBinding();

        // ============ 批量设置接口 ============

        VertexArrayBuffer& addBindings(const std::vector<VkVertexInputBindingDescription>& bindings);
        VertexArrayBuffer& addAttributes(const std::vector<VkVertexInputAttributeDescription>& attributes);
        VertexArrayBuffer& setBindings(const std::vector<VkVertexInputBindingDescription>& bindings);
        VertexArrayBuffer& setAttributes(const std::vector<VkVertexInputAttributeDescription>& attributes);

    private:
        // 私有方法
        void uploadInternal(uint32_t binding, const void* data, size_t size,
            const VertexLayout& layout, BufferMode mode);

        void validateLayout(const VertexLayout& layout);
        void updateDescriptions(const VertexLayout& layout);

        // 私有成员
        LogicalDevice::Ptr mLogicalDevice;
        CommandPool::Ptr mCommandPool;

        std::map<uint32_t, BufferData> mBuffers;
        std::vector<VkVertexInputBindingDescription> mBindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> mAttributeDescriptions;

        uint32_t mCurrentBinding = 0;
        std::vector<SeparatedAttribute> mSeparatedAttributes;
    };

    // ==================== 预定义布局生成特化 ====================

    template<>
    inline VertexLayout VertexArrayBuffer::generateLayout<VertexPos>(uint32_t binding) {
        VertexLayout layout;
        layout.binding = binding;
        layout.stride = sizeof(VertexPos);
        layout.addAttribute(0, VK_FORMAT_R32G32B32_SFLOAT,
            offsetof(VertexPos, position), "position");
        return layout;
    }

    template<>
    inline VertexLayout VertexArrayBuffer::generateLayout<VertexPosColor>(uint32_t binding) {
        VertexLayout layout;
        layout.binding = binding;
        layout.stride = sizeof(VertexPosColor);
        layout.addAttribute(0, VK_FORMAT_R32G32B32_SFLOAT,
            offsetof(VertexPosColor, position), "position");
        layout.addAttribute(1, VK_FORMAT_R32G32B32_SFLOAT,
            offsetof(VertexPosColor, color), "color");
        return layout;
    }

    template<>
    inline VertexLayout VertexArrayBuffer::generateLayout<VertexPosTex>(uint32_t binding) {
        VertexLayout layout;
        layout.binding = binding;
        layout.stride = sizeof(VertexPosTex);
        layout.addAttribute(0, VK_FORMAT_R32G32B32_SFLOAT,
            offsetof(VertexPosTex, position), "position");
        layout.addAttribute(1, VK_FORMAT_R32G32_SFLOAT,
            offsetof(VertexPosTex, texCoord), "texCoord");
        return layout;
    }

    template<>
    inline VertexLayout VertexArrayBuffer::generateLayout<VertexPosNormalTex>(uint32_t binding) {
        VertexLayout layout;
        layout.binding = binding;
        layout.stride = sizeof(VertexPosNormalTex);
        layout.addAttribute(0, VK_FORMAT_R32G32B32_SFLOAT,
            offsetof(VertexPosNormalTex, position), "position");
        layout.addAttribute(1, VK_FORMAT_R32G32B32_SFLOAT,
            offsetof(VertexPosNormalTex, normal), "normal");
        layout.addAttribute(2, VK_FORMAT_R32G32_SFLOAT,
            offsetof(VertexPosNormalTex, texCoord), "texCoord");
        return layout;
    }

    // ==================== 通用模板定义 ====================

    template<typename VertexType>
    inline VertexLayout VertexArrayBuffer::generateLayout(uint32_t binding) {
        static_assert(sizeof(VertexType) == 0,
            "Please specialize generateLayout for your vertex type");
        return VertexLayout{};
    }
}