#pragma once
#include <stb_image.h>
#include <variant>
#include "../interface/RHI_RESOURCE_MANAGER.hpp"

namespace StarryEngine::RenderGraph {
    class VertexLayout {
    public:
        VertexLayout& addBinding(uint32_t binding, uint32_t stride,
            RHI::VertexInputRate inputRate = RHI::VertexInputRate::PerVertex);
        VertexLayout& addAttribute(uint32_t location, uint32_t binding,
            RHI::Format format, uint32_t offset);
        VertexLayout& addAttribute(uint32_t location, uint32_t binding, RHI::Format format);
        RHI::VertexInputState build() const;

        // 获取指定 binding 的 stride，用于创建顶点缓冲区
        uint32_t getBindingStride(uint32_t binding) const;

        // 获取所有已定义的 binding 索引（升序）
        std::vector<uint32_t> getBindings() const;

    private:
        uint32_t getNextOffset(uint32_t binding) const;
        uint32_t getFormatSize(RHI::Format format) const;

        struct BindingInfo {
            uint32_t stride;
            RHI::VertexInputRate inputRate;
        };
        std::unordered_map<uint32_t, BindingInfo> mBindings;
        std::vector<RHI::VertexAttribute> mAttributes;
        mutable std::unordered_map<uint32_t, uint32_t> mBindingCurrentOffsets;
    };

    class Geometry {
    public:
        Geometry(std::shared_ptr<RHI::ResourceManager> resMgr);

        // 设置顶点缓冲区（指定 binding）
        void setVertexBuffer(uint32_t binding, const std::vector<float>& vertices,
            const VertexLayout& layout, const std::string& debugName);

        // 设置顶点缓冲区（默认 binding 0）
        void setVertexBuffer(const std::vector<float>& vertices,
            const VertexLayout& layout, const std::string& debugName);

        void setIndexBuffer(const std::vector<uint32_t>& indices, const std::string& debugName);

        RHI::BufferHandle getVertexBufferHandle(uint32_t binding) const;
        RHI::BufferHandle getIndexBufferHandle() const { return mIndexBufferHandle; }
        uint32_t getIndexCount() const { return mIndexCount; }

        // 获取所有已定义的 binding 列表
        std::vector<uint32_t> getBindings() const;

        // 获取顶点输入状态（用于管线创建）
        RHI::VertexInputState getVertexInputState() const;

    private:
        std::shared_ptr<RHI::ResourceManager> mResMgr;
        std::unordered_map<uint32_t, RHI::BufferHandle> mVertexBufferHandles;
        RHI::BufferHandle mIndexBufferHandle = RHI::BufferHandle::Null();
        uint32_t mIndexCount = 0;
        VertexLayout mLayout;
    };

}