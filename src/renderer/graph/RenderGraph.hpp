#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include "../interface/RHI_ENUMS.hpp"
#include "../interface/RHI_STRUCTS_DESC.hpp"
#include "../interface/RHI_HANDLES_SYSTEM.hpp"
#include "../backend/vulkan/VulkanRHI.hpp"
#include "PassNode.hpp"

namespace StarryEngine::RenderGraph {
    struct TexturePassInfo {
        int32_t lastWriterIndex = -1;       
        int32_t firstReaderIndex = -1;       
        RHI::PipelineStageFlags writeStage = static_cast<RHI::PipelineStageFlags>(0);
        RHI::AccessFlags writeAccess = static_cast<RHI::AccessFlags>(0);
        RHI::PipelineStageFlags readStage = static_cast<RHI::PipelineStageFlags>(0);
        RHI::AccessFlags readAccess = static_cast<RHI::AccessFlags>(0);
    }; 

    class RenderGraph {
    public:
        explicit RenderGraph(std::shared_ptr<RHI::IRHI> rhi);
        ~RenderGraph();

        RenderGraph(const RenderGraph&) = delete;
        RenderGraph& operator=(const RenderGraph&) = delete;

        // 设置交换链图像数量（必须在 compile 前调用）
        void setSwapchainImageCount(uint32_t count);

        RHI::TextureDesc createBaseTextureDesc(
            uint32_t width, uint32_t height,
            RHI::Format format = RHI::Format::RGBA8_UNorm,
            bool allowDepthStencil = true,
            bool allowRenderTarget = true,
            bool allowInputAttachment = true,
            RHI::TextureType type = RHI::TextureType::Texture2D);

        RHI::GraphicsPipelineDesc createBasePipelineDesc(
            RHI::ShaderHandle vertexShaderHandle,
            RHI::ShaderHandle fragmentShaderHandle,
            RHI::VertexInputState vertexInput,
            RHI::PipelineLayoutHandle pipelineLayoutHandle,
            uint32_t width, uint32_t height,
            uint32_t rasterizationSamples = 1,
            float lineWidth = 1.0f,
            RHI::PrimitiveTopology topology = RHI::PrimitiveTopology::TriangleList,
            RHI::CullMode cullMode = RHI::CullMode::None,
            bool depthTestEnable = false,
            bool depthWriteEnable = false,
            RHI::CompareOp depthCompareOp = RHI::CompareOp::Less,
            std::vector<RHI::BlendAttachmentState> attachments = { RHI::BlendAttachmentState{} },
            std::vector<RHI::DynamicState> dynamicStates = { RHI::DynamicState::Viewport, RHI::DynamicState::Scissor }
        );

        // 创建虚拟纹理资源
        TextureId createVirtualTexture(const RHI::TextureDesc& desc, const std::string& name = "");

        // 导入外部纹理（支持多视图，如交换链）
        TextureId importExternalTexture(RHI::TextureHandle externalHandle,
            const std::vector<void*>& imageViews,
            const RHI::TextureDesc& desc,
            RHI::ImageLayout initialLayout,
            const std::string& name = "");

        // 创建虚拟缓冲区
        BufferId createVirtualBuffer(const RHI::BufferDesc& desc, const std::string& name = "");

        // 添加 PassNode
        PassNode* addPassNode(const std::string& name);

        // 编译整个图（自动创建帧缓冲）
        bool compile();

        // 执行一帧
        void execute(uint32_t frameIndex, RHI::RHICommandEncoder* encoder);

        // 获取物理资源（调试用）
        RHI::TextureHandle getPhysicalTextureHandle(TextureId id) const;
        RHI::BufferHandle getPhysicalBuffer(BufferId id) const;

        const std::vector<PassNode*>& getSortedPasses() const { return m_sortedPasses; }

        void addDependency(PassNode* from, PassNode* to);

    private:
        struct VirtualTexture {
            TextureId id;
            RHI::TextureDesc desc;
            std::string name;
            bool imported;
            RHI::TextureHandle externalHandle;
            std::vector<void*> externalViews; 
            RHI::ImageLayout initialLayout;
        };

        struct VirtualBuffer {
            BufferId id;
            RHI::BufferDesc desc;
            std::string name;
            bool imported = false;
            RHI::BufferHandle externalHandle;
        };

        std::vector<uint32_t> topologicalSort(const std::vector<std::vector<uint32_t>>& adj) const;

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;

        // 交换链图像数量
        uint32_t m_swapchainImageCount = 0;

        // 虚拟资源存储
        std::vector<VirtualTexture> m_virtualTextures;
        std::unordered_map<std::string, TextureId> m_nameToTextureId;
        uint32_t m_nextTextureId = 1;

        std::vector<VirtualBuffer> m_virtualBuffers;
        std::unordered_map<std::string, BufferId> m_nameToBufferId;
        uint32_t m_nextBufferId = 1;

        // Pass 存储
        std::vector<std::unique_ptr<PassNode>> m_passes;
        std::vector<std::pair<uint32_t, uint32_t>> m_manualDependencies;

        // 编译后数据
        std::vector<PassNode*> m_sortedPasses;
        std::unordered_map<TextureId, PhysicalTextureInfo> m_textureMap;  // 虚拟 -> 物理信息
        std::unordered_map<BufferId, RHI::BufferHandle> m_bufferMap;

        // 每个 Pass 的帧缓冲（[passIndex][imageIndex]）
        std::vector<std::vector<RHI::FramebufferHandle>> m_perPassFramebuffers;
    };

} // namespace StarryEngine::RenderGraph