#pragma once
#include <iostream>
#include <string>

namespace StarryEngine::RHI {
    // 平台无关的枚举和结构定义
    enum class Format {
        Undefined,
        R8_UNorm,
        RGBA8_UNorm,
        RGBA16_Float,
        Depth32_Float,
        Depth24_Stencil8,
        BC1_RGB_UNorm,
        BC3_UNorm
    };

    enum class BufferUsage {
        Vertex = 0x01,
        Index = 0x02,
        Uniform = 0x04,
        Storage = 0x08,
        Indirect = 0x10,
        TransferSrc = 0x20,
        TransferDst = 0x40
    };

    enum class MemoryType {
        GPU = 0,      // GPU专用内存
        CPU_To_GPU = 1,      // CPU到GPU内存（频繁更新）
        CPU_Only = 2,      // CPU专用内存
        GPU_To_CPU = 3       // GPU到CPU内存（读回）
    };

    enum class TextureType {
        Texture2D,
        Texture3D,
        TextureCube,
        Texture2DArray
    };

    enum class SamplerFilter {
        Nearest,
        Linear,
        Anisotropic
    };

    enum class SamplerAddressMode {
        Repeat,
        Mirror,
        Clamp,
        Border
    };

    enum class CompareOp {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    enum class ShaderStage {
        Vertex = 0x01,
        Fragment = 0x02,
        Compute = 0x04,
        Geometry = 0x08,
        TessControl = 0x10,
        TessEval = 0x20
    };

    enum class PrimitiveTopology {
        PointList,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        PatchList
    };

    enum class CullMode {
        None,
        Front,
        Back,
        FrontAndBack
    };

    enum class BlendFactor {
        Zero,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha
    };

    enum class BlendOp {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max
    };
}