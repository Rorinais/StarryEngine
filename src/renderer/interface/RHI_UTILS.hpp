#include "RHI_INTERFACE.hpp"
#include <cmath>
#include <algorithm>
#include <random>
#include <cstring>
#include <array>
#include <fstream>
#include <sstream>

namespace StarryEngine::RHI {

#define SHADER_STAGE_FLAG(flag) (static_cast<std::underlying_type_t<ShaderStage>>(stage) & static_cast<std::underlying_type_t<ShaderStage>>(ShaderStage::flag))

    class RHIUtils {
    public:
        // 格式支持查询
        static bool isDepthFormat(Format format);
        static bool isStencilFormat(Format format);
        static bool isDepthStencilFormat(Format format);
        static bool isCompressedFormat(Format format);
        static bool isSRGBFormat(Format format);
        static bool isIntegerFormat(Format format);
        static bool isFloatFormat(Format format);
        static bool isNormalizedFormat(Format format);

        static uint32_t getFormatSize(Format format);
        static uint32_t getFormatComponentCount(Format format);
        static Format getSRGBFormat(Format format);
        static Format getLinearFormat(Format format);
        static Format getDepthFormat(uint32_t depthBits, bool stencil);
        static std::string formatToString(Format format);

        // 内存对齐
        static uint64_t alignUp(uint64_t value, uint64_t alignment);
        static uint64_t alignDown(uint64_t value, uint64_t alignment);
        static bool isAligned(uint64_t value, uint64_t alignment);

        // 资源大小计算
        static uint64_t calculateTextureSize(const TextureDesc& desc);
        static uint64_t calculateBufferSize(const BufferDesc& desc);
        static uint32_t calculateMipLevels(uint32_t width, uint32_t height, uint32_t depth = 1);
        static Extent3D calculateMipExtent(const Extent3D& baseExtent, uint32_t mipLevel);

        // 着色器工具
        static std::vector<uint8_t> compileShader(
            const std::string& source,
            ShaderStage stage,
            API targetAPI,
            const std::string& entryPoint = "main",
            const std::vector<std::string>& defines = {},
            const std::vector<std::string>& includePaths = {});

        static bool decompileShader(
            const std::vector<uint8_t>& bytecode,
            std::string& source,
            API sourceAPI);

        static bool reflectShader(
            const std::vector<uint8_t>& bytecode,
            ShaderReflectionInfo& reflection,
            API shaderAPI);

        // 纹理加载
        static std::unique_ptr<RHITexture> loadTexture(
            IRHIContext* context,
            const std::string& filepath,
            bool generateMips = true,
            bool srgb = false);

        static std::unique_ptr<RHITexture> createTextureFromData(
            IRHIContext* context,
            const void* data,
            uint32_t width,
            uint32_t height,
            Format format,
            bool generateMips = true);

        // 模型加载和缓冲区创建
        template<typename VertexType>
        static std::unique_ptr<RHIBuffer> createVertexBuffer(
            IRHIContext* context,
            const std::vector<VertexType>& vertices,
            const std::string& name = "") {

            BufferDesc desc;
            desc.size = sizeof(VertexType) * vertices.size();
            desc.type = BufferType::Vertex;
            desc.memoryType = MemoryType::GPU_Only;
            desc.allowUpdate = false;
            desc.debugName = name;

            auto buffer = context->createBuffer(desc);
            if (buffer) {
                // 通过暂存缓冲区上传数据
                auto staging = context->createBuffer({
                    desc.size,
                    BufferType::Staging,
                    MemoryType::CPU_To_GPU,
                    Format::Undefined,
                    0,
                    true,
                    false,
                    false,
                    false,
                    false,
                    false,
                    false,
                    name + "_Staging"
                    });

                if (staging) {
                    void* mapped = staging->map();
                    if (mapped) {
                        memcpy(mapped, vertices.data(), desc.size);
                        staging->unmap();

                        // 执行拷贝命令
                        // 这里需要命令缓冲区来执行拷贝
                        // 简化实现，实际需要完整的命令录制
                    }
                    context->destroyBuffer(staging);
                }
            }
            return std::unique_ptr<RHIBuffer>(buffer);
        }

        template<typename IndexType>
        static std::unique_ptr<RHIBuffer> createIndexBuffer(
            IRHIContext* context,
            const std::vector<IndexType>& indices,
            const std::string& name = "") {

            BufferDesc desc;
            desc.size = sizeof(IndexType) * indices.size();
            desc.type = BufferType::Index;
            desc.memoryType = MemoryType::GPU_Only;
            desc.allowUpdate = false;
            desc.debugName = name;

            auto buffer = context->createBuffer(desc);
            if (buffer) {
                // 类似顶点缓冲区的上传逻辑
            }
            return std::unique_ptr<RHIBuffer>(buffer);
        }

        template<typename T>
        static std::unique_ptr<RHIBuffer> createUniformBuffer(
            IRHIContext* context,
            const std::string& name = "") {

            BufferDesc desc;
            desc.size = sizeof(T);
            desc.type = BufferType::Uniform;
            desc.memoryType = MemoryType::CPU_To_GPU;
            desc.allowUpdate = true;
            desc.persistentMapped = true;
            desc.debugName = name;

            return std::unique_ptr<RHIBuffer>(context->createBuffer(desc));
        }

        // 管线状态预设
        static GraphicsPipelineDesc createDefaultOpaquePipeline();
        static GraphicsPipelineDesc createDefaultAlphaBlendPipeline();
        static GraphicsPipelineDesc createDefaultWireframePipeline();
        static GraphicsPipelineDesc createDefaultSkyboxPipeline();
        static GraphicsPipelineDesc createDefaultPostProcessPipeline();

        static RasterizerState createDefaultRasterizerState();
        static DepthStencilState createDefaultDepthStencilState();
        static ColorBlendState createDefaultBlendState();

        // 调试工具
        static void setDebugColor(float* color, uint32_t resourceId);
        static std::string resourceTypeToString(IResource* resource);
        static std::string formatToString(Format format);
        static std::string shaderStageToString(ShaderStage stage);

        // 性能分析
        static void beginGPUTimestamp(IRHIContext* context, const std::string& name);
        static void endGPUTimestamp(IRHIContext* context, const std::string& name);
        static double getGPUTimestampDuration(IRHIContext* context, const std::string& name);

        // 验证和检查
        static bool validatePipelineState(const GraphicsPipelineDesc& desc);
        static bool validateResourceState(IResource* resource, const std::string& operation);
        static bool checkMemoryLeaks(IRHIContext* context);

        // 转换函数
        static Format fromVulkanFormat(void* vkFormat);
        static void* toVulkanFormat(Format format);

        static Format fromDXGIFormat(uint32_t dxgiFormat);
        static uint32_t toDXGIFormat(Format format);

        static Format fromMetalFormat(void* mtlFormat);
        static void* toMetalFormat(Format format);

        // 数学工具
        static glm::mat4 createPerspectiveMatrix(float fov, float aspect, float near, float far);
        static glm::mat4 createOrthographicMatrix(float left, float right, float bottom, float top, float near, float far);
        static glm::mat4 createViewMatrix(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up);

        // 随机工具
        static Color randomColor();
        static float randomFloat(float min = 0.0f, float max = 1.0f);
        static uint32_t randomUint(uint32_t min = 0, uint32_t max = UINT32_MAX);
    };
}