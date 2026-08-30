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
#include <renderer/graph/RenderNode.hpp>
#include <renderer/graph/GraphNode.hpp>
#include <renderer/graph/ParallelRecording.hpp>

namespace StarryEngine::RenderGraph {
    struct TexturePassInfo {
        int32_t firstUserIndex = -1;
        int32_t lastUserIndex  = -1;
        int32_t firstWriterIndex = -1;
        int32_t lastWriterIndex = -1;
        int32_t firstReaderIndex = -1;
    };

    class RenderGraph {
    public:
        explicit RenderGraph(std::shared_ptr<RHI::IRHI> rhi);
        ~RenderGraph();

        RenderGraph(const RenderGraph&) = delete;
        RenderGraph& operator=(const RenderGraph&) = delete;

        void setSwapchainImageCount(uint32_t count);

        TextureId createVirtualTexture(const RHI::TextureDesc& desc, const std::string& name = "");

        // 导入外部纹理
        TextureId importExternalTexture(RHI::TextureHandle externalHandle,
            const std::vector<void*>& imageViews,
            const RHI::TextureDesc& desc,
            RHI::ImageLayout initialLayout,
            const std::string& name = "");

        BufferId createVirtualBuffer(const RHI::BufferDesc& desc, const std::string& name = "");

        GraphNode* addGraphicsPassNode(const std::string& name);

        GraphNode* addGraphicsPassNodeShared(const std::string& name, const std::string& dedupKey);

        GraphNode* addComputePassNode(const std::string& name);

        // 按名字查找节点（cullUnusedPasses 可能删除节点 → 持有旧指针的 pass 需重新解析）
        GraphNode* findNode(const std::string& name);

        bool compile();

        void execute(RHI::RHICommandEncoder* encoder, const RenderContext& context, uint32_t frameIndex,
            uint32_t frameSlot,
            const ParallelRecordingContext* parallel = nullptr);

        RHI::TextureHandle getPhysicalTextureHandle(TextureId id) const;
        RHI::BufferHandle getPhysicalBuffer(BufferId id) const;

        const std::vector<GraphNode*>& getSortedPasses() const { return m_sortedPasses; }

        TextureId getTextureId(const std::string& name) const;
        BufferId getBufferId(const std::string& name) const;

        bool isDepthOnlyFormat(RHI::Format format) {
            return format == RHI::Format::D16_UNorm || format == RHI::Format::D32_Float;
        }

        bool isDepthStencilFormat(RHI::Format format) {
            return format == RHI::Format::D24_UNorm_S8_UInt || format == RHI::Format::D32_Float_S8_UInt;
        }

        uint32_t getAspectMask(RHI::Format format) {
            if (isDepthOnlyFormat(format)) {
                return static_cast<uint32_t>(RHI::ImageAspect::Depth);
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

        // ── 图分析 / 编译 ──
        void dependencyAnalysis();
        void cullUnusedPasses();
        void exportDot(const std::string& filepath) const;
        void createFrameBuffer();
        // 分析纹理使用（first/last user/writer/reader + 跨 pass 输入依赖）
        void analyzeTextureUsage(std::unordered_map<TextureId, TexturePassInfo>& texPassInfo);
        // 物理资源分配（纹理/缓冲）+ 附件 loadOp 推断 + pass 编译
        void createPhysicalResources(const std::unordered_map<TextureId, TexturePassInfo>& texPassInfo);
        // 动态渲染：收集每 pass 的颜色/深度附件视图
        void collectDynamicViews();
        // 布局转换分析（首用/写读间 barrier 生成）
        void buildLayoutTransitions(const std::unordered_map<TextureId, TexturePassInfo>& texPassInfo);

        // ── execute() 拆分 ──
        // Phase 1（并行路径）：预分配 secondary + 逐 (pass×subpass) 提交录制 job
        void recordSecondariesParallel(RHI::RHICommandEncoder* encoder, const RenderContext& context,
            uint32_t frameIndex, uint32_t frameSlot, const ParallelRecordingContext* parallel,
            std::vector<std::vector<void*>>& secondaries);
        // 单 pass 主缓冲执行（串行/并行双模统一入口）
        void recordPassOnPrimary(RHI::RHICommandEncoder* encoder, const RenderContext& context,
            uint32_t frameIndex, uint32_t frameSlot, size_t passIndex,
            const std::vector<std::vector<void*>>& secondaries);
        // 单个布局转换 barrier（apply 到 encoder）
        void applyLayoutTransition(RHI::RHICommandEncoder* encoder,
            const LayoutTransition& trans,
            std::unordered_map<TextureId, RHI::ImageLayout>& currentLayouts);
        // 取某 pass 的 framebuffer（传统模式）或空 handle（compute/动态）
        RHI::FramebufferHandle getFramebufferForPass(size_t passIndex, uint32_t frameIndex) const;

        // 布局/阶段/访问转换辅助
        std::pair<RHI::PipelineStage, RHI::AccessFlag> getStageAccessFromLayout(RHI::ImageLayout layout);
        std::pair<RHI::PipelineStage, RHI::AccessFlag> getReadStageAccess(RHI::ImageLayout layout, bool computeReader);

        // 动态渲染：pass 附件视图访问（[pass] 索引）
        const std::vector<void*>& getAttachmentViewsForPass(size_t passIndex, uint32_t imageIndex) const;
        void* getDepthAttachmentViewForPass(size_t passIndex, uint32_t imageIndex) const;

    private:
        std::shared_ptr<RHI::IRHI> m_rhi;
        bool m_useDynamicRendering = false;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        uint32_t m_swapchainImageCount = 0;

        std::vector<VirtualTexture> m_virtualTextures;
        std::unordered_map<std::string, TextureId> m_nameToTextureId;
        // 纹理格式按 id 索引（id 从 1 连续递增，vector 下标 = id-1），避免按 texId 线性 find_if
        std::vector<RHI::Format> m_textureFormats;
        uint32_t m_nextTextureId = 1;

        // 按 texId 取纹理格式（id 有效且已填充时返回格式，否则 Undefined）
        RHI::Format getTextureFormat(TextureId texId) const {
            uint32_t idx = texId.id();
            if (idx == 0 || idx > m_textureFormats.size()) return RHI::Format::Undefined;
            return m_textureFormats[idx - 1];
        }

        std::vector<VirtualBuffer> m_virtualBuffers;
        std::unordered_map<std::string, BufferId> m_nameToBufferId;
        uint32_t m_nextBufferId = 1;

        std::vector<std::unique_ptr<GraphNode>> m_passes;
        // 每 pass 的动态渲染附件视图：colorViews[交换链图像索引][附件]，depthView 为深度附件（无深度=nullptr）
        struct PassAttachmentViews {
            std::vector<std::vector<void*>> colorViews;
            void* depthView = nullptr;
        };
        std::vector<PassAttachmentViews> m_perPassViews;

        // 共享图形 pass 节点注册表：dedupKey → 节点（addGraphicsPassNodeShared 用）
        std::unordered_map<std::string, GraphNode*> m_sharedGraphicsNodes;

        std::vector<GraphNode*> m_sortedPasses;
        std::unordered_map<TextureId, PhysicalTextureInfo> m_textureMap;  // 虚拟 -> 物理信息
        std::unordered_map<BufferId, RHI::BufferHandle> m_bufferMap;

        std::vector<LayoutTransition> m_layoutTransitions;
        // 每个 Pass 的帧缓冲（[passIndex][imageIndex]）
        std::vector<std::vector<RHI::FramebufferHandle>> m_perPassFramebuffers;
    };

} // namespace StarryEngine::RenderGraph