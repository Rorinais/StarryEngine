#pragma once
#include <iostream>
#include <string>

namespace StarryEngine::RHI {
    struct BufferDesc {
        uint64_t size = 0;
        BufferUsage usage = BufferUsage::Vertex;
        MemoryType memoryType = MemoryType::GPU;
        std::string debugName;
        bool allowUpdate = false;
        bool allowReadback = false;
        const void* initialData = nullptr;
        size_t initialDataSize = 0;
    };

    struct TextureDesc {
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        Format format = Format::RGBA8_UNorm;
        TextureType type = TextureType::Texture2D;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        bool generateMips = false;
        std::string debugName;
        bool allowRenderTarget = false;
        bool allowDepthStencil = false;
        bool allowUnorderedAccess = false;
        bool allowSimultaneousAccess = false;
    };

    struct SamplerDesc {
        SamplerFilter minFilter = SamplerFilter::Linear;
        SamplerFilter magFilter = SamplerFilter::Linear;
        SamplerFilter mipFilter = SamplerFilter::Linear;
        SamplerAddressMode addressU = SamplerAddressMode::Repeat;
        SamplerAddressMode addressV = SamplerAddressMode::Repeat;
        SamplerAddressMode addressW = SamplerAddressMode::Repeat;
        float mipLodBias = 0.0f;
        float maxAnisotropy = 1.0f;
        float minLod = 0.0f;
        float maxLod = 1000.0f;
        bool compareEnable = false;
        CompareOp compareOp = CompareOp::Always;
        std::string debugName;
    };

    struct Viewport {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float minDepth = 0.0f;
        float maxDepth = 1.0f;
    };

    struct Rect {
        int32_t x = 0;
        int32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct ClearValue {
        float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        float depth = 1.0f;
        uint32_t stencil = 0;
    };

    struct VertexAttribute {
        uint32_t location = 0;
        Format format = Format::Undefined;
        uint32_t offset = 0;
        std::string semanticName;
    };

    struct VertexLayout {
        std::vector<VertexAttribute> attributes;
        uint32_t stride = 0;
        bool perInstance = false;
    };

    struct ShaderModuleDesc {
        ShaderStage stage = ShaderStage::Vertex;
        std::vector<uint8_t> code;  // SPIR-V字节码
        std::string entryPoint = "main";
        std::string debugName;
    };

    struct PipelineLayoutDesc {
        std::vector<std::vector<DescriptorSetLayoutBinding>> descriptorSets;
        std::vector<PushConstantRange> pushConstants;
        std::string debugName;
    };

    struct GraphicsPipelineDesc {
        std::vector<ShaderModuleDesc> shaders;
        VertexLayout vertexLayout;
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;
        CullMode cullMode = CullMode::Back;
        bool frontCounterClockwise = false;
        float depthBiasConstant = 0.0f;
        float depthBiasClamp = 0.0f;
        float depthBiasSlope = 0.0f;
        bool depthTestEnable = true;
        bool depthWriteEnable = true;
        CompareOp depthCompareOp = CompareOp::Less;
        bool stencilTestEnable = false;
        bool blendEnable = false;
        BlendFactor srcColorBlendFactor = BlendFactor::One;
        BlendFactor dstColorBlendFactor = BlendFactor::Zero;
        BlendOp colorBlendOp = BlendOp::Add;
        BlendFactor srcAlphaBlendFactor = BlendFactor::One;
        BlendFactor dstAlphaBlendFactor = BlendFactor::Zero;
        BlendOp alphaBlendOp = BlendOp::Add;
        std::vector<Format> renderTargetFormats;
        Format depthStencilFormat = Format::Undefined;
        PipelineLayoutDesc layoutDesc;
        std::string debugName;
    };

    struct ComputePipelineDesc {
        ShaderModuleDesc computeShader;
        PipelineLayoutDesc layoutDesc;
        std::string debugName;
    };

    // 句柄类型
    struct BufferHandle {
        uint64_t id = 0;
        bool isValid() const { return id != 0; }
        bool operator==(const BufferHandle& other) const { return id == other.id; }
        bool operator!=(const BufferHandle& other) const { return id != other.id; }
        bool operator<(const BufferHandle& other) const { return id < other.id; }
    };

    struct TextureHandle {
        uint64_t id = 0;
        bool isValid() const { return id != 0; }
        bool operator==(const TextureHandle& other) const { return id == other.id; }
        bool operator!=(const TextureHandle& other) const { return id != other.id; }
        bool operator<(const TextureHandle& other) const { return id < other.id; }
    };

    struct SamplerHandle {
        uint64_t id = 0;
        bool isValid() const { return id != 0; }
        bool operator==(const SamplerHandle& other) const { return id == other.id; }
        bool operator!=(const SamplerHandle& other) const { return id != other.id; }
    };

    struct ShaderModuleHandle {
        uint64_t id = 0;
        bool isValid() const { return id != 0; }
        bool operator==(const ShaderModuleHandle& other) const { return id == other.id; }
        bool operator!=(const ShaderModuleHandle& other) const { return id != other.id; }
    };

    struct PipelineLayoutHandle {
        uint64_t id = 0;
        bool isValid() const { return id != 0; }
        bool operator==(const PipelineLayoutHandle& other) const { return id == other.id; }
        bool operator!=(const PipelineLayoutHandle& other) const { return id != other.id; }
    };

    struct PipelineHandle {
        uint64_t id = 0;
        bool isValid() const { return id != 0; }
        bool operator==(const PipelineHandle& other) const { return id == other.id; }
        bool operator!=(const PipelineHandle& other) const { return id != other.id; }
    };

    struct RenderPassHandle {
        uint64_t id = 0;
        bool isValid() const { return id != 0; }
        bool operator==(const RenderPassHandle& other) const { return id == other.id; }
        bool operator!=(const RenderPassHandle& other) const { return id != other.id; }
    };

    struct FramebufferHandle {
        uint64_t id = 0;
        bool isValid() const { return id != 0; }
        bool operator==(const FramebufferHandle& other) const { return id == other.id; }
        bool operator!=(const FramebufferHandle& other) const { return id != other.id; }
    };

}