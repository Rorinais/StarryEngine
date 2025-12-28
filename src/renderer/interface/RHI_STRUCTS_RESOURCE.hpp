#pragma once
#include "RHI_ENUMS.hpp"
#include "RHI_STRUCTS_BASE.hpp"
#include <string>

namespace StarryEngine::RHI {

    // ==================== 资源描述结构体 ====================

    /**
     * @brief 缓冲区描述结构体
     * @details 描述缓冲区的属性、用途和内存分配方式
     */
    struct BufferDesc {
        uint64_t size = 0;                     ///< 缓冲区大小（字节）
        BufferType type = BufferType::Vertex;  ///< 缓冲区类型
        Format format = Format::Undefined;     ///< 数据格式（如为结构体则可能为Undefined）
        MemoryType memoryType = MemoryType::GPU_Only; ///< 内存类型
        uint32_t stride = 0;                   ///< 结构化缓冲区步长（字节）
        bool allowUpdate = false;              ///< 允许更新
        bool allowReadback = false;            ///< 允许读回
        bool allowRawViews = false;            ///< 允许字节地址视图
        bool allowCounter = false;             ///< 允许原子计数器
        bool allowIndirectArgs = false;        ///< 允许间接参数
        bool allowShaderAtomics = false;       ///< 允许着色器原子操作
        bool persistentMapped = false;         ///< 持久映射内存
        std::string debugName;                 ///< 调试名称
        const void* initialData = nullptr;     ///< 初始数据
        size_t initialDataSize = 0;            ///< 初始数据大小

        bool operator==(const BufferDesc& other) const {
            return size == other.size && type == other.type &&
                format == other.format && memoryType == other.memoryType &&
                stride == other.stride && allowUpdate == other.allowUpdate &&
                allowReadback == other.allowReadback && allowRawViews == other.allowRawViews &&
                allowCounter == other.allowCounter && allowIndirectArgs == other.allowIndirectArgs &&
                allowShaderAtomics == other.allowShaderAtomics &&
                persistentMapped == other.persistentMapped;
        }

        bool operator!=(const BufferDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 纹理描述结构体
     * @details 描述纹理的尺寸、格式、类型和用途
     */
    struct TextureDesc {
        Extent3D extent = { 1, 1, 1 };         ///< 纹理尺寸
        Format format = Format::RGBA8_UNorm;   ///< 纹理格式
        TextureType type = TextureType::Texture2D; ///< 纹理类型
        uint32_t mipLevels = 1;                ///< MIP层级数
        uint32_t arrayLayers = 1;              ///< 数组层数
        uint32_t sampleCount = 1;              ///< 采样数（MSAA）
        bool generateMips = false;             ///< 自动生成MIP
        bool allowRenderTarget = false;        ///< 允许作为渲染目标
        bool allowDepthStencil = false;        ///< 允许作为深度模板缓冲区
        bool allowUnorderedAccess = false;     ///< 允许无序访问视图
        bool allowSimultaneousAccess = false;  ///< 允许并发访问
        bool allowCrossQueueSharing = false;   ///< 允许队列间共享
        bool memoryless = false;               ///< 仅临时内存（移动平台）
        bool protectedMemory = false;          ///< 受保护内存
        bool sparseBinding = false;            ///< 稀疏绑定
        std::string debugName;                 ///< 调试名称

        bool operator==(const TextureDesc& other) const {
            return extent == other.extent && format == other.format &&
                type == other.type && mipLevels == other.mipLevels &&
                arrayLayers == other.arrayLayers && sampleCount == other.sampleCount &&
                generateMips == other.generateMips && allowRenderTarget == other.allowRenderTarget &&
                allowDepthStencil == other.allowDepthStencil &&
                allowUnorderedAccess == other.allowUnorderedAccess &&
                allowSimultaneousAccess == other.allowSimultaneousAccess &&
                allowCrossQueueSharing == other.allowCrossQueueSharing &&
                memoryless == other.memoryless && protectedMemory == other.protectedMemory &&
                sparseBinding == other.sparseBinding;
        }

        bool operator!=(const TextureDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 采样器描述结构体
     * @details 描述纹理采样器的过滤、寻址方式和比较操作
     */
    struct SamplerDesc {
        SamplerFilter minFilter = SamplerFilter::Linear;      ///< 缩小过滤
        SamplerFilter magFilter = SamplerFilter::Linear;      ///< 放大过滤
        SamplerFilter mipFilter = SamplerFilter::Linear;      ///< MIP过滤
        SamplerAddressMode addressU = SamplerAddressMode::Repeat; ///< U轴寻址模式
        SamplerAddressMode addressV = SamplerAddressMode::Repeat; ///< V轴寻址模式
        SamplerAddressMode addressW = SamplerAddressMode::Repeat; ///< W轴寻址模式
        float mipLodBias = 0.0f;              ///< MIP LOD偏移
        float maxAnisotropy = 1.0f;           ///< 最大各向异性
        float minLod = 0.0f;                  ///< 最小LOD级别
        float maxLod = 1000.0f;               ///< 最大LOD级别
        bool compareEnable = false;           ///< 是否启用比较
        CompareOp compareOp = CompareOp::Always; ///< 比较操作
        SamplerBorderColor borderColor = SamplerBorderColor::OpaqueBlack; ///< 边框颜色
        bool unnormalizedCoordinates = false; ///< 是否使用非归一化坐标
        std::string debugName;                ///< 调试名称

        bool operator==(const SamplerDesc& other) const {
            return minFilter == other.minFilter && magFilter == other.magFilter &&
                mipFilter == other.mipFilter && addressU == other.addressU &&
                addressV == other.addressV && addressW == other.addressW &&
                mipLodBias == other.mipLodBias && maxAnisotropy == other.maxAnisotropy &&
                minLod == other.minLod && maxLod == other.maxLod &&
                compareEnable == other.compareEnable && compareOp == other.compareOp &&
                borderColor == other.borderColor &&
                unnormalizedCoordinates == other.unnormalizedCoordinates;
        }

        bool operator!=(const SamplerDesc& other) const {
            return !(*this == other);
        }
    };

} // namespace StarryEngine::RHI