#include <renderer/interface/RHI_TYPES.hpp>
#include <cmath>
#include <algorithm>
#include <random>
#include <cstring>
#include <array>
#include <fstream>
#include <sstream>

namespace StarryEngine::RHI {

    // 格式支持查询
    bool RHIUtils::isDepthFormat(Format format) {
        switch (format) {
        case Format::D16_UNorm:
        case Format::D32_Float:
        case Format::D24_UNorm_S8_UInt:
        case Format::D32_Float_S8_UInt:
            return true;
        default:
            return false;
        }
    }

    bool RHIUtils::isStencilFormat(Format format) {
        switch (format) {
        case Format::D24_UNorm_S8_UInt:
        case Format::D32_Float_S8_UInt:
            return true;
        default:
            return false;
        }
    }

    bool RHIUtils::isDepthStencilFormat(Format format) {
        return isDepthFormat(format) && isStencilFormat(format);
    }

    bool RHIUtils::isCompressedFormat(Format format) {
        switch (format) {
        case Format::BC1_RGB_UNorm:
        case Format::BC1_RGBA_UNorm:
        case Format::BC1_RGB_sRGB:
        case Format::BC1_RGBA_sRGB:
        case Format::BC2_UNorm:
        case Format::BC2_sRGB:
        case Format::BC3_UNorm:
        case Format::BC3_sRGB:
        case Format::BC4_UNorm:
        case Format::BC4_SNorm:
        case Format::BC5_UNorm:
        case Format::BC5_SNorm:
        case Format::BC6H_UF16:
        case Format::BC6H_SF16:
        case Format::BC7_UNorm:
        case Format::BC7_sRGB:
        case Format::ASTC_4x4_UNorm:
        case Format::ASTC_4x4_sRGB:
        case Format::ASTC_8x8_UNorm:
        case Format::ASTC_8x8_sRGB:
        case Format::ETC2_RGB8_UNorm:
        case Format::ETC2_RGB8_sRGB:
        case Format::ETC2_RGBA8_UNorm:
        case Format::ETC2_RGBA8_sRGB:
        case Format::EAC_R11_UNorm:
        case Format::EAC_R11_SNorm:
        case Format::EAC_RG11_UNorm:
        case Format::EAC_RG11_SNorm:
            return true;
        default:
            return false;
        }
    }

    bool RHIUtils::isSRGBFormat(Format format) {
        switch (format) {
        case Format::R8_sRGB:
        case Format::RGBA8_sRGB:
        case Format::BGRA8_sRGB:
        case Format::BC1_RGB_sRGB:
        case Format::BC1_RGBA_sRGB:
        case Format::BC2_sRGB:
        case Format::BC3_sRGB:
        case Format::BC7_sRGB:
        case Format::ASTC_4x4_sRGB:
        case Format::ASTC_8x8_sRGB:
        case Format::ETC2_RGB8_sRGB:
        case Format::ETC2_RGBA8_sRGB:
            return true;
        default:
            return false;
        }
    }

    bool RHIUtils::isIntegerFormat(Format format) {
        switch (format) {
        case Format::R8_UInt:
        case Format::R8_SInt:
        case Format::R16_UInt:
        case Format::R16_SInt:
        case Format::R32_UInt:
        case Format::R32_SInt:
        case Format::RG8_UInt:
        case Format::RG8_SInt:
        case Format::RG16_UInt:
        case Format::RG16_SInt:
        case Format::RG32_UInt:
        case Format::RG32_SInt:
        case Format::RGBA8_UInt:
        case Format::RGBA8_SInt:
        case Format::RGBA16_UInt:
        case Format::RGBA16_SInt:
        case Format::RGBA32_UInt:
        case Format::RGBA32_SInt:
        case Format::BGRA8_UInt:
        case Format::BGRA8_SInt:
            return true;
        default:
            return false;
        }
    }

    bool RHIUtils::isFloatFormat(Format format) {
        switch (format) {
        case Format::R16_Float:
        case Format::R32_Float:
        case Format::RG16_Float:
        case Format::RG32_Float:
        case Format::RGB32_Float:
        case Format::RGBA16_Float:
        case Format::RGBA32_Float:
        case Format::BC6H_UF16:
        case Format::BC6H_SF16:
            return true;
        default:
            return false;
        }
    }

    bool RHIUtils::isNormalizedFormat(Format format) {
        switch (format) {
        case Format::R8_UNorm:
        case Format::R8_SNorm:
        case Format::R16_UNorm:
        case Format::R16_SNorm:
        case Format::RG8_UNorm:
        case Format::RG8_SNorm:
        case Format::RG16_UNorm:
        case Format::RG16_SNorm:
        case Format::RGBA8_UNorm:
        case Format::RGBA8_SNorm:
        case Format::RGBA16_UNorm:
        case Format::RGBA16_SNorm:
        case Format::BGRA8_UNorm:
        case Format::BGRA8_SNorm:
        case Format::D16_UNorm:
        case Format::D24_UNorm_S8_UInt:
            return true;
        default:
            return false;
        }
    }

    std::string RHIUtils::formatToString(Format format) {
        static const std::array<const char*, 79> formatNames = {
            "Undefined",          // [0]
            "R8_UNorm",           // [1]
            "R8_SNorm",           // [2]
            "R8_UInt",            // [3]
            "R8_SInt",            // [4]
            "R8_sRGB",            // [5]
            "R16_UNorm",          // [6]
            "R16_SNorm",          // [7]
            "R16_UInt",           // [8]
            "R16_SInt",           // [9]
            "R16_Float",          // [10]
            "RG8_UNorm",          // [11]
            "RG8_SNorm",          // [12]
            "RG8_UInt",           // [13]
            "RG8_SInt",           // [14]
            "R32_UInt",           // [15]
            "R32_SInt",           // [16]
            "R32_Float",          // [17]
            "RG16_UNorm",         // [18]
            "RG16_SNorm",         // [19]
            "RG16_UInt",          // [20]
            "RG16_SInt",          // [21]
            "RG16_Float",         // [22]
            "RGBA8_UNorm",        // [23]
            "RGBA8_SNorm",        // [24]
            "RGBA8_UInt",         // [25]
            "RGBA8_SInt",         // [26]
            "BGRA8_UNorm",        // [27]
            "BGRA8_SNorm",        // [28]
            "BGRA8_UInt",         // [29]
            "BGRA8_SInt",         // [30]
            "RGBA8_sRGB",         // [31]
            "BGRA8_sRGB",         // [32]
            "RG32_UInt",          // [33]
            "RG32_SInt",          // [34]
            "RG32_Float",         // [35]
            "RGBA16_UNorm",       // [36]
            "RGBA16_SNorm",       // [37]
            "RGBA16_UInt",        // [38]
            "RGBA16_SInt",        // [39]
            "RGBA16_Float",       // [40]
            "RGB32_UInt",         // [41]
            "RGB32_SInt",         // [42]
            "RGB32_Float",        // [43]
            "RGBA32_UInt",        // [44]
            "RGBA32_SInt",        // [45]
            "RGBA32_Float",       // [46]
            "D16_UNorm",          // [47]
            "D32_Float",          // [48]
            "D24_UNorm_S8_UInt",  // [49]
            "D32_Float_S8_UInt",  // [50]
            // BC压缩格式
            "BC1_RGB_UNorm",      // [51]
            "BC1_RGBA_UNorm",     // [52]
            "BC1_RGB_sRGB",       // [53]
            "BC1_RGBA_sRGB",      // [54]
            "BC2_UNorm",          // [55]
            "BC2_sRGB",           // [56]
            "BC3_UNorm",          // [57]
            "BC3_sRGB",           // [58]
            "BC4_UNorm",          // [59]
            "BC4_SNorm",          // [60]
            "BC5_UNorm",          // [61]
            "BC5_SNorm",          // [62]
            "BC6H_UF16",          // [63]
            "BC6H_SF16",          // [64]
            "BC7_UNorm",          // [65]
            "BC7_sRGB",           // [66]
            // ASTC压缩格式
            "ASTC_4x4_UNorm",     // [67]
            "ASTC_4x4_sRGB",      // [68]
            "ASTC_8x8_UNorm",     // [69]
            "ASTC_8x8_sRGB",      // [70]
            // ETC2/EAC压缩格式
            "ETC2_RGB8_UNorm",    // [71]
            "ETC2_RGB8_sRGB",     // [72]
            "ETC2_RGBA8_UNorm",   // [73]
            "ETC2_RGBA8_sRGB",    // [74]
            "EAC_R11_UNorm",      // [75]
            "EAC_R11_SNorm",      // [76]
            "EAC_RG11_UNorm",     // [77]
            "EAC_RG11_SNorm"      // [78]
        };

        size_t index = static_cast<size_t>(format);
        if (index < formatNames.size()) {
            return formatNames[index];
        }
        return "Unknown";
    }

    uint32_t RHIUtils::getFormatSize(Format format) {
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
            // BC压缩格式 [51-66]
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
            // ASTC压缩格式 [67-70]
            16,   // ASTC_4x4_UNorm [67]
            16,   // ASTC_4x4_sRGB [68]
            16,   // ASTC_8x8_UNorm [69]
            16,   // ASTC_8x8_sRGB [70]
            // ETC2/EAC压缩格式 [71-78]
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
        if (index < formatSizes.size()) {
            return formatSizes[index];
        }
        return 0;
    }
    uint32_t RHIUtils::getFormatComponentCount(Format format) {
        switch (format) {
        case Format::R8_UNorm:
        case Format::R8_SNorm:
        case Format::R8_UInt:
        case Format::R8_SInt:
        case Format::R8_sRGB:
        case Format::R16_UNorm:
        case Format::R16_SNorm:
        case Format::R16_UInt:
        case Format::R16_SInt:
        case Format::R16_Float:
        case Format::R32_UInt:
        case Format::R32_SInt:
        case Format::R32_Float:
            return 1;
        case Format::RG8_UNorm:
        case Format::RG8_SNorm:
        case Format::RG8_UInt:
        case Format::RG8_SInt:
        case Format::RG16_UNorm:
        case Format::RG16_SNorm:
        case Format::RG16_UInt:
        case Format::RG16_SInt:
        case Format::RG16_Float:
        case Format::RG32_UInt:
        case Format::RG32_SInt:
        case Format::RG32_Float:
            return 2;
        case Format::RGB32_UInt:
        case Format::RGB32_SInt:
        case Format::RGB32_Float:
            return 3;
        case Format::RGBA8_UNorm:
        case Format::RGBA8_SNorm:
        case Format::RGBA8_UInt:
        case Format::RGBA8_SInt:
        case Format::RGBA8_sRGB:
        case Format::BGRA8_UNorm:
        case Format::BGRA8_SNorm:
        case Format::BGRA8_UInt:
        case Format::BGRA8_SInt:
        case Format::BGRA8_sRGB:
        case Format::RGBA16_UNorm:
        case Format::RGBA16_SNorm:
        case Format::RGBA16_UInt:
        case Format::RGBA16_SInt:
        case Format::RGBA16_Float:
        case Format::RGBA32_UInt:
        case Format::RGBA32_SInt:
        case Format::RGBA32_Float:
            return 4;
        default:
            return 0; // 压缩格式或深度模板格式返回0
        }
    }

    Format RHIUtils::getSRGBFormat(Format format) {
        switch (format) {
        case Format::RGBA8_UNorm:
            return Format::RGBA8_sRGB;
        case Format::BGRA8_UNorm:
            return Format::BGRA8_sRGB;
        case Format::BC1_RGB_UNorm:
            return Format::BC1_RGB_sRGB;
        case Format::BC1_RGBA_UNorm:
            return Format::BC1_RGBA_sRGB;
        case Format::BC2_UNorm:
            return Format::BC2_sRGB;
        case Format::BC3_UNorm:
            return Format::BC3_sRGB;
        case Format::BC7_UNorm:
            return Format::BC7_sRGB;
        case Format::ASTC_4x4_UNorm:
            return Format::ASTC_4x4_sRGB;
        case Format::ASTC_8x8_UNorm:
            return Format::ASTC_8x8_sRGB;
        case Format::ETC2_RGB8_UNorm:
            return Format::ETC2_RGB8_sRGB;
        case Format::ETC2_RGBA8_UNorm:
            return Format::ETC2_RGBA8_sRGB;
        default:
            return format;
        }
    }

    Format RHIUtils::getLinearFormat(Format format) {
        switch (format) {
        case Format::RGBA8_sRGB:
            return Format::RGBA8_UNorm;
        case Format::BGRA8_sRGB:
            return Format::BGRA8_UNorm;
        case Format::BC1_RGB_sRGB:
            return Format::BC1_RGB_UNorm;
        case Format::BC1_RGBA_sRGB:
            return Format::BC1_RGBA_UNorm;
        case Format::BC2_sRGB:
            return Format::BC2_UNorm;
        case Format::BC3_sRGB:
            return Format::BC3_UNorm;
        case Format::BC7_sRGB:
            return Format::BC7_UNorm;
        case Format::ASTC_4x4_sRGB:
            return Format::ASTC_4x4_UNorm;
        case Format::ASTC_8x8_sRGB:
            return Format::ASTC_8x8_UNorm;
        case Format::ETC2_RGB8_sRGB:
            return Format::ETC2_RGB8_UNorm;
        case Format::ETC2_RGBA8_sRGB:
            return Format::ETC2_RGBA8_UNorm;
        default:
            return format;
        }
    }

    Format RHIUtils::getDepthFormat(uint32_t depthBits, bool stencil) {
        if (stencil) {
            if (depthBits == 32) {
                return Format::D32_Float_S8_UInt;
            }
            else if (depthBits == 24) {
                return Format::D24_UNorm_S8_UInt;
            }
        }
        else {
            if (depthBits == 32) {
                return Format::D32_Float;
            }
            else if (depthBits == 16) {
                return Format::D16_UNorm;
            }
        }
        return Format::Undefined;
    }

    // 内存对齐
    uint64_t RHIUtils::alignUp(uint64_t value, uint64_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    uint64_t RHIUtils::alignDown(uint64_t value, uint64_t alignment) {
        return value & ~(alignment - 1);
    }

    bool RHIUtils::isAligned(uint64_t value, uint64_t alignment) {
        return (value & (alignment - 1)) == 0;
    }

    // 资源大小计算
    uint64_t RHIUtils::calculateTextureSize(const TextureDesc& desc) {
        if (desc.extent.width == 0 || desc.extent.height == 0 || desc.extent.depth == 0) {
            return 0;
        }

        uint64_t totalSize = 0;
        uint32_t mipWidth = desc.extent.width;
        uint32_t mipHeight = desc.extent.height;
        uint32_t mipDepth = desc.extent.depth;

        for (uint32_t mipLevel = 0; mipLevel < desc.mipLevels; ++mipLevel) {
            uint64_t mipSize = 0;

            if (RHIUtils::isCompressedFormat(desc.format)) {
                // 压缩纹理大小计算
                uint32_t blockWidth = 4; // 大部分压缩格式使用4x4块
                uint32_t blockHeight = 4;
                uint32_t blockSize = getFormatSize(desc.format); // 返回的是块大小

                uint32_t widthInBlocks = (mipWidth + blockWidth - 1) / blockWidth;
                uint32_t heightInBlocks = (mipHeight + blockHeight - 1) / blockHeight;

                mipSize = static_cast<uint64_t>(widthInBlocks) * heightInBlocks * blockSize;
            }
            else {
                // 非压缩纹理大小计算
                uint32_t pixelSize = getFormatSize(desc.format);
                mipSize = static_cast<uint64_t>(mipWidth) * mipHeight * pixelSize;
            }

            totalSize += mipSize * mipDepth * desc.arrayLayers;

            // 下一级MIP
            mipWidth = std::max(mipWidth >> 1, 1u);
            mipHeight = std::max(mipHeight >> 1, 1u);
            mipDepth = std::max(mipDepth >> 1, 1u);
        }

        return totalSize;
    }

    uint64_t RHIUtils::calculateBufferSize(const BufferDesc& desc) {
        return desc.size;
    }

    uint32_t RHIUtils::calculateMipLevels(uint32_t width, uint32_t height, uint32_t depth) {
        uint32_t maxDimension = std::max({ width, height, depth });
        return static_cast<uint32_t>(std::floor(std::log2(maxDimension))) + 1;
    }

    Extent3D RHIUtils::calculateMipExtent(const Extent3D& baseExtent, uint32_t mipLevel) {
        Extent3D extent;
        extent.width = std::max(baseExtent.width >> mipLevel, 1u);
        extent.height = std::max(baseExtent.height >> mipLevel, 1u);
        extent.depth = std::max(baseExtent.depth >> mipLevel, 1u);
        return extent;
    }

    // 着色器工具
    std::vector<uint8_t> RHIUtils::compileShader(
        const std::string& source,
        ShaderStage stage,
        API targetAPI,
        const std::string& entryPoint,
        const std::vector<std::string>& defines,
        const std::vector<std::string>& includePaths) {

        // 这里需要实际的着色器编译器
        // 例如：使用glslangValidator、dxc、或第三方库
        // 简化实现，返回空数据
        return {};
    }

    bool RHIUtils::decompileShader(
        const std::vector<uint8_t>& bytecode,
        std::string& source,
        API sourceAPI) {

        // 这里需要实际的着色器反编译器
        return false;
    }

    bool RHIUtils::reflectShader(
        const std::vector<uint8_t>& bytecode,
        ShaderReflectionInfo& reflection,
        API shaderAPI) {

        // 这里需要实际的着色器反射器
        // 例如：使用SPIRV-Reflect或DXIL反射
        return false;
    }

    // 纹理加载
    std::unique_ptr<RHITexture> RHIUtils::loadTexture(
        IRHIContext* context,
        const std::string& filepath,
        bool generateMips,
        bool srgb) {

        // 这里需要实际的纹理加载库
        // 例如：使用stb_image、FreeImage等
        return nullptr;
    }

    std::unique_ptr<RHITexture> RHIUtils::createTextureFromData(
        IRHIContext* context,
        const void* data,
        uint32_t width,
        uint32_t height,
        Format format,
        bool generateMips) {

        TextureDesc desc;
        desc.extent = { width, height, 1 };
        desc.format = format;
        desc.type = TextureType::Texture2D;
        desc.generateMips = generateMips;
        desc.mipLevels = generateMips ? calculateMipLevels(width, height) : 1;

        // 直接返回 context->createTexture 的结果，它应该已经是一个 unique_ptr
        // 或者返回包装在 unique_ptr 中的指针
        RHITexture* texture = context->createTexture(desc);
        if (!texture) {
            return nullptr;
        }

        // 如果有上传数据的逻辑，这里添加

        // 返回 unique_ptr，使用自定义删除器
        return std::unique_ptr<RHITexture>(texture);
    }

    // 管线状态预设
    GraphicsPipelineDesc RHIUtils::createDefaultOpaquePipeline() {
        GraphicsPipelineDesc desc;

        desc.rasterizer = createDefaultRasterizerState();
        desc.depthStencil = createDefaultDepthStencilState();
        desc.colorBlend = createDefaultBlendState();

        desc.topology = PrimitiveTopology::TriangleList;
        desc.primitiveRestartEnable = false;

        desc.depthStencil.depthTestEnable = true;
        desc.depthStencil.depthWriteEnable = true;
        desc.depthStencil.depthCompareOp = CompareOp::Less;

        desc.colorBlend.attachments.resize(1);
        desc.colorBlend.attachments[0].blendEnable = false;
        desc.colorBlend.attachments[0].colorWriteMask = ColorComponent::All;

        return desc;
    }

    GraphicsPipelineDesc RHIUtils::createDefaultAlphaBlendPipeline() {
        GraphicsPipelineDesc desc = createDefaultOpaquePipeline();

        BlendAttachmentState& blend = desc.colorBlend.attachments[0];
        blend.blendEnable = true;
        blend.srcColorBlendFactor = BlendFactor::SrcAlpha;
        blend.dstColorBlendFactor = BlendFactor::OneMinusSrcAlpha;
        blend.colorBlendOp = BlendOp::Add;
        blend.srcAlphaBlendFactor = BlendFactor::One;
        blend.dstAlphaBlendFactor = BlendFactor::Zero;
        blend.alphaBlendOp = BlendOp::Add;

        return desc;
    }

    GraphicsPipelineDesc RHIUtils::createDefaultWireframePipeline() {
        GraphicsPipelineDesc desc = createDefaultOpaquePipeline();

        desc.rasterizer.polygonMode = PolygonMode::Line;
        desc.rasterizer.lineWidth = 1.5f;

        return desc;
    }

    GraphicsPipelineDesc RHIUtils::createDefaultSkyboxPipeline() {
        GraphicsPipelineDesc desc = createDefaultOpaquePipeline();

        desc.rasterizer.cullMode = CullMode::Front; // 天空盒通常使用正面剔除
        desc.depthStencil.depthCompareOp = CompareOp::LessOrEqual;
        desc.depthStencil.depthWriteEnable = false;

        return desc;
    }

    GraphicsPipelineDesc RHIUtils::createDefaultPostProcessPipeline() {
        GraphicsPipelineDesc desc = createDefaultOpaquePipeline();

        desc.rasterizer.cullMode = CullMode::None; // 全屏四边形，不需要剔除
        desc.depthStencil.depthTestEnable = false;
        desc.depthStencil.depthWriteEnable = false;

        return desc;
    }

    RasterizerState RHIUtils::createDefaultRasterizerState() {
        RasterizerState state;
        state.polygonMode = PolygonMode::Fill;
        state.cullMode = CullMode::Back;
        state.frontFace = FrontFace::CounterClockwise;
        state.lineWidth = 1.0f;
        state.depthBiasEnable = false;
        return state;
    }

    DepthStencilState RHIUtils::createDefaultDepthStencilState() {
        DepthStencilState state;
        state.depthTestEnable = true;
        state.depthWriteEnable = true;
        state.depthCompareOp = CompareOp::Less;
        state.depthBoundsTestEnable = false;
        state.stencilTestEnable = false;
        return state;
    }

    ColorBlendState RHIUtils::createDefaultBlendState() {
        ColorBlendState state;
        state.attachments.resize(1);
        state.attachments[0] = BlendAttachmentState();
        return state;
    }

    // 调试工具
    void RHIUtils::setDebugColor(float* color, uint32_t resourceId) {
        // 使用简单的哈希函数生成颜色
        uint32_t hash = resourceId;
        hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
        hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
        hash = (hash >> 16) ^ hash;

        color[0] = ((hash >> 16) & 0xFF) / 255.0f;
        color[1] = ((hash >> 8) & 0xFF) / 255.0f;
        color[2] = (hash & 0xFF) / 255.0f;
        color[3] = 1.0f;
    }

    std::string RHIUtils::resourceTypeToString(IResource* resource) {
        // 这里需要RTTI或自定义类型标识
        return "Unknown";
    }
    std::string RHIUtils::shaderStageToString(ShaderStage stage) {
        std::string result;

        if (SHADER_STAGE_FLAG(Vertex)) result += "Vertex|";
        if (SHADER_STAGE_FLAG(TessellationControl)) result += "TessellationControl|";
        if (SHADER_STAGE_FLAG(TessellationEvaluation)) result += "TessellationEvaluation|";
        if (SHADER_STAGE_FLAG(Geometry)) result += "Geometry|";
        if (SHADER_STAGE_FLAG(Fragment)) result += "Fragment|";
        if (SHADER_STAGE_FLAG(Compute)) result += "Compute|";
        if (SHADER_STAGE_FLAG(Amplification)) result += "Amplification|";
        if (SHADER_STAGE_FLAG(Mesh)) result += "Mesh|";
        if (SHADER_STAGE_FLAG(RayGen)) result += "RayGen|";
        if (SHADER_STAGE_FLAG(AnyHit)) result += "AnyHit|";
        if (SHADER_STAGE_FLAG(ClosestHit)) result += "ClosestHit|";
        if (SHADER_STAGE_FLAG(Miss)) result += "Miss|";
        if (SHADER_STAGE_FLAG(Intersection)) result += "Intersection|";
        if (SHADER_STAGE_FLAG(Callable)) result += "Callable|";

        if (!result.empty() && result.back() == '|') {
            result.pop_back();
        }

        return result.empty() ? "None" : result;
    }

    // 性能分析
    void RHIUtils::beginGPUTimestamp(IRHIContext* context, const std::string& name) {
        // 这里需要查询池和时间戳支持
    }

    void RHIUtils::endGPUTimestamp(IRHIContext* context, const std::string& name) {
        // 这里需要查询池和时间戳支持
    }

    double RHIUtils::getGPUTimestampDuration(IRHIContext* context, const std::string& name) {
        return 0.0;
    }

    // 验证和检查
    bool RHIUtils::validatePipelineState(const GraphicsPipelineDesc& desc) {

        return true;
    }

    bool RHIUtils::validateResourceState(IResource* resource, const std::string& operation) {
        if (!resource || !resource->isValid()) {
            return false;
        }

        // 这里可以进行更复杂的验证
        return true;
    }

    bool RHIUtils::checkMemoryLeaks(IRHIContext* context) {
        // 这里需要跟踪资源创建和销毁
        return false; // 简化实现
    }

    // 转换函数
    Format RHIUtils::fromVulkanFormat(void* vkFormat) {
        // 实际需要根据Vulkan格式枚举转换
        return Format::Undefined;
    }

    void* RHIUtils::toVulkanFormat(Format format) {
        // 实际需要根据Format转换为Vulkan格式
        return nullptr;
    }

    Format RHIUtils::fromDXGIFormat(uint32_t dxgiFormat) {
        // 实际需要根据DXGI格式枚举转换
        return Format::Undefined;
    }

    uint32_t RHIUtils::toDXGIFormat(Format format) {
        // 实际需要根据Format转换为DXGI格式
        return 0;
    }

    Format RHIUtils::fromMetalFormat(void* mtlFormat) {
        // 实际需要根据Metal格式枚举转换
        return Format::Undefined;
    }

    void* RHIUtils::toMetalFormat(Format format) {
        // 实际需要根据Format转换为Metal格式
        return nullptr;
    }

    // 数学工具
    glm::mat4 RHIUtils::createPerspectiveMatrix(float fov, float aspect, float near, float far) {
        float tanHalfFov = std::tan(fov * 0.5f);

        glm::mat4 result(0.0f);
        result[0][0] = 1.0f / (aspect * tanHalfFov);
        result[1][1] = 1.0f / tanHalfFov;
        result[2][2] = far / (far - near);
        result[2][3] = 1.0f;
        result[3][2] = -(far * near) / (far - near);

        return result;
    }

    glm::mat4 RHIUtils::createOrthographicMatrix(float left, float right, float bottom, float top, float near, float far) {
        glm::mat4 result(1.0f);
        result[0][0] = 2.0f / (right - left);
        result[1][1] = 2.0f / (top - bottom);
        result[2][2] = 1.0f / (far - near);
        result[3][0] = -(right + left) / (right - left);
        result[3][1] = -(top + bottom) / (top - bottom);
        result[3][2] = -near / (far - near);

        return result;
    }

    glm::mat4 RHIUtils::createViewMatrix(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up) {
        glm::vec3 z = glm::normalize(eye - target);
        glm::vec3 x = glm::normalize(glm::cross(up, z));
        glm::vec3 y = glm::cross(z, x);

        glm::mat4 result(1.0f);
        result[0][0] = x.x;
        result[1][0] = x.y;
        result[2][0] = x.z;
        result[0][1] = y.x;
        result[1][1] = y.y;
        result[2][1] = y.z;
        result[0][2] = z.x;
        result[1][2] = z.y;
        result[2][2] = z.z;
        result[3][0] = -glm::dot(x, eye);
        result[3][1] = -glm::dot(y, eye);
        result[3][2] = -glm::dot(z, eye);

        return result;
    }

    // 随机工具
    Color RHIUtils::randomColor() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<> dis(0.0, 1.0);

        return Color(
            static_cast<float>(dis(gen)),
            static_cast<float>(dis(gen)),
            static_cast<float>(dis(gen)),
            1.0f
        );
    }

    float RHIUtils::randomFloat(float min, float max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(min, max);

        return static_cast<float>(dis(gen));
    }

    uint32_t RHIUtils::randomUint(uint32_t min, uint32_t max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(min, max);

        return static_cast<uint32_t>(dis(gen));
    }

} // namespace StarryEngine::RHI