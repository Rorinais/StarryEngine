#include "VertexArrayBuffer.hpp"
#include <algorithm>
#include <stdexcept>

namespace StarryEngine {

    // VertexArrayBuffer 静态方法
    VertexArrayBuffer::Ptr VertexArrayBuffer::create(const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool) {
        return std::make_shared<VertexArrayBuffer>(logicalDevice, commandPool);
    }

    VertexArrayBuffer::VertexArrayBuffer(const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool)
        : mLogicalDevice(logicalDevice)
        , mCommandPool(commandPool) {
    }

    VertexArrayBuffer::~VertexArrayBuffer() {
        cleanup();
    }

    void VertexArrayBuffer::cleanup() {
        mBuffers.clear();
        mBindingDescriptions.clear();
        mAttributeDescriptions.clear();
        mSeparatedAttributes.clear();
    }

    // 原始数据上传（非模板函数）
    void VertexArrayBuffer::upload(uint32_t binding, const void* data, size_t size,
        const VertexLayout& layout) {
        uploadInternal(binding, data, size, layout, BufferMode::INTERLEAVED);
    }

    // 添加分离属性 - 具体实现
    void VertexArrayBuffer::addSeparatedAttributeVec3(uint32_t location, VkFormat format,
        const std::vector<glm::vec3>& data) {
        SeparatedAttribute attr;
        attr.location = location;
        attr.format = format;
        attr.elementSize = sizeof(glm::vec3);

        // 复制数据
        attr.data.resize(data.size() * sizeof(glm::vec3));
        memcpy(attr.data.data(), data.data(), attr.data.size());

        mSeparatedAttributes.push_back(attr);
    }

    void VertexArrayBuffer::addSeparatedAttributeVec2(uint32_t location, VkFormat format,
        const std::vector<glm::vec2>& data) {
        SeparatedAttribute attr;
        attr.location = location;
        attr.format = format;
        attr.elementSize = sizeof(glm::vec2);

        // 复制数据
        attr.data.resize(data.size() * sizeof(glm::vec2));
        memcpy(attr.data.data(), data.data(), attr.data.size());

        mSeparatedAttributes.push_back(attr);
    }

    // 查询接口实现
    const std::vector<VkVertexInputBindingDescription>&
        VertexArrayBuffer::getBindingDescriptions() const {
        return mBindingDescriptions;
    }

    const std::vector<VkVertexInputAttributeDescription>&
        VertexArrayBuffer::getAttributeDescriptions() const {
        return mAttributeDescriptions;
    }

    std::vector<VkBuffer> VertexArrayBuffer::getBufferHandles() const {
        std::vector<VkBuffer> handles;
        for (const auto& [binding, bufferData] : mBuffers) {
            if (bufferData.isValid()) {
                handles.push_back(bufferData.getHandle());
            }
        }
        return handles;
    }

    std::vector<VkDeviceSize> VertexArrayBuffer::getOffsets() const {
        return std::vector<VkDeviceSize>(mBuffers.size(), 0);
    }

    uint32_t VertexArrayBuffer::getVertexCount(uint32_t binding) const {
        auto it = mBuffers.find(binding);
        if (it != mBuffers.end()) {
            return it->second.vertexCount;
        }
        return 0;
    }

    std::map<uint32_t, uint32_t> VertexArrayBuffer::getAllVertexCounts() const {
        std::map<uint32_t, uint32_t> counts;
        for (const auto& [binding, bufferData] : mBuffers) {
            counts[binding] = bufferData.vertexCount;
        }
        return counts;
    }

    bool VertexArrayBuffer::hasBinding(uint32_t binding) const {
        return mBuffers.find(binding) != mBuffers.end();
    }

    const VertexArrayBuffer::BufferData* VertexArrayBuffer::getBufferData(uint32_t binding) const {
        auto it = mBuffers.find(binding);
        return it != mBuffers.end() ? &it->second : nullptr;
    }

    uint32_t VertexArrayBuffer::getFormatSize(VkFormat format) {
        switch (format) {
        case VK_FORMAT_R32_SFLOAT: return 4;
        case VK_FORMAT_R32G32_SFLOAT: return 8;
        case VK_FORMAT_R32G32B32_SFLOAT: return 12;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return 16;
        case VK_FORMAT_R8G8B8A8_UNORM: return 4;
        case VK_FORMAT_R16G16_SFLOAT: return 4;
        default:
            throw std::runtime_error("Unsupported format");
        }
    }

    // 兼容性API实现
    void VertexArrayBuffer::beginSeparated(uint32_t binding) {
        mCurrentBinding = binding;
        mSeparatedAttributes.clear();
    }

    void VertexArrayBuffer::finishSeparated(uint32_t stride) {
        if (mSeparatedAttributes.empty()) {
            throw std::runtime_error("No attributes added");
        }

        size_t vertexCount = mSeparatedAttributes[0].data.size() /
            mSeparatedAttributes[0].elementSize;

        for (const auto& attr : mSeparatedAttributes) {
            size_t currentCount = attr.data.size() / attr.elementSize;
            if (currentCount != vertexCount) {
                throw std::runtime_error("Attribute data count mismatch");
            }
        }

        VertexLayout layout;
        layout.binding = mCurrentBinding;

        if (stride == 0) {
            layout.stride = 0;
            for (const auto& attr : mSeparatedAttributes) {
                layout.stride += attr.elementSize;
            }
        }
        else {
            layout.stride = stride;
        }

        uint32_t offset = 0;
        for (const auto& attr : mSeparatedAttributes) {
            layout.addAttribute(attr.location, attr.format, offset);
            offset += attr.elementSize;
        }

        std::vector<uint8_t> interleaved(vertexCount * layout.stride);
        for (size_t i = 0; i < vertexCount; ++i) {
            uint8_t* vertexPtr = interleaved.data() + i * layout.stride;
            uint32_t currentOffset = 0;

            for (const auto& attr : mSeparatedAttributes) {
                const uint8_t* src = attr.data.data() + i * attr.elementSize;
                uint8_t* dst = vertexPtr + currentOffset;
                memcpy(dst, src, attr.elementSize);
                currentOffset += attr.elementSize;
            }
        }

        uploadInternal(mCurrentBinding, interleaved.data(),
            interleaved.size(), layout, BufferMode::SEPARATED);

        mSeparatedAttributes.clear();
    }

    void VertexArrayBuffer::beginBinding(uint32_t binding, uint32_t stride) {
        beginSeparated(binding);
    }

    void VertexArrayBuffer::finishBinding() {
        finishSeparated();
    }

    // 内部方法实现
    void VertexArrayBuffer::uploadInternal(uint32_t binding, const void* data, size_t size,
        const VertexLayout& layout, BufferMode mode) {
        validateLayout(layout);

        BufferData bufferData;
        bufferData.layout = layout;
        bufferData.mode = mode;
        bufferData.vertexCount = (layout.stride > 0) ?
            static_cast<uint32_t>(size / layout.stride) : 0;

        bufferData.buffer = std::make_shared<VertexBuffer>(mLogicalDevice, mCommandPool);
        bufferData.buffer->uploadData(data, size);

        mBuffers[binding] = std::move(bufferData);
        updateDescriptions(layout);

        if (mode == BufferMode::SEPARATED) {
            std::cerr << "[Performance] Using separated vertex data for binding "
                << binding << ". Consider using upload() with interleaved data."
                << std::endl;
        }
    }

    void VertexArrayBuffer::validateLayout(const VertexLayout& layout) {
        if (layout.stride == 0) {
            throw std::runtime_error("Layout stride cannot be zero");
        }

        for (const auto& attr : layout.attributes) {
            if (attr.offset >= layout.stride) {
                throw std::runtime_error("Attribute offset exceeds stride");
            }

            uint32_t formatSize = getFormatSize(attr.format);
            if (attr.offset + formatSize > layout.stride) {
                throw std::runtime_error("Attribute exceeds stride");
            }
        }
    }

    void VertexArrayBuffer::updateDescriptions(const VertexLayout& layout) {
        bool bindingExists = false;
        for (auto& desc : mBindingDescriptions) {
            if (desc.binding == layout.binding) {
                desc.stride = layout.stride;
                bindingExists = true;
                break;
            }
        }

        if (!bindingExists) {
            VkVertexInputBindingDescription bindingDesc = {
                .binding = layout.binding,
                .stride = layout.stride,
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            };
            mBindingDescriptions.push_back(bindingDesc);
        }

        for (const auto& attr : layout.attributes) {
            bool attrExists = false;
            for (auto& desc : mAttributeDescriptions) {
                if (desc.location == attr.location && desc.binding == layout.binding) {
                    desc.format = attr.format;
                    desc.offset = static_cast<uint32_t>(attr.offset);
                    attrExists = true;
                    break;
                }
            }

            if (!attrExists) {
                VkVertexInputAttributeDescription attrDesc = {
                    .location = attr.location,
                    .binding = layout.binding,
                    .format = attr.format,
                    .offset = static_cast<uint32_t>(attr.offset)
                };
                mAttributeDescriptions.push_back(attrDesc);
            }
        }

        std::sort(mBindingDescriptions.begin(), mBindingDescriptions.end(),
            [](const auto& a, const auto& b) { return a.binding < b.binding; });

        std::sort(mAttributeDescriptions.begin(), mAttributeDescriptions.end(),
            [](const auto& a, const auto& b) {
                if (a.binding != b.binding) return a.binding < b.binding;
                return a.location < b.location;
            });
    }

    // 批量设置接口实现
    VertexArrayBuffer& VertexArrayBuffer::addBindings(
        const std::vector<VkVertexInputBindingDescription>& bindings) {
        for (const auto& binding : bindings) {
            bool exists = false;
            for (auto& desc : mBindingDescriptions) {
                if (desc.binding == binding.binding) {
                    desc = binding;
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                mBindingDescriptions.push_back(binding);
            }
        }
        return *this;
    }

    VertexArrayBuffer& VertexArrayBuffer::addAttributes(
        const std::vector<VkVertexInputAttributeDescription>& attributes) {
        for (const auto& attr : attributes) {
            bool exists = false;
            for (auto& desc : mAttributeDescriptions) {
                if (desc.location == attr.location && desc.binding == attr.binding) {
                    desc = attr;
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                mAttributeDescriptions.push_back(attr);
            }
        }
        return *this;
    }

    VertexArrayBuffer& VertexArrayBuffer::setBindings(
        const std::vector<VkVertexInputBindingDescription>& bindings) {
        mBindingDescriptions = bindings;
        return *this;
    }

    VertexArrayBuffer& VertexArrayBuffer::setAttributes(
        const std::vector<VkVertexInputAttributeDescription>& attributes) {
        mAttributeDescriptions = attributes;
        return *this;
    }
}