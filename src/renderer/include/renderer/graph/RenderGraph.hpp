#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <renderer/interface/RHIEnums.hpp>
#include <renderer/interface/RHIStructs.hpp>
#include <renderer/interface/RHIHandles.hpp>
#include <renderer/interface/vulkan/VulkanRHI.hpp>
#include <renderer/graph/PassNode.hpp>
#include <renderer/graph/ParallelRecording.hpp>

namespace StarryEngine::RenderGraph {
    struct TexturePassInfo {
        int32_t firstUserIndex = -1;
        int32_t lastUserIndex  = -1;
        int32_t firstWriterIndex = -1;
        int32_t lastWriterIndex = -1;
        int32_t firstReaderIndex = -1;
        RHI::PipelineStage writeStage = RHI::PipelineStage::None;
        RHI::AccessFlag writeAccess = RHI::AccessFlag::None;
        RHI::PipelineStage readStage = RHI::PipelineStage::None;
        RHI::AccessFlag readAccess = RHI::AccessFlag::None;
    };

    class RenderGraph {
    public:
        explicit RenderGraph(std::shared_ptr<RHI::IRHI> rhi);
        ~RenderGraph();

        RenderGraph(const RenderGraph&) = delete;
        RenderGraph& operator=(const RenderGraph&) = delete;

        void setSwapchainImageCount(uint32_t count);

        RHI::TextureDesc createBaseTextureDesc(
            uint32_t width, uint32_t height,
            RHI::Format format = RHI::Format::RGBA8_UNorm,
            bool allowDepthStencil = true,
            bool allowRenderTarget = true,
            bool allowInputAttachment = true,
            RHI::TextureType type = RHI::TextureType::Texture2D);

        TextureId createVirtualTexture(const RHI::TextureDesc& desc, const std::string& name = "");

        // 导入外部纹理
        TextureId importExternalTexture(RHI::TextureHandle externalHandle,
            const std::vector<void*>& imageViews,
            const RHI::TextureDesc& desc,
            RHI::ImageLayout initialLayout,
            const std::string& name = "");

        BufferId createVirtualBuffer(const RHI::BufferDesc& desc, const std::string& name = "");

        PassNode* addGraphicsPassNode(const std::string& name);

        PassNode* addGraphicsPassNodeShared(const std::string& name, const std::string& dedupKey);

        PassNode* addComputePassNode(const std::string& name);

        // 按名字查找节点（cullUnusedPasses 可能删除节点 → 持有旧指针的 pass 需重新解析）
        PassNode* findNode(const std::string& name);

        void dependencyAnalysis();
        void cullUnusedPasses();
        void exportDot(const std::string& filepath) const;

        bool compile();

        void createFrameBuffer();

        void execute(RHI::RHICommandEncoder* encoder, const RenderContext& context, uint32_t frameIndex,
            uint32_t frameSlot,
            const ParallelRecordingContext* parallel = nullptr);

        RHI::TextureHandle getPhysicalTextureHandle(TextureId id) const;
        RHI::BufferHandle getPhysicalBuffer(BufferId id) const;

        const std::vector<PassNode*>& getSortedPasses() const { return m_sortedPasses; }

        std::vector<std::unique_ptr<PassNode>>& getPasses() { return m_passes; }
        const std::vector<std::unique_ptr<PassNode>>& getPasses() const { return m_passes; }
        size_t getPassCount() const { return m_passes.size(); }

        TextureId getTextureId(const std::string& name) const;
        BufferId getBufferId(const std::string& name) const;

        const std::vector<RHI::FramebufferHandle>& getFramebuffersForPass(size_t passIndex) const;
        std::pair<RHI::PipelineStage, RHI::AccessFlag> getStageAccessFromLayout(RHI::ImageLayout layout);
        std::pair<RHI::PipelineStage, RHI::AccessFlag> getReadStageAccess(RHI::ImageLayout layout, bool computeReader);

        bool isDepthOnlyFormat(RHI::Format format) {
            return format == RHI::Format::D16_UNorm || format == RHI::Format::D32_Float;
        }

        bool isStencilOnlyFormat(RHI::Format format) {
            return false;
        }

        bool isDepthStencilFormat(RHI::Format format) {
            return format == RHI::Format::D24_UNorm_S8_UInt || format == RHI::Format::D32_Float_S8_UInt;
        }

        uint32_t getAspectMask(RHI::Format format) {
            if (isDepthOnlyFormat(format)) {
                return static_cast<uint32_t>(RHI::ImageAspect::Depth);
            }
            else if (isStencilOnlyFormat(format)) {
                return static_cast<uint32_t>(RHI::ImageAspect::Stencil);
            }
            else if (isDepthStencilFormat(format)) {
                return static_cast<uint32_t>(RHI::ImageAspect::Depth) |
                    static_cast<uint32_t>(RHI::ImageAspect::Stencil);
            }
            else {
                return static_cast<uint32_t>(RHI::ImageAspect::Color);
            }
        }

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

        void performMemoryAliasing(const std::unordered_map<TextureId, TexturePassInfo>& texPassInfo);

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

        // 共享图形 pass 节点注册表：dedupKey → 节点（addGraphicsPassNodeShared 用）
        std::unordered_map<std::string, PassNode*> m_sharedGraphicsNodes;

        std::vector<PassNode*> m_sortedPasses;
        std::unordered_map<TextureId, PhysicalTextureInfo> m_textureMap;  // 虚拟 -> 物理信息
        std::unordered_map<BufferId, RHI::BufferHandle> m_bufferMap;

        std::vector<LayoutTransition> m_layoutTransitions;
        // 每个 Pass 的帧缓冲（[passIndex][imageIndex]）
        std::vector<std::vector<RHI::FramebufferHandle>> m_perPassFramebuffers;

        std::unordered_map<TextureId, std::string> m_textureNames;
    };

} // namespace StarryEngine::RenderGraph