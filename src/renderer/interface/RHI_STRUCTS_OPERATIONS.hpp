#pragma once
#include "RHI_ENUMS.hpp"
#include "RHI_STRUCTS_BASE.hpp"
#include "RHI_STRUCTS_COPY_OPERATIONS.hpp"
#include <vector>

namespace StarryEngine::RHI {

    // ==================== 操作相关结构体 ====================

    /**
     * @brief 渲染通道开始信息结构体
     * @details 描述渲染通道的开始参数
     */
    struct RenderPassBeginInfo {
        void* renderPass = nullptr;                    ///< 渲染通道句柄
        void* framebuffer = nullptr;                   ///< 帧缓冲句柄
        Rect2D renderArea;                             ///< 渲染区域
        std::vector<ClearValue> clearValues;           ///< 清除值列表

        bool operator==(const RenderPassBeginInfo& other) const {
            return renderPass == other.renderPass &&
                framebuffer == other.framebuffer &&
                renderArea == other.renderArea &&
                clearValues == other.clearValues;
        }

        bool operator!=(const RenderPassBeginInfo& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 加速结构构建信息结构体
     * @details 描述加速结构的构建参数
     */
    struct AccelerationStructureBuildInfo {
        AccelerationStructureType type = AccelerationStructureType::BottomLevel;  ///< 加速结构类型
        BuildAccelerationStructureMode buildMode = BuildAccelerationStructureMode::Build;  ///< 构建模式
        void* dstAccelerationStructure = nullptr;      ///< 目标加速结构句柄
        void* srcAccelerationStructure = nullptr;      ///< 源加速结构句柄（用于更新）
        std::vector<void*> geometries;                 ///< 几何体列表
        std::vector<void*> instances;                  ///< 实例列表
        uint64_t scratchDataOffset = 0;                ///< 暂存数据偏移

        bool operator==(const AccelerationStructureBuildInfo& other) const {
            return type == other.type &&
                buildMode == other.buildMode &&
                dstAccelerationStructure == other.dstAccelerationStructure &&
                srcAccelerationStructure == other.srcAccelerationStructure &&
                scratchDataOffset == other.scratchDataOffset;
        }

        bool operator!=(const AccelerationStructureBuildInfo& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 描述符拷贝结构体
     * @details 描述描述符的拷贝操作
     */
    struct DescriptorCopy {
        void* srcSet = nullptr;                ///< 源描述符集句柄
        uint32_t srcBinding = 0;               ///< 源绑定索引
        uint32_t srcArrayElement = 0;          ///< 源数组元素
        void* dstSet = nullptr;                ///< 目标描述符集句柄
        uint32_t dstBinding = 0;               ///< 目标绑定索引
        uint32_t dstArrayElement = 0;          ///< 目标数组元素
        uint32_t descriptorCount = 1;          ///< 描述符数量

        bool operator==(const DescriptorCopy& other) const {
            return srcSet == other.srcSet &&
                srcBinding == other.srcBinding &&
                srcArrayElement == other.srcArrayElement &&
                dstSet == other.dstSet &&
                dstBinding == other.dstBinding &&
                dstArrayElement == other.dstArrayElement &&
                descriptorCount == other.descriptorCount;
        }

        bool operator!=(const DescriptorCopy& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 提交信息结构体
     * @details 描述命令缓冲区提交到队列的信息
     */
    struct SubmitInfo {
        std::vector<void*> waitSemaphores;                 ///< 等待信号量列表
        std::vector<PipelineStage> waitDstStageMasks;      ///< 等待阶段掩码
        std::vector<void*> commandBuffers;                 ///< 命令缓冲区列表
        std::vector<void*> signalSemaphores;               ///< 信号信号量列表

        bool operator==(const SubmitInfo& other) const {
            return waitSemaphores == other.waitSemaphores &&
                waitDstStageMasks == other.waitDstStageMasks &&
                commandBuffers == other.commandBuffers &&
                signalSemaphores == other.signalSemaphores;
        }

        bool operator!=(const SubmitInfo& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 呈现信息结构体
     * @details 描述交换链呈现的信息
     */
    struct PresentInfo {
        std::vector<void*> waitSemaphores;     ///< 等待信号量列表
        std::vector<void*> swapChains;         ///< 交换链列表
        std::vector<uint32_t> imageIndices;    ///< 图像索引列表

        bool operator==(const PresentInfo& other) const {
            return waitSemaphores == other.waitSemaphores &&
                swapChains == other.swapChains &&
                imageIndices == other.imageIndices;
        }

        bool operator!=(const PresentInfo& other) const {
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

    /**
     * @brief 性能分析结果结构体
     * @details 包含性能分析的相关数据
     */
    struct ProfilingResult {
        std::string name;                           ///< 分析名称
        double cpuTime = 0.0;                       ///< CPU时间（毫秒）
        double gpuTime = 0.0;                       ///< GPU时间（毫秒）
        uint64_t frameCount = 0;                    ///< 帧计数
        double averageCpuTime = 0.0;                ///< 平均CPU时间
        double averageGpuTime = 0.0;                ///< 平均GPU时间
        double minCpuTime = 0.0;                    ///< 最小CPU时间
        double maxCpuTime = 0.0;                    ///< 最大CPU时间
        double minGpuTime = 0.0;                    ///< 最小GPU时间
        double maxGpuTime = 0.0;                    ///< 最大GPU时间

        bool operator==(const ProfilingResult& other) const {
            return name == other.name &&
                cpuTime == other.cpuTime &&
                gpuTime == other.gpuTime &&
                frameCount == other.frameCount &&
                averageCpuTime == other.averageCpuTime &&
                averageGpuTime == other.averageGpuTime &&
                minCpuTime == other.minCpuTime &&
                maxCpuTime == other.maxCpuTime &&
                minGpuTime == other.minGpuTime &&
                maxGpuTime == other.maxGpuTime;
        }

        bool operator!=(const ProfilingResult& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 着色器反射信息结构体
     * @details 包含从着色器字节码中提取的反射信息
     */
    struct ShaderReflectionInfo {
        /**
         * @brief 输入属性结构体
         */
        struct InputAttribute {
            std::string name;      ///< 属性名称
            uint32_t location;     ///< 位置索引
            Format format;         ///< 格式
            uint32_t offset;       ///< 偏移量
        };

        /**
         * @brief 输出属性结构体
         */
        struct OutputAttribute {
            std::string name;      ///< 属性名称
            uint32_t location;     ///< 位置索引
            Format format;         ///< 格式
        };

        /**
         * @brief 资源绑定结构体
         */
        struct ResourceBinding {
            std::string name;      ///< 资源名称
            uint32_t binding;      ///< 绑定索引
            uint32_t set;          ///< 描述符集索引
            DescriptorType type;   ///< 描述符类型
            uint32_t count;        ///< 数组元素数量
            ShaderStage stage;     ///< 着色器阶段
        };

        /**
         * @brief 推送常量结构体
         */
        struct PushConstant {
            std::string name;      ///< 常量名称
            uint32_t offset;       ///< 偏移量
            uint32_t size;         ///< 大小
            ShaderStage stage;     ///< 着色器阶段
        };

        /**
         * @brief 常量结构体
         */
        struct Constant {
            std::string name;      ///< 常量名称
            uint32_t offset;       ///< 偏移量
            uint32_t size;         ///< 大小
        };

        std::vector<InputAttribute> inputAttributes;     ///< 输入属性列表
        std::vector<OutputAttribute> outputAttributes;   ///< 输出属性列表
        std::vector<ResourceBinding> resourceBindings;   ///< 资源绑定列表
        std::vector<PushConstant> pushConstants;         ///< 推送常量列表
        std::vector<Constant> constants;                 ///< 常量列表

        uint32_t workGroupSizeX = 1;    ///< 工作组X大小
        uint32_t workGroupSizeY = 1;    ///< 工作组Y大小
        uint32_t workGroupSizeZ = 1;    ///< 工作组Z大小

        bool operator==(const ShaderReflectionInfo& other) const {
            return inputAttributes == other.inputAttributes &&
                outputAttributes == other.outputAttributes &&
                resourceBindings == other.resourceBindings &&
                pushConstants == other.pushConstants &&
                constants == other.constants &&
                workGroupSizeX == other.workGroupSizeX &&
                workGroupSizeY == other.workGroupSizeY &&
                workGroupSizeZ == other.workGroupSizeZ;
        }

        bool operator!=(const ShaderReflectionInfo& other) const {
            return !(*this == other);
        }
    };

} // namespace StarryEngine::RHI