#pragma once
#include "RHI_ENUMS.hpp"
#include <cstdint>
#include <array>
#include <glm/glm.hpp>

namespace StarryEngine::RHI {

    // ==================== 基础几何和数学结构体 ====================
    /**
     * @brief 版本号结构体
     * @details 用于表示API版本，支持打包为32位整数
     */
    struct Version {
        uint32_t major = 1;
        uint32_t minor = 0;
        uint32_t patch = 0;

        constexpr uint32_t packed() const {
            // Vulkan版本打包格式: major << 22 | minor << 12 | patch
            return (0 << 29) | (major << 22) | (minor << 12) | patch;
        }

        // 转换为字符串
        std::string toString() const {
            return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
        }

        // 比较运算符
        bool operator==(const Version& other) const {
            return major == other.major && minor == other.minor && patch == other.patch;
        }

        bool operator!=(const Version& other) const { return !(*this == other); }

        bool operator<(const Version& other) const {
            if (major != other.major) return major < other.major;
            if (minor != other.minor) return minor < other.minor;
            return patch < other.patch;
        }

        bool operator<=(const Version& other) const { return *this < other || *this == other; }
        bool operator>(const Version& other) const { return !(*this <= other); }
        bool operator>=(const Version& other) const { return !(*this < other); }
    };

    /**
     * @brief 2D范围结构体
     * @details 用于表示宽度和高度
     */
    struct Extent2D {
        uint32_t width = 0;
        uint32_t height = 0;

        bool operator==(const Extent2D& other) const {
            return width == other.width && height == other.height;
        }

        bool operator!=(const Extent2D& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 3D范围结构体
     * @details 继承自Extent2D，增加深度维度
     */
    struct Extent3D : Extent2D {
        uint32_t depth = 1;

        bool operator==(const Extent3D& other) const {
            return width == other.width && height == other.height && depth == other.depth;
        }

        bool operator!=(const Extent3D& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 2D偏移结构体
     * @details 用于表示二维空间中的偏移量
     */
    struct Offset2D {
        int32_t x = 0;
        int32_t y = 0;

        bool operator==(const Offset2D& other) const {
            return x == other.x && y == other.y;
        }

        bool operator!=(const Offset2D& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 3D偏移结构体
     * @details 继承自Offset2D，增加Z轴偏移
     */
    struct Offset3D : Offset2D {
        int32_t z = 0;

        bool operator==(const Offset3D& other) const {
            return x == other.x && y == other.y && z == other.z;
        }

        bool operator!=(const Offset3D& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 2D矩形结构体
     * @details 由偏移和范围组成，用于表示屏幕空间区域
     */
    struct Rect2D {
        Offset2D offset;
        Extent2D extent;

        bool operator==(const Rect2D& other) const {
            return offset == other.offset && extent == other.extent;
        }

        bool operator!=(const Rect2D& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 视口结构体
     * @details 用于定义视口变换参数
     */
    struct Viewport {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float minDepth = 0.0f;
        float maxDepth = 1.0f;

        bool operator==(const Viewport& other) const {
            return x == other.x && y == other.y &&
                width == other.width && height == other.height &&
                minDepth == other.minDepth && maxDepth == other.maxDepth;
        }

        bool operator!=(const Viewport& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 颜色结构体
     * @details 使用RGBA浮点格式表示颜色，提供常用颜色常量
     */
    struct Color {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;

        Color() = default;
        Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

        static Color Black() { return Color(0.0f, 0.0f, 0.0f, 1.0f); }
        static Color White() { return Color(1.0f, 1.0f, 1.0f, 1.0f); }
        static Color Red() { return Color(1.0f, 0.0f, 0.0f, 1.0f); }
        static Color Green() { return Color(0.0f, 1.0f, 0.0f, 1.0f); }
        static Color Blue() { return Color(0.0f, 0.0f, 1.0f, 1.0f); }
        static Color Transparent() { return Color(0.0f, 0.0f, 0.0f, 0.0f); }

        bool operator==(const Color& other) const {
            return r == other.r && g == other.g && b == other.b && a == other.a;
        }

        bool operator!=(const Color& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 清除值结构体
     * @details 用于渲染目标或深度/模板缓冲区的清除值
     */
    struct ClearValue {
        Color color;
        float depth = 1.0f;
        uint32_t stencil = 0;

        ClearValue() = default;
        explicit ClearValue(const Color& color) : color(color) {}
        ClearValue(float depth, uint32_t stencil = 0) : depth(depth), stencil(stencil) {}

        bool operator==(const ClearValue& other) const {
            return color == other.color && depth == other.depth && stencil == other.stencil;
        }

        bool operator!=(const ClearValue& other) const {
            return !(*this == other);
        }
    };

    // ==================== 拷贝操作相关结构体 ====================

    /**
     * @brief 图像子资源范围结构体
     * @details 描述图像的特定子资源区域
     */
    struct ImageSubresourceRange {
        ImageAspect aspectMask = ImageAspect::Color;  ///< 图像切面掩码
        uint32_t baseMipLevel = 0;                   ///< 基础MIP层级
        uint32_t levelCount = 1;                     ///< MIP层级数量
        uint32_t baseArrayLayer = 0;                 ///< 基础数组层
        uint32_t layerCount = 1;                     ///< 数组层数量

        bool operator==(const ImageSubresourceRange& other) const {
            return aspectMask == other.aspectMask &&
                baseMipLevel == other.baseMipLevel &&
                levelCount == other.levelCount &&
                baseArrayLayer == other.baseArrayLayer &&
                layerCount == other.layerCount;
        }

        bool operator!=(const ImageSubresourceRange& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 缓冲区拷贝区域结构体
     * @details 描述缓冲区到缓冲区的拷贝区域
     */
    struct BufferCopyRegion {
        uint64_t srcOffset = 0;      ///< 源缓冲区偏移
        uint64_t dstOffset = 0;      ///< 目标缓冲区偏移
        uint64_t size = 0;           ///< 拷贝大小

        bool operator==(const BufferCopyRegion& other) const {
            return srcOffset == other.srcOffset &&
                dstOffset == other.dstOffset &&
                size == other.size;
        }

        bool operator!=(const BufferCopyRegion& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 图像拷贝区域结构体
     * @details 描述图像到图像的拷贝区域
     */
    struct ImageCopyRegion {
        ImageSubresourceRange srcSubresource;    ///< 源图像子资源
        Offset3D srcOffset;                      ///< 源图像偏移
        ImageSubresourceRange dstSubresource;    ///< 目标图像子资源
        Offset3D dstOffset;                      ///< 目标图像偏移
        Extent3D extent;                         ///< 拷贝范围

        bool operator==(const ImageCopyRegion& other) const {
            return srcSubresource == other.srcSubresource &&
                srcOffset == other.srcOffset &&
                dstSubresource == other.dstSubresource &&
                dstOffset == other.dstOffset &&
                extent == other.extent;
        }

        bool operator!=(const ImageCopyRegion& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 缓冲区到图像拷贝区域结构体
     * @details 描述缓冲区到图像的拷贝区域
     */
    struct BufferImageCopyRegion {
        uint64_t bufferOffset = 0;               ///< 缓冲区偏移
        uint32_t bufferRowLength = 0;            ///< 缓冲区行长度
        uint32_t bufferImageHeight = 0;          ///< 缓冲区图像高度
        ImageSubresourceRange imageSubresource;  ///< 图像子资源
        Offset3D imageOffset;                    ///< 图像偏移
        Extent3D imageExtent;                    ///< 图像范围

        bool operator==(const BufferImageCopyRegion& other) const {
            return bufferOffset == other.bufferOffset &&
                bufferRowLength == other.bufferRowLength &&
                bufferImageHeight == other.bufferImageHeight &&
                imageSubresource == other.imageSubresource &&
                imageOffset == other.imageOffset &&
                imageExtent == other.imageExtent;
        }

        bool operator!=(const BufferImageCopyRegion& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 图像位块传送区域结构体
     * @details 描述图像的位块传送操作区域
     */
    struct ImageBlitRegion {
        ImageSubresourceRange srcSubresource;        ///< 源图像子资源
        std::array<Offset3D, 2> srcOffsets;          ///< 源图像两个角点偏移
        ImageSubresourceRange dstSubresource;        ///< 目标图像子资源
        std::array<Offset3D, 2> dstOffsets;          ///< 目标图像两个角点偏移

        bool operator==(const ImageBlitRegion& other) const {
            return srcSubresource == other.srcSubresource &&
                srcOffsets == other.srcOffsets &&
                dstSubresource == other.dstSubresource &&
                dstOffsets == other.dstOffsets;
        }

        bool operator!=(const ImageBlitRegion& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 清除附件结构体
     * @details 描述要清除的附件信息
     */
    struct ClearAttachment {
        ImageAspect aspectMask = ImageAspect::Color;  ///< 图像切面掩码
        uint32_t colorAttachment = 0;                 ///< 颜色附件索引
        ClearValue clearValue;                        ///< 清除值

        bool operator==(const ClearAttachment& other) const {
            return aspectMask == other.aspectMask &&
                colorAttachment == other.colorAttachment &&
                clearValue == other.clearValue;
        }

        bool operator!=(const ClearAttachment& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 清除矩形结构体
     * @details 描述要清除的矩形区域
     */
    struct ClearRect {
        Rect2D rect;                       ///< 矩形区域
        uint32_t baseArrayLayer = 0;       ///< 基础数组层
        uint32_t layerCount = 1;           ///< 数组层数量

        bool operator==(const ClearRect& other) const {
            return rect == other.rect &&
                baseArrayLayer == other.baseArrayLayer &&
                layerCount == other.layerCount;
        }

        bool operator!=(const ClearRect& other) const {
            return !(*this == other);
        }
    };

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

            bool operator==(const InputAttribute& other) const {
                return name == other.name &&
                    location == other.location &&
                    format == other.format &&
                    offset == other.offset;
            }

            bool operator!=(const InputAttribute& other) const {
                return !(*this == other);
            }
        };

        /**
         * @brief 输出属性结构体
         */
        struct OutputAttribute {
            std::string name;      ///< 属性名称
            uint32_t location;     ///< 位置索引
            Format format;         ///< 格式

            bool operator==(const OutputAttribute& other) const {
                return name == other.name &&
                    location == other.location &&
                    format == other.format;
            }

            bool operator!=(const OutputAttribute& other) const {
                return !(*this == other);
            }
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

            bool operator==(const ResourceBinding& other) const {
                return name == other.name &&
                    binding == other.binding &&
                    set == other.set &&
                    type == other.type &&
                    count == other.count &&
                    stage == other.stage;
            }

            bool operator!=(const ResourceBinding& other) const {
                return !(*this == other);
            }
        };

        /**
         * @brief 推送常量结构体
         */
        struct PushConstant {
            std::string name;      ///< 常量名称
            uint32_t offset;       ///< 偏移量
            uint32_t size;         ///< 大小
            ShaderStage stage;     ///< 着色器阶段

            bool operator==(const PushConstant& other) const {
                return name == other.name &&
                    offset == other.offset &&
                    size == other.size &&
                    stage == other.stage;
            }

            bool operator!=(const PushConstant& other) const {
                return !(*this == other);
            }
        };

        /**
         * @brief 常量结构体
         */
        struct Constant {
            std::string name;      ///< 常量名称
            uint32_t offset;       ///< 偏移量
            uint32_t size;         ///< 大小

            bool operator==(const Constant& other) const {
                return name == other.name &&
                    offset == other.offset &&
                    size == other.size;
            }

            bool operator!=(const Constant& other) const {
                return !(*this == other);
            }
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

    // ==================== 资源屏障结构体 ====================

    /**
     * @brief 缓冲区屏障结构体
     * @details 描述缓冲区访问同步屏障
     */
    struct BufferBarrier {
        void* buffer = nullptr;                     ///< 缓冲区句柄
        AccessFlag srcAccessMask = AccessFlag::None; ///< 源访问掩码
        AccessFlag dstAccessMask = AccessFlag::None; ///< 目标访问掩码
        uint64_t offset = 0;                        ///< 偏移量
        uint64_t size = 0;                          ///< 大小
        uint32_t srcQueueFamilyIndex = 0;           ///< 源队列族索引
        uint32_t dstQueueFamilyIndex = 0;           ///< 目标队列族索引

        bool operator==(const BufferBarrier& other) const {
            return buffer == other.buffer &&
                srcAccessMask == other.srcAccessMask &&
                dstAccessMask == other.dstAccessMask &&
                offset == other.offset && size == other.size &&
                srcQueueFamilyIndex == other.srcQueueFamilyIndex &&
                dstQueueFamilyIndex == other.dstQueueFamilyIndex;
        }

        bool operator!=(const BufferBarrier& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 图像屏障结构体
     * @details 描述图像访问同步屏障
     */
    struct ImageBarrier {
        void* image = nullptr;                      ///< 图像句柄
        ImageLayout oldLayout = ImageLayout::Undefined; ///< 旧布局
        ImageLayout newLayout = ImageLayout::Undefined; ///< 新布局
        AccessFlag srcAccessMask = AccessFlag::None; ///< 源访问掩码
        AccessFlag dstAccessMask = AccessFlag::None; ///< 目标访问掩码
        ImageAspect aspectMask = ImageAspect::Color; ///< 图像切面掩码
        uint32_t srcQueueFamilyIndex = 0;           ///< 源队列族索引
        uint32_t dstQueueFamilyIndex = 0;           ///< 目标队列族索引
        uint32_t baseMipLevel = 0;                  ///< 基础MIP层级
        uint32_t levelCount = 1;                    ///< 层级数量
        uint32_t baseArrayLayer = 0;                ///< 基础数组层
        uint32_t layerCount = 1;                    ///< 层数量

        bool operator==(const ImageBarrier& other) const {
            return image == other.image &&
                oldLayout == other.oldLayout && newLayout == other.newLayout &&
                srcAccessMask == other.srcAccessMask && dstAccessMask == other.dstAccessMask &&
                aspectMask == other.aspectMask &&
                srcQueueFamilyIndex == other.srcQueueFamilyIndex &&
                dstQueueFamilyIndex == other.dstQueueFamilyIndex &&
                baseMipLevel == other.baseMipLevel && levelCount == other.levelCount &&
                baseArrayLayer == other.baseArrayLayer && layerCount == other.layerCount;
        }

        bool operator!=(const ImageBarrier& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 内存屏障结构体
     * @details 描述内存访问同步屏障
     */
    struct MemoryBarrier {
        AccessFlag srcAccessMask = AccessFlag::None; ///< 源访问掩码
        AccessFlag dstAccessMask = AccessFlag::None; ///< 目标访问掩码

        bool operator==(const MemoryBarrier& other) const {
            return srcAccessMask == other.srcAccessMask && dstAccessMask == other.dstAccessMask;
        }

        bool operator!=(const MemoryBarrier& other) const {
            return !(*this == other);
        }
    };

} // namespace StarryEngine::RHI