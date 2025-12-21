#pragma once
#include "IRHI_STRUCT.hpp"
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>

namespace StarryEngine::RHI {
    class IRHI {
    public:
        virtual ~IRHI() = default;

        // === 初始化/清理 ===
        virtual bool initialize(void* windowHandle, uint32_t width, uint32_t height) = 0;
        virtual void shutdown() = 0;

        // === 帧管理 ===
        virtual bool beginFrame() = 0;
        virtual void endFrame() = 0;
        virtual void present() = 0;
        virtual void waitIdle() = 0;

        // === 资源创建 ===
        virtual BufferHandle createBuffer(const BufferDesc& desc) = 0;
        virtual void destroyBuffer(BufferHandle handle) = 0;

        virtual TextureHandle createTexture(const TextureDesc& desc) = 0;
        virtual void destroyTexture(TextureHandle handle) = 0;

        virtual SamplerHandle createSampler(const SamplerDesc& desc) = 0;
        virtual void destroySampler(SamplerHandle handle) = 0;

        virtual ShaderModuleHandle createShaderModule(const ShaderModuleDesc& desc) = 0;
        virtual void destroyShaderModule(ShaderModuleHandle handle) = 0;

        virtual PipelineLayoutHandle createPipelineLayout(const PipelineLayoutDesc& desc) = 0;
        virtual void destroyPipelineLayout(PipelineLayoutHandle handle) = 0;

        virtual PipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
        virtual PipelineHandle createComputePipeline(const ComputePipelineDesc& desc) = 0;
        virtual void destroyPipeline(PipelineHandle handle) = 0;

        // === 资源更新 ===
        virtual void updateBuffer(BufferHandle handle, const void* data,
            uint64_t offset, uint64_t size) = 0;
        virtual void* mapBuffer(BufferHandle handle, uint64_t offset = 0, uint64_t size = 0) = 0;
        virtual void unmapBuffer(BufferHandle handle) = 0;

        virtual void generateTextureMips(TextureHandle handle) = 0;

        // === 命令录制 ===
        virtual void beginCommandList() = 0;
        virtual void endCommandList() = 0;

        virtual void setViewport(const Viewport& viewport) = 0;
        virtual void setScissor(const Rect& scissor) = 0;

        virtual void clearColor(TextureHandle target, const float color[4]) = 0;
        virtual void clearDepthStencil(TextureHandle target, float depth, uint8_t stencil) = 0;

        virtual void setPipeline(PipelineHandle pipeline) = 0;
        virtual void setVertexBuffer(BufferHandle buffer, uint32_t slot = 0, uint64_t offset = 0) = 0;
        virtual void setIndexBuffer(BufferHandle buffer, uint64_t offset = 0, bool use32Bit = false) = 0;
        virtual void setConstantBuffer(uint32_t set, uint32_t binding, BufferHandle buffer) = 0;
        virtual void setTexture(uint32_t set, uint32_t binding, TextureHandle texture) = 0;
        virtual void setSampler(uint32_t set, uint32_t binding, SamplerHandle sampler) = 0;

        virtual void setRenderTargets(const std::vector<TextureHandle>& colorTargets,
            TextureHandle depthStencil = TextureHandle{}) = 0;

        virtual void draw(uint32_t vertexCount, uint32_t instanceCount = 1,
            uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
        virtual void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
            uint32_t firstIndex = 0, int32_t vertexOffset = 0,
            uint32_t firstInstance = 0) = 0;

        virtual void dispatch(uint32_t groupCountX, uint32_t groupCountY = 1,
            uint32_t groupCountZ = 1) = 0;

        virtual void copyBuffer(BufferHandle src, BufferHandle dst, uint64_t size,
            uint64_t srcOffset = 0, uint64_t dstOffset = 0) = 0;
        virtual void copyBufferToTexture(BufferHandle src, TextureHandle dst) = 0;
        virtual void copyTextureToBuffer(TextureHandle src, BufferHandle dst) = 0;

        // === 查询 ===
        virtual uint32_t getCurrentFrameIndex() const = 0;
        virtual uint32_t getCurrentImageIndex() const = 0;

        virtual uint32_t getMaxTextureSize() const = 0;
        virtual uint32_t getMaxAnisotropy() const = 0;
        virtual bool supportsFormat(Format format) const = 0;
        virtual bool supportsFeature(const std::string& feature) const = 0;

        // === 统计 ===
        virtual uint64_t getTotalAllocatedMemory() const = 0;
        virtual uint64_t getPeakMemoryUsage() const = 0;
        virtual uint64_t getDrawCallCount() const = 0;
        virtual uint64_t getTriangleCount() const = 0;
        virtual double getFrameTime() const = 0;
        virtual double getGPUTime() const = 0;
        virtual uint32_t getFPS() const = 0;

        // === 调试 ===
        virtual void setDebugName(BufferHandle handle, const char* name) = 0;
        virtual void setDebugName(TextureHandle handle, const char* name) = 0;
        virtual void setDebugName(SamplerHandle handle, const char* name) = 0;
        virtual void setDebugName(PipelineHandle handle, const char* name) = 0;

        virtual void beginDebugLabel(const char* name, const float color[4] = nullptr) = 0;
        virtual void endDebugLabel() = 0;
        virtual void insertDebugLabel(const char* name, const float color[4] = nullptr) = 0;

        // === 获取原生句柄 ===
        virtual void* getNativeDevice() const = 0;
        virtual void* getNativeCommandQueue() const = 0;
    };

}