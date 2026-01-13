#pragma once
#include "RHI_ENUMS.hpp"
#include <cstdint>
#include <array>
#include <glm/glm.hpp>

namespace StarryEngine::RHI {
    // ==================== 命令缓冲区结构体 ====================

    /**
     * @brief 命令缓冲区描述结构体
     * @details 描述命令缓冲区的属性和行为
     */
    struct CommandBufferDesc {
        CommandBufferLevel level = CommandBufferLevel::Primary; ///< 命令缓冲区级别
        CommandBufferType type = CommandBufferType::Graphics;   ///< 命令缓冲区类型
        bool oneTimeSubmit = true;               ///< 是否为一次性提交
        bool simultaneousUse = false;            ///< 是否支持同时使用
        std::string debugName;                   ///< 调试名称

        bool operator==(const CommandBufferDesc& other) const {
            return level == other.level && type == other.type &&
                oneTimeSubmit == other.oneTimeSubmit &&
                simultaneousUse == other.simultaneousUse;
        }

        bool operator!=(const CommandBufferDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 命令池描述结构体
     * @details 描述命令池的配置
     */
    struct CommandPoolDesc {
        QueueType queueType = QueueType::Graphics; ///< 队列类型
        bool transient = false;                   ///< 短生命周期命令缓冲区
        bool resetCommandBuffer = true;           ///< 允许重置命令缓冲区
        bool protectedMemory = false;             ///< 使用受保护内存
        std::string debugName;                    ///< 调试名称

        bool operator==(const CommandPoolDesc& other) const {
            return queueType == other.queueType && transient == other.transient &&
                resetCommandBuffer == other.resetCommandBuffer &&
                protectedMemory == other.protectedMemory;
        }

        bool operator!=(const CommandPoolDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 描述符池描述结构体
     * @details 描述描述符池的配置参数
     */
    struct DescriptorPoolDesc {
        std::vector<std::pair<DescriptorType, uint32_t>> poolSizes;  ///< 池大小配置
        uint32_t maxSets = 1000;                                     ///< 最大描述符集数量
        bool freeDescriptorSet = false;                              ///< 是否允许释放单个描述符集
        std::string debugName;                                       ///< 调试名称

        bool operator==(const DescriptorPoolDesc& other) const {
            return poolSizes == other.poolSizes &&
                maxSets == other.maxSets &&
                freeDescriptorSet == other.freeDescriptorSet;
        }

        bool operator!=(const DescriptorPoolDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 交换链描述结构体
     * @details 描述交换链的配置参数
     */
    struct SwapChainDesc {
        void* windowHandle = nullptr;               ///< 窗口句柄
        uint32_t width = 0;                         ///< 宽度
        uint32_t height = 0;                        ///< 高度
        uint32_t bufferCount = 2;                   ///< 缓冲区数量
        Format format = Format::RGBA8_UNorm;        ///< 格式
        ColorSpace colorSpace = ColorSpace::SRGBNonlinear;  ///< 颜色空间
        bool vsync = true;                          ///< 垂直同步
        bool fullscreen = false;                    ///< 全屏模式
        bool hdr = false;                           ///< HDR支持
        std::string debugName;                      ///< 调试名称

        bool operator==(const SwapChainDesc& other) const {
            return windowHandle == other.windowHandle &&
                width == other.width &&
                height == other.height &&
                bufferCount == other.bufferCount &&
                format == other.format &&
                colorSpace == other.colorSpace &&
                vsync == other.vsync &&
                fullscreen == other.fullscreen &&
                hdr == other.hdr;
        }

        bool operator!=(const SwapChainDesc& other) const {
            return !(*this == other);
        }
    };

    // ==================== 渲染通道结构体 ====================

   /**
    * @brief 附件描述结构体
    * @details 描述渲染通道中的单个附件
    */
    struct AttachmentDesc {
        Format format = Format::Undefined;         ///< 附件格式
        uint32_t sampleCount = 1;                  ///< 采样数
        AttachmentLoadOp loadOp = AttachmentLoadOp::DontCare; ///< 加载操作
        AttachmentStoreOp storeOp = AttachmentStoreOp::DontCare; ///< 存储操作
        AttachmentLoadOp stencilLoadOp = AttachmentLoadOp::DontCare; ///< 模板加载操作
        AttachmentStoreOp stencilStoreOp = AttachmentStoreOp::DontCare; ///< 模板存储操作
        ImageLayout initialLayout = ImageLayout::Undefined; ///< 初始布局
        ImageLayout finalLayout = ImageLayout::Undefined;   ///< 最终布局

        bool operator==(const AttachmentDesc& other) const {
            return format == other.format && sampleCount == other.sampleCount &&
                loadOp == other.loadOp && storeOp == other.storeOp &&
                stencilLoadOp == other.stencilLoadOp && stencilStoreOp == other.stencilStoreOp &&
                initialLayout == other.initialLayout && finalLayout == other.finalLayout;
        }

        bool operator!=(const AttachmentDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 附件引用结构体
     * @details 引用渲染通道中的特定附件
     */
    struct AttachmentReference {
        uint32_t attachment = 0;                     ///< 附件索引
        ImageLayout layout = ImageLayout::Undefined; ///< 附件布局

        bool operator==(const AttachmentReference& other) const {
            return attachment == other.attachment && layout == other.layout;
        }

        bool operator!=(const AttachmentReference& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 子通道描述结构体
     * @details 描述渲染通道中的一个子通道
     */
    struct SubpassDesc {
        std::vector<AttachmentReference> inputAttachments;       ///< 输入附件
        std::vector<AttachmentReference> colorAttachments;       ///< 颜色附件
        std::vector<AttachmentReference> resolveAttachments;     ///< 解析附件
        AttachmentReference depthStencilAttachment;              ///< 深度模板附件
        std::vector<uint32_t> preserveAttachments;               ///< 保留附件

        bool operator==(const SubpassDesc& other) const {
            return inputAttachments == other.inputAttachments &&
                colorAttachments == other.colorAttachments &&
                resolveAttachments == other.resolveAttachments &&
                depthStencilAttachment == other.depthStencilAttachment &&
                preserveAttachments == other.preserveAttachments;
        }

        bool operator!=(const SubpassDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 子通道依赖结构体
     * @details 描述子通道之间的执行依赖关系
     */
    struct SubpassDependency {
        uint32_t srcSubpass = 0;                     ///< 源子通道索引
        uint32_t dstSubpass = 0;                     ///< 目标子通道索引
        PipelineStage srcStageMask = PipelineStage::TopOfPipe; ///< 源阶段掩码
        PipelineStage dstStageMask = PipelineStage::BottomOfPipe; ///< 目标阶段掩码
        AccessFlag srcAccessMask = AccessFlag::None; ///< 源访问掩码
        AccessFlag dstAccessMask = AccessFlag::None; ///< 目标访问掩码
        bool byRegion = false;                       ///< 是否为区域依赖

        bool operator==(const SubpassDependency& other) const {
            return srcSubpass == other.srcSubpass && dstSubpass == other.dstSubpass &&
                srcStageMask == other.srcStageMask && dstStageMask == other.dstStageMask &&
                srcAccessMask == other.srcAccessMask && dstAccessMask == other.dstAccessMask &&
                byRegion == other.byRegion;
        }

        bool operator!=(const SubpassDependency& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 渲染通道描述结构体
     * @details 描述完整的渲染通道配置
     */
    struct RenderPassDesc {
        std::vector<AttachmentDesc> attachments;    ///< 附件列表
        std::vector<SubpassDesc> subpasses;         ///< 子通道列表
        std::vector<SubpassDependency> dependencies; ///< 依赖关系
        std::string debugName;                      ///< 调试名称

        bool operator==(const RenderPassDesc& other) const {
            return attachments == other.attachments &&
                subpasses == other.subpasses &&
                dependencies == other.dependencies;
        }

        bool operator!=(const RenderPassDesc& other) const {
            return !(*this == other);
        }
    };

    /**
   * @brief 帧缓冲描述结构体
   * @details 描述帧缓冲的配置
   */
    struct FramebufferDesc {
        void* renderPass = nullptr;                 ///< 渲染通道句柄
        std::vector<void*> attachments;             ///< 附件句柄列表
        Extent2D extent;                            ///< 尺寸
        uint32_t layers = 1;                        ///< 层数
        std::string debugName;                      ///< 调试名称

        bool operator==(const FramebufferDesc& other) const {
            return extent == other.extent && layers == other.layers;
        }

        bool operator!=(const FramebufferDesc& other) const {
            return !(*this == other);
        }
    };

    // ==================== 资源描述结构体 ====================

    /**
     * @brief 缓冲区描述结构体
     * @details 描述缓冲区的属性、用途和内存分配方式
     */
    struct BufferDesc {
        uint64_t size = 0;
        BufferType type = BufferType::Vertex;
        Format format = Format::Undefined;
        MemoryType memoryType = MemoryType::GPU_Only;
        uint32_t stride = 0;
        bool allowUpdate = false;
        bool allowReadback = false;
        bool allowRawViews = false;
        bool allowCounter = false;
        bool allowIndirectArgs = false;
        bool allowShaderAtomics = false;
        bool persistentMapped = false;
        std::string debugName;
        const void* initialData = nullptr;
        size_t initialDataSize = 0;

        BufferDesc(uint64_t size = 0,
            BufferType type = BufferType::Vertex,
            Format format = Format::Undefined,
            MemoryType memoryType = MemoryType::GPU_Only,
            uint32_t stride = 0,
            bool allowUpdate = false,
            bool allowReadback = false,
            bool allowRawViews = false,
            bool allowCounter = false,
            bool allowIndirectArgs = false,
            bool allowShaderAtomics = false,
            bool persistentMapped = false,
            const std::string& debugName = "")
            : size(size), type(type), format(format),
            memoryType(memoryType), stride(stride),
            allowUpdate(allowUpdate), allowReadback(allowReadback),
            allowRawViews(allowRawViews), allowCounter(allowCounter),
            allowIndirectArgs(allowIndirectArgs),
            allowShaderAtomics(allowShaderAtomics),
            persistentMapped(persistentMapped),
            debugName(debugName) {}

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

    // ==================== 着色器和管线结构体 ====================

    /**
     * @brief 顶点属性结构体
     * @details 描述顶点输入布局中的单个属性
     */
    struct VertexAttribute {
        uint32_t location = 0;          ///< 着色器中的位置索引
        uint32_t binding = 0;           ///< 绑定索引
        Format format = Format::Undefined; ///< 数据格式
        uint32_t offset = 0;            ///< 缓冲区中的偏移量
        std::string semanticName;       ///< 语义名称（DX兼容）

        bool operator==(const VertexAttribute& other) const {
            return location == other.location && binding == other.binding &&
                format == other.format && offset == other.offset &&
                semanticName == other.semanticName;
        }

        bool operator!=(const VertexAttribute& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 顶点布局结构体
     * @details 描述完整的顶点输入布局
     */
    struct VertexLayout {
        std::vector<VertexAttribute> attributes; ///< 属性列表
        uint32_t stride = 0;                     ///< 顶点步长
        VertexInputRate inputRate = VertexInputRate::PerVertex; ///< 输入速率
        uint32_t instanceStepRate = 1;           ///< 每N个实例步进一次

        bool operator==(const VertexLayout& other) const {
            return attributes == other.attributes && stride == other.stride &&
                inputRate == other.inputRate && instanceStepRate == other.instanceStepRate;
        }

        bool operator!=(const VertexLayout& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 着色器模块描述结构体
     * @details 描述着色器代码和编译选项
     */
    struct ShaderModuleDesc {
        ShaderStage stage = ShaderStage::Vertex; ///< 着色器阶段
        std::vector<uint8_t> code;              ///< SPIR-V/HLSL/Metal Shader字节码
        std::string entryPoint = "main";        ///< 入口函数名
        std::vector<std::string> defines;       ///< 预处理器定义
        std::vector<std::string> includePaths;  ///< 包含路径
        std::string debugName;                  ///< 调试名称

        bool operator==(const ShaderModuleDesc& other) const {
            return stage == other.stage && code == other.code &&
                entryPoint == other.entryPoint && defines == other.defines &&
                includePaths == other.includePaths;
        }

        bool operator!=(const ShaderModuleDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 描述符集布局绑定结构体
     * @details 描述单个描述符绑定
     */

    struct DescriptorSetLayoutBinding {
        uint32_t binding = 0;                     ///< 绑定索引
        DescriptorType type = DescriptorType::UniformBuffer; ///< 描述符类型
        uint32_t count = 1;                       ///< 数组元素个数
        ShaderStage stageFlags = ShaderStage::Vertex; ///< 可见的着色器阶段
        bool immutableSamplers = false;           ///< 是否为不可变采样器
        std::vector<SamplerDesc> samplerDescs;    ///< 不可变采样器描述

        bool operator==(const DescriptorSetLayoutBinding& other) const {
            return binding == other.binding && type == other.type &&
                count == other.count && stageFlags == other.stageFlags &&
                immutableSamplers == other.immutableSamplers &&
                samplerDescs == other.samplerDescs;
        }

        bool operator!=(const DescriptorSetLayoutBinding& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 推送常量范围结构体
     * @details 描述推送常量的内存布局
     */
    struct PushConstantRange {
        ShaderStage stage = ShaderStage::Vertex; ///< 可见的着色器阶段
        uint32_t offset = 0;                     ///< 偏移量
        uint32_t size = 0;                       ///< 大小

        bool operator==(const PushConstantRange& other) const {
            return stage == other.stage && offset == other.offset && size == other.size;
        }

        bool operator!=(const PushConstantRange& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 管线布局描述结构体
     * @details 描述管线的资源绑定布局
     */
    struct PipelineLayoutDesc {
        std::vector<std::vector<DescriptorSetLayoutBinding>> descriptorSets; ///< 描述符集布局
        std::vector<PushConstantRange> pushConstants; ///< 推送常量范围
        std::string debugName;                        ///< 调试名称

        bool operator==(const PipelineLayoutDesc& other) const {
            return descriptorSets == other.descriptorSets &&
                pushConstants == other.pushConstants;
        }

        bool operator!=(const PipelineLayoutDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 光栅化器状态结构体
     * @details 描述光栅化阶段的状态
     */
    struct RasterizerState {
        PolygonMode polygonMode = PolygonMode::Fill; ///< 多边形填充模式
        CullMode cullMode = CullMode::Back;          ///< 剔除模式
        FrontFace frontFace = FrontFace::CounterClockwise; ///< 正面方向
        float lineWidth = 1.0f;                     ///< 线宽
        bool depthBiasEnable = false;               ///< 是否启用深度偏移
        float depthBiasConstantFactor = 0.0f;       ///< 深度偏移常数因子
        float depthBiasClamp = 0.0f;                ///< 深度偏移钳位值
        float depthBiasSlopeFactor = 0.0f;          ///< 深度偏移斜率因子
        bool depthClampEnable = false;              ///< 是否启用深度钳位
        bool discardEnable = false;                 ///< 是否启用丢弃
        bool conservativeRasterEnable = false;      ///< 是否启用保守光栅化
        ConservativeRasterizationMode conservativeRasterMode = ConservativeRasterizationMode::Disabled; ///< 保守光栅化模式

        bool operator==(const RasterizerState& other) const {
            return polygonMode == other.polygonMode && cullMode == other.cullMode &&
                frontFace == other.frontFace && lineWidth == other.lineWidth &&
                depthBiasEnable == other.depthBiasEnable &&
                depthBiasConstantFactor == other.depthBiasConstantFactor &&
                depthBiasClamp == other.depthBiasClamp &&
                depthBiasSlopeFactor == other.depthBiasSlopeFactor &&
                depthClampEnable == other.depthClampEnable &&
                discardEnable == other.discardEnable &&
                conservativeRasterEnable == other.conservativeRasterEnable &&
                conservativeRasterMode == other.conservativeRasterMode;
        }

        bool operator!=(const RasterizerState& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 深度模板状态结构体
     * @details 描述深度和模板测试状态
     */
    struct DepthStencilState {
        bool depthTestEnable = true;                ///< 是否启用深度测试
        bool depthWriteEnable = true;               ///< 是否启用深度写入
        CompareOp depthCompareOp = CompareOp::Less; ///< 深度比较操作
        bool depthBoundsTestEnable = false;         ///< 是否启用深度边界测试
        float minDepthBounds = 0.0f;                ///< 最小深度边界
        float maxDepthBounds = 1.0f;                ///< 最大深度边界
        bool stencilTestEnable = false;             ///< 是否启用模板测试

        /// @brief 模板操作状态
        struct StencilOpState {
            StencilOp failOp = StencilOp::Keep;      ///< 模板测试失败操作
            StencilOp passOp = StencilOp::Keep;      ///< 模板测试通过操作
            StencilOp depthFailOp = StencilOp::Keep; ///< 深度测试失败操作
            CompareOp compareOp = CompareOp::Always; ///< 模板比较操作
            uint32_t compareMask = 0xFF;             ///< 比较掩码
            uint32_t writeMask = 0xFF;               ///< 写入掩码
            uint32_t reference = 0;                  ///< 参考值
        };

        StencilOpState front;                       ///< 正面模板状态
        StencilOpState back;                        ///< 背面模板状态

        bool operator==(const DepthStencilState& other) const {
            return depthTestEnable == other.depthTestEnable &&
                depthWriteEnable == other.depthWriteEnable &&
                depthCompareOp == other.depthCompareOp &&
                depthBoundsTestEnable == other.depthBoundsTestEnable &&
                minDepthBounds == other.minDepthBounds &&
                maxDepthBounds == other.maxDepthBounds &&
                stencilTestEnable == other.stencilTestEnable &&
                front.failOp == other.front.failOp && front.passOp == other.front.passOp &&
                front.depthFailOp == other.front.depthFailOp && front.compareOp == other.front.compareOp &&
                front.compareMask == other.front.compareMask && front.writeMask == other.front.writeMask &&
                front.reference == other.front.reference &&
                back.failOp == other.back.failOp && back.passOp == other.back.passOp &&
                back.depthFailOp == other.back.depthFailOp && back.compareOp == other.back.compareOp &&
                back.compareMask == other.back.compareMask && back.writeMask == other.back.writeMask &&
                back.reference == other.back.reference;
        }

        bool operator!=(const DepthStencilState& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 混合附件状态结构体
     * @details 描述单个渲染目标的混合状态
     */
    struct BlendAttachmentState {
        bool blendEnable = false;                          ///< 是否启用混合
        BlendFactor srcColorBlendFactor = BlendFactor::One; ///< 源颜色混合因子
        BlendFactor dstColorBlendFactor = BlendFactor::Zero; ///< 目标颜色混合因子
        BlendOp colorBlendOp = BlendOp::Add;               ///< 颜色混合操作
        BlendFactor srcAlphaBlendFactor = BlendFactor::One; ///< 源Alpha混合因子
        BlendFactor dstAlphaBlendFactor = BlendFactor::Zero; ///< 目标Alpha混合因子
        BlendOp alphaBlendOp = BlendOp::Add;               ///< Alpha混合操作
        ColorComponent colorWriteMask = ColorComponent::All; ///< 颜色写入掩码

        bool operator==(const BlendAttachmentState& other) const {
            return blendEnable == other.blendEnable &&
                srcColorBlendFactor == other.srcColorBlendFactor &&
                dstColorBlendFactor == other.dstColorBlendFactor &&
                colorBlendOp == other.colorBlendOp &&
                srcAlphaBlendFactor == other.srcAlphaBlendFactor &&
                dstAlphaBlendFactor == other.dstAlphaBlendFactor &&
                alphaBlendOp == other.alphaBlendOp &&
                colorWriteMask == other.colorWriteMask;
        }

        bool operator!=(const BlendAttachmentState& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 颜色混合状态结构体
     * @details 描述所有渲染目标的混合状态
     */
    struct ColorBlendState {
        std::vector<BlendAttachmentState> attachments; ///< 各个附件的混合状态
        bool logicOpEnable = false;                    ///< 是否启用逻辑操作
        LogicOp logicOp = LogicOp::Copy;               ///< 逻辑操作类型
        std::array<float, 4> blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }; ///< 混合常数

        bool operator==(const ColorBlendState& other) const {
            return attachments == other.attachments &&
                logicOpEnable == other.logicOpEnable &&
                logicOp == other.logicOp &&
                blendConstants == other.blendConstants;
        }

        bool operator!=(const ColorBlendState& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 图形管线描述结构体
     * @details 描述完整的图形渲染管线配置
     */
    struct GraphicsPipelineDesc {
        // 着色器阶段
        ShaderModuleDesc vertexShader;
        ShaderModuleDesc tessellationControlShader;
        ShaderModuleDesc tessellationEvaluationShader;
        ShaderModuleDesc geometryShader;
        ShaderModuleDesc fragmentShader;

        // 顶点输入
        VertexLayout vertexLayout;

        // 输入装配
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;
        bool primitiveRestartEnable = false;

        // 管线状态
        RasterizerState rasterizer;
        DepthStencilState depthStencil;
        ColorBlendState colorBlend;

        // 动态状态
        std::vector<std::string> dynamicStates;  // "Viewport", "Scissor", "LineWidth", etc.

        // 渲染目标
        std::vector<Format> renderTargetFormats;
        Format depthStencilFormat = Format::Undefined;
        uint32_t sampleCount = 1;
        uint32_t sampleMask = 0xFFFFFFFF;
        bool alphaToCoverageEnable = false;
        bool alphaToOneEnable = false;

        // 管线布局
        PipelineLayoutDesc layoutDesc;

        // 渲染子通道
        void* renderPass = nullptr;
        uint32_t subpass = 0;

        std::string debugName;

        bool operator==(const GraphicsPipelineDesc& other) const {
            return vertexLayout == other.vertexLayout &&
                topology == other.topology &&
                rasterizer == other.rasterizer &&
                depthStencil == other.depthStencil &&
                colorBlend == other.colorBlend &&
                renderTargetFormats == other.renderTargetFormats &&
                depthStencilFormat == other.depthStencilFormat &&
                sampleCount == other.sampleCount &&
                layoutDesc == other.layoutDesc;
        }

        bool operator!=(const GraphicsPipelineDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 计算管线描述结构体
     * @details 描述计算着色器管线配置
     */
    struct ComputePipelineDesc {
        ShaderModuleDesc computeShader;      ///< 计算着色器
        PipelineLayoutDesc layoutDesc;       ///< 管线布局
        std::string debugName;               ///< 调试名称

        bool operator==(const ComputePipelineDesc& other) const {
            return layoutDesc == other.layoutDesc;
        }

        bool operator!=(const ComputePipelineDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 光线追踪管线描述结构体
     * @details 描述光线追踪管线配置
     */
    struct RayTracingPipelineDesc {
        std::vector<ShaderModuleDesc> shaders;            ///< 着色器列表
        std::vector<RayTracingShaderGroupType> shaderGroups; ///< 着色器组类型
        uint32_t maxRecursionDepth = 1;                   ///< 最大递归深度
        PipelineLayoutDesc layoutDesc;                    ///< 管线布局
        std::string debugName;                            ///< 调试名称

        bool operator==(const RayTracingPipelineDesc& other) const {
            return layoutDesc == other.layoutDesc &&
                maxRecursionDepth == other.maxRecursionDepth;
        }

        bool operator!=(const RayTracingPipelineDesc& other) const {
            return !(*this == other);
        }
    };


    // ==================== 查询结构体 ====================
    /**
     * @brief 查询池描述结构体
     * @details 描述查询池的配置
     */
    struct QueryPoolDesc {
        QueryType type = QueryType::Timestamp;      ///< 查询类型
        uint32_t count = 0;                         ///< 查询数量
        std::vector<PipelineStatistic> pipelineStatistics; ///< 管线统计类型
        std::string debugName;                      ///< 调试名称

        bool operator==(const QueryPoolDesc& other) const {
            return type == other.type && count == other.count &&
                pipelineStatistics == other.pipelineStatistics;
        }

        bool operator!=(const QueryPoolDesc& other) const {
            return !(*this == other);
        }
    };

    // ==================== 同步结构体 ====================

    /**
     * @brief 栅栏描述结构体
     * @details 描述GPU-CPU同步栅栏
     */
    struct FenceDesc {
        bool signaled = false;                      ///< 是否已发出信号
        std::string debugName;                      ///< 调试名称

        bool operator==(const FenceDesc& other) const {
            return signaled == other.signaled;
        }

        bool operator!=(const FenceDesc& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 信号量描述结构体
     * @details 描述GPU-GPU同步信号量
     */
    struct SemaphoreDesc {
        std::string debugName;                      ///< 调试名称

        bool operator==(const SemaphoreDesc& other) const {
            return true;  // 所有信号量描述都一样
        }

        bool operator!=(const SemaphoreDesc& other) const {
            return false;
        }
    };

    /**
     * @brief 事件描述结构体
     * @details 描述GPU内部同步事件
     */
    struct EventDesc {
        std::string debugName;                      ///< 调试名称

        bool operator==(const EventDesc& other) const {
            return true;  // 所有事件描述都一样
        }

        bool operator!=(const EventDesc& other) const {
            return false;
        }
    };
}