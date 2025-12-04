#pragma once
#include "Buffer.hpp"
#include <vector>

namespace StarryEngine {

    // 索引类型枚举
    enum class IndexType {
        UINT16 = VK_INDEX_TYPE_UINT16,
        UINT32 = VK_INDEX_TYPE_UINT32
    };

    class IndexBuffer : public Buffer {
    public:
        using Ptr = std::shared_ptr<IndexBuffer>;

        // 创建空的IndexBuffer
        static Ptr create(const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool);

        // 创建并初始化IndexBuffer
        template<typename T>
        static Ptr createWithData(const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool,
            const std::vector<T>& indices);

        IndexBuffer(const LogicalDevice::Ptr& logicalDevice,
            const CommandPool::Ptr& commandPool);

        // 加载索引数据
        void loadData(const std::vector<uint16_t>& indices);
        void loadData(const std::vector<uint32_t>& indices);

        // 模板化的加载方法
        template<typename T>
        void loadData(const std::vector<T>& indices);

        // 获取索引信息
        uint32_t getIndexCount() const { return mIndexCount; }
        IndexType getIndexType() const { return mIndexType; }
        VkIndexType getVkIndexType() const { return static_cast<VkIndexType>(mIndexType); }

        // 绑定命令
        void bind(VkCommandBuffer commandBuffer) const;

    private:
        uint32_t mIndexCount = 0;
        IndexType mIndexType = IndexType::UINT32;
    };
}