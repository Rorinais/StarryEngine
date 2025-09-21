#include "VertexArrayBuffer.hpp"
namespace StarryEngine {
    VertexArrayBuffer::Ptr VertexArrayBuffer::create(const LogicalDevice::Ptr& logicalDevice, const CommandPool::Ptr& commandPool) {
        return std::make_shared<VertexArrayBuffer>(logicalDevice, commandPool);
    }

    VertexArrayBuffer::VertexArrayBuffer(
        const LogicalDevice::Ptr& logicalDevice,
        const CommandPool::Ptr& commandPool
    ) : mLogicalDevice(logicalDevice), mCommandPool(commandPool) {
    }

    VertexArrayBuffer::~VertexArrayBuffer() { cleanup(); }

    void VertexArrayBuffer::cleanup() {
        mBindings.clear();
        mBindingDescriptions.clear();
        mAttributeDescriptions.clear();
    }

    // 开始定义顶点绑定
    void VertexArrayBuffer::beginBinding(uint32_t binding, uint32_t stride) {
        mCurrentBinding = binding;
        mBindings[binding] = BindingInfo();
        mBindings[binding].binding = binding;
        mBindings[binding].stride = stride;
        mBindings[binding].attributes.clear();
        mBindings[binding].buffer = std::make_shared<VertexBuffer>(mLogicalDevice, mCommandPool);
    }

    // 完成当前绑定，生成缓冲区
    void VertexArrayBuffer::finishBinding() {
        auto& binding = mBindings[mCurrentBinding];
        if (binding.attributes.empty()) return;

        // 验证属性一致性
        validateAttributes(binding);

        // 计算布局参数
        calculateLayout(binding);

        // 生成交错数据
        std::vector<uint8_t> bufferData = interleaveData(binding);

        // 上传数据到GPU
        binding.buffer->uploadData(bufferData.data(), bufferData.size());

        // 生成Vulkan描述信息
        generateVulkanDescriptions(binding);
    }

    // 获取缓冲区句柄列表（按绑定顺序）
    std::vector<VkBuffer> VertexArrayBuffer::getBufferHandles() const {
        std::vector<VkBuffer> handles;
        for (const auto& [id, binding] : mBindings) {
            handles.push_back(binding.buffer->getBuffer());
        }
        return handles;
    }

    // 获取顶点总数
    uint32_t VertexArrayBuffer::getVertexCount() const {
        if (mBindings.empty()) return 0;
        const auto& firstBinding = mBindings.begin()->second;
        if (firstBinding.attributes.empty()) return 0;
        return static_cast<uint32_t>(
            firstBinding.attributes[0].data.size() /
            firstBinding.attributes[0].elementSize
            );
    }

    // 获取偏移列表（用于vkCmdBindVertexBuffers）
    std::vector<VkDeviceSize> VertexArrayBuffer::getOffsets() const {
        std::vector<VkDeviceSize> offsets;
        for (const auto& [id, binding] : mBindings) {
            offsets.push_back(0); // 所有缓冲区都从0开始
        }
        return offsets;
    }

    void VertexArrayBuffer::validateAttributes(const BindingInfo& binding) {
        const size_t vertexCount = binding.attributes[0].data.size() /
            binding.attributes[0].elementSize;

        for (const auto& attr : binding.attributes) {
            const size_t currentCount = attr.data.size() / attr.elementSize;
            if (currentCount != vertexCount) {
                throw std::runtime_error(
                    "Attribute data count mismatch in binding " +
                    std::to_string(binding.binding) +
                    ": expected " + std::to_string(vertexCount) +
                    ", got " + std::to_string(currentCount)
                );
            }
        }
    }

    // 计算布局参数（偏移、stride等）
    void VertexArrayBuffer::calculateLayout(BindingInfo& binding) {
        // 自动计算stride
        if (binding.stride == 0) {
            binding.stride = 0;
            for (const auto& attr : binding.attributes) {
                binding.stride += attr.elementSize;
            }
        }

        // 验证手动设置的stride
        uint32_t minStride = 0;
        for (const auto& attr : binding.attributes) {
            minStride += attr.elementSize;
        }
        if (binding.stride < minStride) {
            throw std::runtime_error(
                "Stride " + std::to_string(binding.stride) +
                " is too small for attributes in binding " +
                std::to_string(binding.binding) +
                ", minimum is " + std::to_string(minStride)
            );
        }
    }

    // 生成交错数据
    std::vector<uint8_t> VertexArrayBuffer::interleaveData(const BindingInfo& binding) {
        const size_t vertexCount = binding.attributes[0].data.size() /
            binding.attributes[0].elementSize;
        std::vector<uint8_t> bufferData(vertexCount * binding.stride);

        // 为每个顶点设置数据
        for (size_t i = 0; i < vertexCount; ++i) {
            uint32_t offset = 0;
            for (const auto& attr : binding.attributes) {
                const uint8_t* src = attr.data.data() + i * attr.elementSize;
                uint8_t* dst = bufferData.data() + i * binding.stride + offset;
                memcpy(dst, src, attr.elementSize);
                offset += attr.elementSize;
            }
        }

        return bufferData;
    }

    // 生成Vulkan描述符
    void VertexArrayBuffer::generateVulkanDescriptions(const BindingInfo& binding) {
        // 绑定描述
        VkVertexInputBindingDescription bindingDesc{
            .binding = binding.binding,
            .stride = binding.stride,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };

        // 检查是否已存在相同的绑定描述
        bool bindingExists = false;
        for (auto& desc : mBindingDescriptions) {
            if (desc.binding == binding.binding) {
                bindingExists = true;
                desc = bindingDesc;
                break;
            }
        }

        if (!bindingExists) {
            mBindingDescriptions.push_back(bindingDesc);
        }

        // 属性描述
        uint32_t offset = 0;
        for (const auto& attr : binding.attributes) {
            VkVertexInputAttributeDescription attrDesc{
                .location = attr.location,
                .binding = binding.binding,
                .format = attr.format,
                .offset = offset
            };

            // 检查是否已存在相同的属性描述
            bool attrExists = false;
            for (auto& desc : mAttributeDescriptions) {
                if (desc.location == attr.location &&
                    desc.binding == binding.binding) {
                    attrExists = true;
                    desc = attrDesc;
                    break;
                }
            }

            if (!attrExists) {
                mAttributeDescriptions.push_back(attrDesc);
            }

            offset += attr.elementSize;
        }
    }

    uint32_t VertexArrayBuffer::getFormatSize(VkFormat format) {
        switch (format) {
        case VK_FORMAT_R32_SFLOAT: return 4;
        case VK_FORMAT_R32G32_SFLOAT: return 8;
        case VK_FORMAT_R32G32B32_SFLOAT: return 12;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return 16;
        default:
            throw std::runtime_error("Unsupported vertex format: " +
                std::to_string(format));
        }
    }
}
