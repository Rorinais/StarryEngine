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

        TextureId createVirtualTexture(const RHI::TextureDesc& desc, const std::string& name = "");

        // 导入外部纹理（支持多视图，如交换链）
        TextureId importExternalTexture(RHI::TextureHandle externalHandle,
            const std::vector<void*>& imageViews,
            const RHI::TextureDesc& desc,
            RHI::ImageLayout initialLayout,
            const std::string& name = "");

        BufferId createVirtualBuffer(const RHI::BufferDesc& desc, const std::string& name = "");

        PassNode* addPassNode(const std::string& name);

        void dependencyAnalysis();

        bool compile();

        void createFrameBuffer();

        void execute(RHI::RHICommandEncoder* encoder, const RenderContext& context,uint32_t frameIndex);

        RHI::TextureHandle getPhysicalTextureHandle(TextureId id) const;
        RHI::BufferHandle getPhysicalBuffer(BufferId id) const;

        const std::vector<PassNode*>& getSortedPasses() const { return m_sortedPasses; }

        TextureId getTextureId(const std::string& name) const;

        const std::vector<RHI::FramebufferHandle>& getFramebuffersForPass(size_t passIndex) const;

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

    private:
        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        uint32_t m_swapchainImageCount = 0;

        std::vector<VirtualTexture> m_virtualTextures;
        std::unordered_map<std::string, TextureId> m_nameToTextureId;
        uint32_t m_nextTextureId = 1;

        std::vector<VirtualBuffer> m_virtualBuffers;
        std::unordered_map<std::string, BufferId> m_nameToBufferId;
        uint32_t m_nextBufferId = 1;

        std::vector<std::unique_ptr<PassNode>> m_passes;

        std::vector<PassNode*> m_sortedPasses;
        std::unordered_map<TextureId, PhysicalTextureInfo> m_textureMap;  // 虚拟 -> 物理信息
        std::unordered_map<BufferId, RHI::BufferHandle> m_bufferMap;

        std::vector<LayoutTransition> m_layoutTransitions;
        // 每个 Pass 的帧缓冲（[passIndex][imageIndex]）
        std::vector<std::vector<RHI::FramebufferHandle>> m_perPassFramebuffers;

        std::unordered_map<TextureId, std::string> m_textureNames;
    };

} // namespace StarryEngine::RenderGraph