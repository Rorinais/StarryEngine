#pragma once
#include <cstdint>
#include <string>

namespace StarryEngine::RHI {
    // ==================== 基础枚举 ====================
    enum class API {
        Vulkan,
        DirectX12,
        DirectX11,
        Metal,
        OpenGL,
        OpenGLES
    };

    enum class FeatureLevel {
        ES_100,    // OpenGL ES 1.0
        ES_200,    // OpenGL ES 2.0
        ES_300,    // OpenGL ES 3.0
        ES_310,    // OpenGL ES 3.1
        GL_110,    // OpenGL 1.1
        GL_120,    // OpenGL 1.2
        GL_130,    // OpenGL 1.3
        GL_140,    // OpenGL 1.4
        GL_150,    // OpenGL 1.5
        GL_200,    // OpenGL 2.0
        GL_210,    // OpenGL 2.1
        GL_300,    // OpenGL 3.0
        GL_310,    // OpenGL 3.1
        GL_320,    // OpenGL 3.2
        GL_330,    // OpenGL 3.3
        GL_400,    // OpenGL 4.0
        GL_410,    // OpenGL 4.1
        GL_420,    // OpenGL 4.2
        GL_430,    // OpenGL 4.3
        GL_440,    // OpenGL 4.4
        GL_450,    // OpenGL 4.5
        GL_460,    // OpenGL 4.6
        DX_9_1,    // DirectX 9.1
        DX_9_2,    // DirectX 9.2
        DX_9_3,    // DirectX 9.3
        DX_10_0,   // DirectX 10.0
        DX_10_1,   // DirectX 10.1
        DX_11_0,   // DirectX 11.0
        DX_11_1,   // DirectX 11.1
        DX_12_0,   // DirectX 12.0
        DX_12_1,   // DirectX 12.1
        DX_12_2,   // DirectX 12.2
        VK_1_0,    // Vulkan 1.0
        VK_1_1,    // Vulkan 1.1
        VK_1_2,    // Vulkan 1.2
        VK_1_3,    // Vulkan 1.3
        MT_1_0,    // Metal 1.0
        MT_1_1,    // Metal 1.1
        MT_1_2,    // Metal 1.2
        MT_2_0,    // Metal 2.0
        MT_2_1,    // Metal 2.1
        MT_2_2,    // Metal 2.2
        MT_2_3,    // Metal 2.3
        MT_2_4,    // Metal 2.4
        MT_3_0     // Metal 3.0
    };

    // ==================== 资源类别 ====================
    enum class ResourceCategory : uint8_t {
        Buffer = 1,
        Texture = 2,
        Pipeline = 3,
        PipelineLayout = 4,
        Shader = 5,
        RenderPass = 6,
        Framebuffer = 7,
        DescriptorSet = 8,
        DescriptorPool = 9,
        DescriptorSetLayout = 10,  
        Sampler = 11,
        QueryPool = 12,
        CommandBuffer = 13,
        CommandPool = 14,
        Fence = 15,
        Semaphore = 16,
        Event = 17,
        SwapChain = 18,
        AccelerationStructure = 19,  
        Queue = 20,                 
        MAX_CATEGORIES
    };

    enum class ImageViewType {
        Auto,           
        Texture1D,
        Texture1DArray,
        Texture2D,
        Texture2DArray,
        Texture3D,
        TextureCube,
        TextureCubeArray
    };

    enum class DynamicState : uint32_t {
        Viewport = 0,
        Scissor,
        LineWidth,
        DepthBias,
        BlendConstants,
        DepthBounds,
        StencilCompareMask,
        StencilWriteMask,
        StencilReference,
        VertexInputBindingStride,
        PrimitiveTopology,
        CullMode,
        FrontFace,
        PolygonMode,
        ColorWriteEnable,
        SampleLocations,
        DiscardRectangle,
        ConservativeRasterization,

        Count  
    };

    // ==================== 加速结构相关枚举 ====================

    /**
     * @brief 加速结构类型枚举
     */
    enum class AccelerationStructureType {
        BottomLevel = 0,      ///< 底层加速结构
        TopLevel = 1,         ///< 顶层加速结构
        Generic = 2           ///< 通用加速结构
    };

    /**
     * @brief 加速结构构建标志枚举
     */
    enum class AccelerationStructureBuildFlags {
        None = 0,
        AllowUpdate = 1 << 0,                ///< 允许更新
        AllowCompaction = 1 << 1,            ///< 允许压缩
        PreferFastTrace = 1 << 2,            ///< 优先快速追踪
        PreferFastBuild = 1 << 3,            ///< 优先快速构建
        MinimizeMemory = 1 << 4,             ///< 最小化内存使用
        PerformUpdate = 1 << 5,              ///< 执行更新
        LowMemory = 1 << 6                   ///< 低内存模式
    };

    /**
     * @brief 加速结构构建模式枚举
     */
    enum class AccelerationStructureBuildMode {
        Build = 0,           ///< 构建
        Update = 1           ///< 更新
    };

    /**
     * @brief 几何体标志枚举
     */
    enum class GeometryFlags {
        None = 0,
        Opaque = 1 << 0,                     ///< 不透明几何体
        NoDuplicateAnyHitInvocation = 1 << 1, ///< 不复制任何击中调用
        OpaqueForCulling = 1 << 2,           ///< 剔除时不透明
        TriangleFrontCounterclockwise = 1 << 3 ///< 三角形正面为逆时针
    };

    /**
     * @brief 复制加速结构模式枚举
     */
    enum class CopyAccelerationStructureMode {
        Clone = 0,           ///< 克隆
        Compact = 1,         ///< 压缩
        Serialize = 2,       ///< 序列化
        Deserialize = 3      ///< 反序列化
    };

    // ==================== 队列相关枚举 ====================

    /**
     * @brief 队列类型枚举
     */
    enum class QueueType {
        Graphics = 0,        ///< 图形队列
        Compute = 1,         ///< 计算队列
        Transfer = 2,        ///< 传输队列
		Present = 3,		 ///< 显示队列
        SparseBinding = 4,   ///< 稀疏绑定队列
        Protected = 5        ///< 受保护队列
    };

    enum class ColorSpace {
        SRGBNonlinear,
        ExtendedSRGBLinear,
        HDR10_ST2084,
        HDR10_HLG,
        DCI_P3,
        DisplayP3,
        AdobeRGB,
        BT2020,
        BT709
    };

    enum class ValidationLevel {
        Disabled,
        Minimal,
        Basic,
        Full
    };

    enum class QueryControlFlags : uint32_t {
        None = 0,
        Precise = 0x01
    };

    enum class QueryResultFlags : uint32_t {
        None = 0,
        _64Bit = 0x01,
        Wait = 0x02,
        WithAvailability = 0x04,
        Partial = 0x08
    };

    // ==================== 资源类型枚举 ====================
    enum class BufferType {
        Vertex,
        Index,
        Uniform,
        Storage,
        Staging,
        Indirect,
        Structured,
        Constant,
        Raw,
        AccelerationStructure,
        ShaderBindingTable
    };

    enum class TextureType {
        Texture1D,
        Texture2D,
        Texture3D,
        TextureCube,
        Texture1DArray,
        Texture2DArray,
        TextureCubeArray,
        Texture2DMultisample,
        Texture2DMultisampleArray
    };

    enum class TextureDimension :uint8_t {
        Unknown = 0,
        Tex1D,
        Tex2D,
        Tex3D,
        Cube,
        Tex2DArray,
        CubeArray
    };

    enum class Format {
        // 未定义格式
        Undefined,

        // 8位无符号整型
        R8_UNorm,
        R8_SNorm,
        R8_UInt,
        R8_SInt,

        // 8位sRGB
        R8_sRGB,

        // 16位
        R16_UNorm,
        R16_SNorm,
        R16_UInt,
        R16_SInt,
        R16_Float,
        RG8_UNorm,
        RG8_SNorm,
        RG8_UInt,
        RG8_SInt,

        // 32位
        R32_UInt,
        R32_SInt,
        R32_Float,
        RG16_UNorm,
        RG16_SNorm,
        RG16_UInt,
        RG16_SInt,
        RG16_Float,
        RGBA8_UNorm,
        RGBA8_SNorm,
        RGBA8_UInt,
        RGBA8_SInt,
        BGRA8_UNorm,
        BGRA8_SNorm,
        BGRA8_UInt,
        BGRA8_SInt,

        // sRGB格式
        RGBA8_sRGB,
        BGRA8_sRGB,

        // 64位
        RG32_UInt,
        RG32_SInt,
        RG32_Float,
        RGBA16_UNorm,
        RGBA16_SNorm,
        RGBA16_UInt,
        RGBA16_SInt,
        RGBA16_Float,

        // 96位
        RGB32_UInt,
        RGB32_SInt,
        RGB32_Float,

        // 128位
        RGBA32_UInt,
        RGBA32_SInt,
        RGBA32_Float,

        // 深度/模板格式
        D16_UNorm,
        D32_Float,
        D24_UNorm_S8_UInt,
        D32_Float_S8_UInt,

        // BC压缩格式
        BC1_RGB_UNorm,
        BC1_RGBA_UNorm,
        BC1_RGB_sRGB,
        BC1_RGBA_sRGB,
        BC2_UNorm,
        BC2_sRGB,
        BC3_UNorm,
        BC3_sRGB,
        BC4_UNorm,
        BC4_SNorm,
        BC5_UNorm,
        BC5_SNorm,
        BC6H_UF16,
        BC6H_SF16,
        BC7_UNorm,
        BC7_sRGB,

        // ASTC压缩格式
        ASTC_4x4_UNorm,
        ASTC_4x4_sRGB,
        ASTC_8x8_UNorm,
        ASTC_8x8_sRGB,

        // ETC2/EAC压缩格式
        ETC2_RGB8_UNorm,
        ETC2_RGB8_sRGB,
        ETC2_RGBA8_UNorm,
        ETC2_RGBA8_sRGB,
        EAC_R11_UNorm,
        EAC_R11_SNorm,
        EAC_RG11_UNorm,
        EAC_RG11_SNorm
    };

    enum class MemoryType {
        GPU_Only,      // GPU专用内存（最快）
        CPU_To_GPU,    // CPU到GPU内存（频繁更新）
        CPU_Only,      // CPU专用内存
        GPU_To_CPU,    // GPU到CPU内存（读回）
        CPU_Cached,    // CPU缓存内存（频繁读取）
        Write_Combined // 写合并内存（频繁写入）
    };

    enum class MemoryHeap {
        Default,
        Upload,
        Readback,
        Custom
    };

    enum class ImageSubresourceFlags : uint32_t {
        None = 0,
        MipLevel = 0x01,
        ArrayLayer = 0x02,
        Plane = 0x04
    };

    enum class FilterMode {
        Nearest,
        Linear,
        Cubic
    };

    enum class SamplerMipmapMode {
        Nearest,
        Linear
    };

    enum class FormatFeatureFlags : uint32_t {
        None = 0,
        SampledImage = 0x00000001,
        StorageImage = 0x00000002,
        StorageImageAtomic = 0x00000004,
        UniformTexelBuffer = 0x00000008,
        StorageTexelBuffer = 0x00000010,
        StorageTexelBufferAtomic = 0x00000020,
        VertexBuffer = 0x00000040,
        ColorAttachment = 0x00000080,
        ColorAttachmentBlend = 0x00000100,
        DepthStencilAttachment = 0x00000200,
        BlitSrc = 0x00000400,
        BlitDst = 0x00000800,
        SampledImageFilterLinear = 0x00001000,
        SampledImageFilterCubic = 0x00002000,
        TransferSrc = 0x00004000,
        TransferDst = 0x00008000,
        MidpointChromaSamples = 0x00010000,
        SampledImageYCbCrConversionLinearFilter = 0x00020000,
        SampledImageYCbCrConversionSeparateReconstructionFilter = 0x00040000,
        SampledImageYCbCrConversionChromaReconstructionExplicit = 0x00080000,
        SampledImageYCbCrConversionChromaReconstructionExplicitForceable = 0x00100000,
        Disjoint = 0x00200000,
        CositedChromaSamples = 0x00400000,
        SampledImageFilterMinmax = 0x01000000,
        VideoDecodeOutput = 0x02000000,
        VideoDecodeDPB = 0x04000000,
        AccelerationStructureVertexBuffer = 0x08000000,
        FragmentDensityMap = 0x10000000,
        FragmentShadingRateAttachment = 0x20000000,
        All = 0x7FFFFFFF
    };

    // ==================== 着色器阶段枚举 ====================
    /**
     * @brief 着色器阶段（单值，用于标识单个 shader module 的类型）
     */
    enum class ShaderStage : uint32_t {
        Vertex = 0x0001,
        TessellationControl = 0x0002,
        TessellationEvaluation = 0x0004,
        Geometry = 0x0008,
        Fragment = 0x0010,
        Compute = 0x0020,
        Amplification = 0x0040,
        Mesh = 0x0080,
        RayGen = 0x0100,
        AnyHit = 0x0200,
        ClosestHit = 0x0400,
        Miss = 0x0800,
        Intersection = 0x1000,
        Callable = 0x2000,
    };

    /**
     * @brief 着色器阶段标志集（位掩码，一个值可包含多个阶段）
     */
    struct ShaderStageFlags {
        uint32_t bits = 0;

        // ── 所有构造函数都标记 constexpr ──
        constexpr ShaderStageFlags() = default;
        constexpr ShaderStageFlags(ShaderStage s) : bits(static_cast<uint32_t>(s)) {}
        explicit constexpr ShaderStageFlags(uint32_t rawBits) : bits(rawBits) {}

        // ── 运算符也加上 constexpr（可选，但推荐，可支持编译期位运算）──
        constexpr ShaderStageFlags& operator|=(ShaderStage s) {
            bits |= static_cast<uint32_t>(s);
            return *this;
        }
        constexpr ShaderStageFlags& operator&=(ShaderStage s) {
            bits &= static_cast<uint32_t>(s);
            return *this;
        }

        friend constexpr ShaderStageFlags operator|(ShaderStageFlags f, ShaderStage s) {
            return ShaderStageFlags{ f.bits | static_cast<uint32_t>(s) };
        }
        friend constexpr ShaderStageFlags operator&(ShaderStageFlags f, ShaderStage s) {
            return ShaderStageFlags{ f.bits & static_cast<uint32_t>(s) };
        }
        friend constexpr ShaderStageFlags operator~(ShaderStageFlags f) {
            constexpr uint32_t kValidMask = 0x3FFF;
            return ShaderStageFlags{ f.bits & kValidMask };
        }

        constexpr bool Has(ShaderStage s) const {
            return (bits & static_cast<uint32_t>(s)) != 0;
        }

        explicit constexpr operator uint32_t() const { return bits; }

        constexpr bool operator==(const ShaderStageFlags&) const = default;

        static constexpr ShaderStageFlags AllGraphics() {
            return ShaderStageFlags{ 0x00FFu };  
        }
        static constexpr ShaderStageFlags AllRayTracing() {
            return ShaderStageFlags{ 0x3F00u };
        }
        static constexpr ShaderStageFlags All() {
            return ShaderStageFlags{ 0x3FFFu };
        }
    };

    inline constexpr ShaderStageFlags operator|(ShaderStage lhs, ShaderStage rhs) {
        return ShaderStageFlags{
            static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs)
        };
    }

    // ==================== 采样器枚举 ====================
    enum class SamplerFilter {
        Nearest,
        Linear,
        Anisotropic,
        Cubic,
        MinLinearMagNearestMipLinear,
        MinLinearMagNearestMipNearest,
        MinNearestMagLinearMipLinear,
        MinNearestMagLinearMipNearest,
        MinNearestMagNearestMipLinear,
        MinNearestMagNearestMipNearest
    };

    enum class SamplerAddressMode {
        Repeat,
        MirrorRepeat,
        ClampToEdge,
        ClampToBorder,
        MirrorClampToEdge
    };

    enum class SamplerBorderColor {
        TransparentBlack,
        OpaqueBlack,
        OpaqueWhite,
        Custom
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

    // ==================== 管线状态枚举 ====================

    enum class PrimitiveTopology {
        PointList,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        TriangleFan,
        LineListWithAdjacency,
        LineStripWithAdjacency,
        TriangleListWithAdjacency,
        TriangleStripWithAdjacency,
        PatchList
    };

    enum class CullMode {
        None,
        Front,
        Back,
        FrontAndBack
    };

    enum class PolygonMode {
        Fill,
        Line,
        Point
    };

    enum class FrontFace {
        Clockwise,
        CounterClockwise
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
        OneMinusDstAlpha,
        ConstantColor,
        OneMinusConstantColor,
        ConstantAlpha,
        OneMinusConstantAlpha,
        SrcAlphaSaturate,
        Src1Color,
        OneMinusSrc1Color,
        Src1Alpha,
        OneMinusSrc1Alpha
    };

    enum class StencilFace {
        Front,
        Back,
        FrontAndBack
    };

    enum class BlendOp {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max
    };

    enum class LogicOp {
        Clear,
        And,
        AndReverse,
        Copy,
        AndInverted,
        NoOp,
        Xor,
        Or,
        Nor,
        Equivalent,
        Invert,
        OrReverse,
        CopyInverted,
        OrInverted,
        Nand,
        Set
    };

    enum class StencilOp {
        Keep,
        Zero,
        Replace,
        IncrementAndClamp,
        DecrementAndClamp,
        Invert,
        IncrementAndWrap,
        DecrementAndWrap
    };

    enum class ColorComponent {
        R = 0x01,
        G = 0x02,
        B = 0x04,
        A = 0x08,
        All = R | G | B | A
    };

    enum class IndexType {
        UInt16,
        UInt32
    };

    enum class VertexInputRate {
        PerVertex,
        PerInstance
    };

    enum class PipelineBindPoint {
        Graphics,
        Compute,
        RayTracing
    };

    enum class PipelineStageFlags2 : uint64_t {
        None = 0,
        TopOfPipe = 0x00000001,
        DrawIndirect = 0x00000002,
        VertexInput = 0x00000004,
        VertexShader = 0x00000008,
        TessellationControlShader = 0x00000010,
        TessellationEvaluationShader = 0x00000020,
        GeometryShader = 0x00000040,
        FragmentShader = 0x00000080,
        EarlyFragmentTests = 0x00000100,
        LateFragmentTests = 0x00000200,
        ColorAttachmentOutput = 0x00000400,
        ComputeShader = 0x00000800,
        AllTransfer = 0x00001000,
        BottomOfPipe = 0x00002000,
        Host = 0x00004000,
        AllGraphics = 0x00008000,
        AllCommands = 0x00010000,
        Copy = 0x100000000,
        Resolve = 0x200000000,
        Blit = 0x400000000,
        Clear = 0x800000000,
        IndexInput = 0x1000000000,
        VertexAttributeInput = 0x2000000000,
        PreRasterizationShaders = 0x4000000000,
        VideoDecode = 0x04000000,
        VideoEncode = 0x08000000,
        TransformFeedback = 0x01000000,
        ConditionalRendering = 0x00040000,
        CommandPreprocess = 0x00020000,
        FragmentShadingRateAttachment = 0x00400000,
        AccelerationStructureBuild = 0x02000000,
        RayTracingShader = 0x00200000,
        FragmentDensityProcess = 0x00800000,
        TaskShader = 0x00080000,
        MeshShader = 0x00100000
    };

    // ==================== 资源视图枚举 ====================
    enum class DescriptorType {
        Sampler,
        CombinedImageSampler,
        SampledImage,
        StorageImage,
        UniformTexelBuffer,
        StorageTexelBuffer,
        UniformBuffer,
        StorageBuffer,
        UniformBufferDynamic,
        StorageBufferDynamic,
        InputAttachment,
        AccelerationStructure,
        InlineUniformBlock
    };

    enum class ImageAspect {
        Color = 0x01,
        Depth = 0x02,
        Stencil = 0x04,
        DepthStencil = Depth | Stencil,
        Metadata = 0x08,
        Plane0 = 0x10,
        Plane1 = 0x20,
        Plane2 = 0x40,
        MemoryPlane0 = 0x80,
        MemoryPlane1 = 0x100,
        MemoryPlane2 = 0x200,
        MemoryPlane3 = 0x400
    };

    // ==================== 命令缓冲区枚举 ====================
    enum class CommandBufferLevel {
        Primary,
        Secondary
    };

    enum class CommandBufferType {
        Graphics,
        Compute,
        Copy,
        Present
    };

    //enum class QueueType {
    //    Graphics,
    //    Compute,
    //    Transfer,
    //    Present,
    //    VideoDecode,
    //    VideoEncode,
    //    OpticalFlow
    //};

    enum class PipelineStage {
        TopOfPipe = 0x00000001,
        DrawIndirect = 0x00000002,
        VertexInput = 0x00000004,
        VertexShader = 0x00000008,
        TessellationControlShader = 0x00000010,
        TessellationEvaluationShader = 0x00000020,
        GeometryShader = 0x00000040,
        FragmentShader = 0x00000080,
        EarlyFragmentTests = 0x00000100,
        LateFragmentTests = 0x00000200,
        ColorAttachmentOutput = 0x00000400,
        ComputeShader = 0x00000800,
        Transfer = 0x00001000,
        BottomOfPipe = 0x00002000,
        Host = 0x00004000,
        AllGraphics = 0x00008000,
        AllCommands = 0x00010000,
        RayTracingShader = 0x00200000,
        AccelerationStructureBuild = 0x02000000,
        TaskShader = 0x00080000,
        MeshShader = 0x00100000
    };

    enum class AccessFlag {
        None = 0,
        IndirectCommandRead = 0x00000001,
        IndexRead = 0x00000002,
        VertexAttributeRead = 0x00000004,
        UniformRead = 0x00000008,
        InputAttachmentRead = 0x00000010,
        ShaderRead = 0x00000020,
        ShaderWrite = 0x00000040,
        ColorAttachmentRead = 0x00000080,
        ColorAttachmentWrite = 0x00000100,
        DepthStencilAttachmentRead = 0x00000200,
        DepthStencilAttachmentWrite = 0x00000400,
        TransferRead = 0x00000800,
        TransferWrite = 0x00001000,
        HostRead = 0x00002000,
        HostWrite = 0x00004000,
        MemoryRead = 0x00008000,
        MemoryWrite = 0x00010000,
        AccelerationStructureRead = 0x00200000,
        AccelerationStructureWrite = 0x00400000,
        ShaderSampledRead = 0x10000000,
        ShaderStorageRead = 0x20000000,
        ShaderStorageWrite = 0x40000000
    };

    // ==================== 图像布局枚举 ====================
    enum class ImageLayout {
        Undefined,
        General,
        ColorAttachment,
        DepthStencilAttachment,
        DepthStencilReadOnly,
        ShaderReadOnly,
        TransferSrc,
        TransferDst,
        Preinitialized,
        PresentSrc,
        DepthReadOnlyStencilAttachment,
        DepthAttachmentStencilReadOnly,
        DepthReadOnly,
        StencilReadOnly,
        ReadOnly,
        Attachment,
        ReadOnlyAttachment
    };

    // ==================== 渲染通道枚举 ====================
    enum class AttachmentLoadOp {
        Load,
        Clear,
        DontCare
    };

    enum class AttachmentStoreOp {
        Store,
        DontCare,
        None
    };

    enum class SubpassContents {
        Inline,
        SecondaryCommandBuffers
    };

    enum class DependencyFlags : uint32_t {
        None = 0,
        ByRegion = 0x01,
        DeviceGroup = 0x02,
        ViewLocal = 0x04,
        ViewGlobal = 0x08
    };

    enum class AccessFlags2 : uint64_t {
        None = 0,
        IndirectCommandRead = 0x00000001,
        IndexRead = 0x00000002,
        VertexAttributeRead = 0x00000004,
        UniformRead = 0x00000008,
        InputAttachmentRead = 0x00000010,
        ShaderRead = 0x00000020,
        ShaderWrite = 0x00000040,
        ColorAttachmentRead = 0x00000080,
        ColorAttachmentWrite = 0x00000100,
        DepthStencilAttachmentRead = 0x00000200,
        DepthStencilAttachmentWrite = 0x00000400,
        TransferRead = 0x00000800,
        TransferWrite = 0x00001000,
        HostRead = 0x00002000,
        HostWrite = 0x00004000,
        MemoryRead = 0x00008000,
        MemoryWrite = 0x00010000,
        ShaderSampledRead = 0x100000000,
        ShaderStorageRead = 0x200000000,
        ShaderStorageWrite = 0x400000000,
        VideoDecodeRead = 0x00000400,
        VideoDecodeWrite = 0x00000800,
        VideoEncodeRead = 0x00001000,
        VideoEncodeWrite = 0x00002000,
        TransformFeedbackWrite = 0x02000000,
        TransformFeedbackCounterRead = 0x04000000,
        TransformFeedbackCounterWrite = 0x08000000,
        ConditionalRenderingRead = 0x00100000,
        CommandPreprocessRead = 0x00020000,
        CommandPreprocessWrite = 0x00040000,
        FragmentShadingRateAttachmentRead = 0x00800000,
        AccelerationStructureRead = 0x00200000,
        AccelerationStructureWrite = 0x00400000,
        FragmentDensityMapRead = 0x01000000,
        ColorAttachmentReadNoncoherent = 0x00080000
    };

    // ==================== 查询类型枚举 ====================
    enum class QueryType {
        Occlusion,
        PipelineStatistics,
        Timestamp,
        Performance,
        AccelerationStructureCompactedSize,
        AccelerationStructureSerializationSize
    };

    enum class PipelineStatistic {
        InputAssemblyVertices,
        InputAssemblyPrimitives,
        VertexShaderInvocations,
        GeometryShaderInvocations,
        GeometryShaderPrimitives,
        ClippingInvocations,
        ClippingPrimitives,
        FragmentShaderInvocations,
        TessellationControlShaderPatches,
        TessellationEvaluationShaderInvocations,
        ComputeShaderInvocations,
        TaskShaderInvocations,
        MeshShaderInvocations
    };

    // ==================== 光线追踪枚举 ====================
    enum class RayTracingShaderGroupType {
        General,
        TrianglesHitGroup,
        ProceduralHitGroup,
        Callable
    };

    enum class BuildAccelerationStructureMode {
        Build,
        Update,
        Compact
    };

    enum class GeometryType {
        Triangles,
        AABBs,
        Instances
    };

    // ==================== 同步原语枚举 ====================
    enum class Filter {
        Nearest,
        Linear,
        Cubic
    };

    enum class ShadingRate {
        _1x1 = 0,
        _1x2 = 1,
        _2x1 = 2,
        _2x2 = 3,
        _2x4 = 4,
        _4x2 = 5,
        _4x4 = 6
    };

    enum class ConservativeRasterizationMode {
        Disabled,
        Overestimate,
        Underestimate
    };

    // ==================== 调试和验证枚举 ====================
    enum class MessageSeverity {
        Verbose,
        Info,
        Warning,
        Error,
        Critical
    };

    enum class MessageSource {
        General,
        Validation,
        Performance,
        Shader,
        API
    };

    // ==================== 特性支持枚举 ====================
    enum class Feature {
        MultiDrawIndirect,
        DrawIndirectFirstInstance,
        DepthClamp,
        DepthBiasClamp,
        FillModeNonSolid,
        DepthBounds,
        WideLines,
        LargePoints,
        AlphaToOne,
        MultiViewport,
        SamplerAnisotropy,
        TextureCompressionETC2,
        TextureCompressionASTC,
        TextureCompressionBC,
        OcclusionQueryPrecise,
        PipelineStatisticsQuery,
        VertexPipelineStoresAndAtomics,
        FragmentStoresAndAtomics,
        ShaderTessellationAndGeometryPointSize,
        ShaderImageGatherExtended,
        ShaderStorageImageExtendedFormats,
        ShaderStorageImageMultisample,
        ShaderStorageImageReadWithoutFormat,
        ShaderStorageImageWriteWithoutFormat,
        ShaderUniformBufferArrayDynamicIndexing,
        ShaderSampledImageArrayDynamicIndexing,
        ShaderStorageBufferArrayDynamicIndexing,
        ShaderStorageImageArrayDynamicIndexing,
        ShaderClipDistance,
        ShaderCullDistance,
        ShaderFloat64,
        ShaderInt64,
        ShaderInt16,
        ShaderResourceResidency,
        ShaderResourceMinLod,
        SparseBinding,
        SparseResidencyBuffer,
        SparseResidencyImage2D,
        SparseResidencyImage3D,
        SparseResidency2Samples,
        SparseResidency4Samples,
        SparseResidency8Samples,
        SparseResidency16Samples,
        SparseResidencyAliased,
        VariableMultisampleRate,
        InheritedQueries,
        GeometryShader,
        TessellationShader,
        SampleRateShading,
        DualSrcBlend,
        LogicOp,
        MultiDrawIndirectCount,
        DrawIndirectCount,
        DepthClipEnable,
        SamplerMirrorClampToEdge,
        ShaderDrawParameters,
        ShaderFloat16,
        ShaderInt8,
        ShaderClock,
        DepthStencilResolve,
        FragmentShaderRate,
        TimelineSemaphore,
        BufferDeviceAddress,
        RayTracingPipeline,
        RayQuery,
        AccelerationStructure,
        MeshShader,
        TaskShader,
        ShaderDemoteToHelperInvocation,
        ShaderTerminateInvocation,
        FragmentShadingRate,
        FragmentShadingRateAttachment,
        ShaderZeroInitializeWorkgroupMemory,
        ShaderIntegerDotProduct,
        PipelineLibrary,
        PresentWait,
        HDRMetadata,
        SwapchainMutableFormat,
        MemoryBudget,
        MemoryPriority,
        DedicatedAllocation,
        DeviceCoherentMemory,
        DeviceGroup,
        ProtectedMemory,
        PipelineRobustness,
        PipelineProtectedAccess,
        ConservativeRasterization,
        VRS,
        PrimitiveShadingRate,
        ShadingRateImage
    };

    //// ==================== 混合状态枚举 ====================
    //enum class ColorBlend {
    //    Opaque,
    //    AlphaBlend,
    //    Additive,
    //    Multiply,
    //    PremultipliedAlpha
    //};

    //// ==================== 深度模板状态枚举 ====================
    //enum class DepthStencil {
    //    None,
    //    DepthRead,
    //    DepthWrite,
    //    DepthReadWrite,
    //    DepthReadStencilRead,
    //    DepthWriteStencilWrite
    //};

    //// ==================== 光栅化状态枚举 ====================
    //enum class Rasterizer {
    //    CullNone,
    //    CullFront,
    //    CullBack,
    //    CullFrontAndBack,
    //    Wireframe,
    //    NoDepthClip
    //};

    /**
     * @brief 管线类型枚举
     */
    enum class PipelineType {
        Graphics,
        Compute,
        RayTracing,
        Mesh,
        Task
    };

    enum class ImageCreateFlags : uint32_t {
        None = 0,
        CubeCompatible = 0x00000001,           // 允许将2D图像作为立方体贴图处理
        MutableFormat = 0x00000002,            // 允许格式转换
        Protected = 0x00000004,                // 受保护内存
        SparseBinding = 0x00000008,            // 稀疏绑定
        SparseResidency = 0x00000010,          // 稀疏驻留
        SparseAliased = 0x00000020,            // 稀疏别名
        Disjoint = 0x00000040,                 // 分离平面
        SplitInstanceBindRegions = 0x00000080, // 分割实例绑定区域
        BlockTexelViewCompatible = 0x00000100, // 块纹素视图兼容
        ExtendedUsage = 0x00000200,            // 扩展使用
        NoFlags = 0
    };

    inline ImageCreateFlags operator|(ImageCreateFlags a, ImageCreateFlags b) {
        return static_cast<ImageCreateFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    inline ImageCreateFlags operator&(ImageCreateFlags a, ImageCreateFlags b) {
        return static_cast<ImageCreateFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }
    inline ImageCreateFlags& operator|=(ImageCreateFlags& a, ImageCreateFlags b) {
        a = a | b;
        return a;
    }
} // namespace StarryEngine::RHI