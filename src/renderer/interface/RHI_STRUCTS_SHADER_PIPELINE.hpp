#pragma once
#include "RHI_ENUMS.hpp"
#include "RHI_STRUCTS_BASE.hpp"
#include <vector>
#include <string>

namespace StarryEngine::RHI {

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

        // 渲染子通道（Vulkan特定）
        void* renderPass = nullptr;  // 特定API的渲染通道句柄
        uint32_t subpass = 0;

        // 其他
        std::string debugName;

        bool operator==(const GraphicsPipelineDesc& other) const {
            // 简化的相等比较，实际使用可能需要更复杂的比较
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

} // namespace StarryEngine::RHI