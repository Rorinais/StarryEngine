#include"Geometry.hpp"

namespace StarryEngine::RenderGraph {
    VertexLayout& VertexLayout::addBinding(uint32_t binding, uint32_t stride, RHI::VertexInputRate inputRate) {
        mBindings[binding] = { stride, inputRate };
        return *this;
    }

    VertexLayout& VertexLayout::addAttribute(uint32_t location, uint32_t binding,
        RHI::Format format, uint32_t offset) {
        RHI::VertexAttribute attr{};
        attr.location = location;
        attr.binding = binding;
        attr.format = format;
        attr.offset = offset;
        mAttributes.push_back(attr);
        return *this;
    }

    VertexLayout& VertexLayout::addAttribute(uint32_t location, uint32_t binding, RHI::Format format) {
        uint32_t offset = getNextOffset(binding);
        uint32_t size = getFormatSize(format);
        addAttribute(location, binding, format, offset);
        mBindingCurrentOffsets[binding] = offset + size;
        return *this;
    }

    RHI::VertexInputState VertexLayout::build() const {
        RHI::VertexInputState state;
        for (const auto& [binding, info] : mBindings) {
            RHI::VertexBinding vb{};
            vb.binding = binding;
            vb.stride = info.stride;
            vb.inputRate = info.inputRate;
            state.bindings.push_back(vb);
        }
        state.attributes = mAttributes;
        return state;
    }

    uint32_t VertexLayout::getBindingStride(uint32_t binding) const {
        auto it = mBindings.find(binding);
        if (it != mBindings.end()) {
            return it->second.stride;
        }
        return 0;  // 未找到返回 0，调用者应检查
    }

    std::vector<uint32_t> VertexLayout::getBindings() const {
        std::vector<uint32_t> bindings;
        bindings.reserve(mBindings.size());
        for (const auto& [binding, _] : mBindings) {
            bindings.push_back(binding);
        }
        std::sort(bindings.begin(), bindings.end());
        return bindings;
    }

    uint32_t VertexLayout::getNextOffset(uint32_t binding) const {
        auto it = mBindingCurrentOffsets.find(binding);
        return (it == mBindingCurrentOffsets.end()) ? 0 : it->second;
    }

    uint32_t VertexLayout::getFormatSize(RHI::Format format) const {
        static const std::array<uint32_t, 79> formatSizes = {
            0,    // Undefined [0]
            1,    // R8_UNorm [1]
            1,    // R8_SNorm [2]
            1,    // R8_UInt [3]
            1,    // R8_SInt [4]
            1,    // R8_sRGB [5]
            2,    // R16_UNorm [6]
            2,    // R16_SNorm [7]
            2,    // R16_UInt [8]
            2,    // R16_SInt [9]
            2,    // R16_Float [10]
            2,    // RG8_UNorm [11]
            2,    // RG8_SNorm [12]
            2,    // RG8_UInt [13]
            2,    // RG8_SInt [14]
            4,    // R32_UInt [15]
            4,    // R32_SInt [16]
            4,    // R32_Float [17]
            4,    // RG16_UNorm [18]
            4,    // RG16_SNorm [19]
            4,    // RG16_UInt [20]
            4,    // RG16_SInt [21]
            4,    // RG16_Float [22]
            4,    // RGBA8_UNorm [23]
            4,    // RGBA8_SNorm [24]
            4,    // RGBA8_UInt [25]
            4,    // RGBA8_SInt [26]
            4,    // BGRA8_UNorm [27]
            4,    // BGRA8_SNorm [28]
            4,    // BGRA8_UInt [29]
            4,    // BGRA8_SInt [30]
            4,    // RGBA8_sRGB [31]
            4,    // BGRA8_sRGB [32]
            8,    // RG32_UInt [33]
            8,    // RG32_SInt [34]
            8,    // RG32_Float [35]
            8,    // RGBA16_UNorm [36]
            8,    // RGBA16_SNorm [37]
            8,    // RGBA16_UInt [38]
            8,    // RGBA16_SInt [39]
            8,    // RGBA16_Float [40]
            12,   // RGB32_UInt [41]
            12,   // RGB32_SInt [42]
            12,   // RGB32_Float [43]
            16,   // RGBA32_UInt [44]
            16,   // RGBA32_SInt [45]
            16,   // RGBA32_Float [46]
            2,    // D16_UNorm [47]
            4,    // D32_Float [48]
            4,    // D24_UNorm_S8_UInt [49]
            5,    // D32_Float_S8_UInt [50]
            8,    // BC1_RGB_UNorm [51]
            8,    // BC1_RGBA_UNorm [52]
            8,    // BC1_RGB_sRGB [53]
            8,    // BC1_RGBA_sRGB [54]
            16,   // BC2_UNorm [55]
            16,   // BC2_sRGB [56]
            16,   // BC3_UNorm [57]
            16,   // BC3_sRGB [58]
            8,    // BC4_UNorm [59]
            8,    // BC4_SNorm [60]
            16,   // BC5_UNorm [61]
            16,   // BC5_SNorm [62]
            16,   // BC6H_UF16 [63]
            16,   // BC6H_SF16 [64]
            16,   // BC7_UNorm [65]
            16,   // BC7_sRGB [66]
            16,   // ASTC_4x4_UNorm [67]
            16,   // ASTC_4x4_sRGB [68]
            16,   // ASTC_8x8_UNorm [69]
            16,   // ASTC_8x8_sRGB [70]
            8,    // ETC2_RGB8_UNorm [71]
            8,    // ETC2_RGB8_sRGB [72]
            16,   // ETC2_RGBA8_UNorm [73]
            16,   // ETC2_RGBA8_sRGB [74]
            8,    // EAC_R11_UNorm [75]
            8,    // EAC_R11_SNorm [76]
            16,   // EAC_RG11_UNorm [77]
            16    // EAC_RG11_SNorm [78]
        };
        size_t index = static_cast<size_t>(format);
        return (index < formatSizes.size()) ? formatSizes[index] : 0;
    }

    Geometry::Geometry(std::shared_ptr<RHI::ResourceManager> resMgr) : mResMgr(resMgr) {}

    void Geometry::setVertexBuffer(uint32_t binding, const std::vector<float>& vertices,
        const VertexLayout& layout, const std::string& debugName) {
        mLayout = layout;  // 保存布局，后续用于获取输入状态和 binding 列表

        uint32_t stride = layout.getBindingStride(binding);
        if (stride == 0) {
            std::cerr << "[Geometry] Binding " << binding << " not found in vertex layout for "
                << debugName << std::endl;
            return;
        }

        RHI::BufferDesc bufferDesc;
        bufferDesc.size = vertices.size() * sizeof(float);
        bufferDesc.stride = stride;
        bufferDesc.type = RHI::BufferType::Vertex;
        bufferDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
        bufferDesc.allowUpdate = true;
        bufferDesc.debugName = debugName;

        RHI::BufferHandle handle = mResMgr->createBuffer(bufferDesc);
        if (!handle.isValid()) {
            std::cerr << "[Geometry] Failed to create vertex buffer for binding " << binding
                << ": " << debugName << std::endl;
            return;
        }

        auto* buffer = mResMgr->getBuffer(handle);
        buffer->update(vertices.data(), vertices.size() * sizeof(float));

        mVertexBufferHandles[binding] = handle;
    }

    void Geometry::setVertexBuffer(const std::vector<float>& vertices,
        const VertexLayout& layout, const std::string& debugName) {
        setVertexBuffer(0, vertices, layout, debugName);  // 默认 binding 0
    }

    void Geometry::setIndexBuffer(const std::vector<uint32_t>& indices, const std::string& debugName) {
        RHI::BufferDesc bufferDesc;
        bufferDesc.size = indices.size() * sizeof(uint32_t);
        bufferDesc.stride = sizeof(uint32_t);
        bufferDesc.type = RHI::BufferType::Index;
        bufferDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
        bufferDesc.allowUpdate = true;
        bufferDesc.debugName = debugName;

        mIndexBufferHandle = mResMgr->createBuffer(bufferDesc);
        if (!mIndexBufferHandle.isValid()) {
            std::cerr << "[Geometry] Failed to create index buffer: " << debugName << std::endl;
            return;
        }
        auto* buffer = mResMgr->getBuffer(mIndexBufferHandle);
        buffer->update(indices.data(), indices.size() * sizeof(uint32_t));
        mIndexCount = static_cast<uint32_t>(indices.size());
    }

    RHI::BufferHandle Geometry::getVertexBufferHandle(uint32_t binding) const {
        auto it = mVertexBufferHandles.find(binding);
        if (it != mVertexBufferHandles.end()) {
            return it->second;
        }
        return RHI::BufferHandle::Null();
    }

    std::vector<uint32_t> Geometry::getBindings() const {
        return mLayout.getBindings();
    }

    RHI::VertexInputState Geometry::getVertexInputState() const {
        return mLayout.build();
    }
}