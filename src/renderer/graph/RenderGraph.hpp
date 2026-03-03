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
#include "../backend/VulkanRHI.hpp"
#include "PassNode.hpp"

namespace StarryEngine::RenderGraph {

    class RenderGraph {
    public:
        explicit RenderGraph(std::shared_ptr<VulkanRHI> rhi);
        ~RenderGraph();

        RenderGraph(const RenderGraph&) = delete;
        RenderGraph& operator=(const RenderGraph&) = delete;

        // 创建虚拟纹理资源
        TextureId createVirtualTexture(const RHI::TextureDesc& desc, const std::string& name = "");

        // 导入外部纹理（如交换链图像）
        TextureId importExternalTexture(RHI::TextureHandle externalHandle,
            const RHI::TextureDesc& desc,
            RHI::ImageLayout initialLayout,
            const std::string& name = "");

        // 创建虚拟缓冲区
        BufferId createVirtualBuffer(const RHI::BufferDesc& desc, const std::string& name = "");

        // 添加 PassNode
        PassNode* addPassNode(const std::string& name);

        // 编译整个图
        bool compile();

        // 执行一帧
        void execute(uint32_t frameIndex, RHI::RHICommandEncoder* encoder);

        // 获取物理资源（调试用）
        RHI::TextureHandle getPhysicalTexture(TextureId id) const;
        RHI::BufferHandle getPhysicalBuffer(BufferId id) const;

        void setPassFramebuffers(const std::vector<RHI::FramebufferHandle>& framebuffers) {
            m_passFramebuffers = framebuffers;
        }

        const std::vector<PassNode*>& getSortedPasses() const { return m_sortedPasses; }
        std::vector<RHI::FramebufferHandle> getPassFramebuffers() { return m_passFramebuffers; }
        void addDependency(PassNode* from, PassNode* to) {
            // 查找索引
            uint32_t srcIdx = UINT32_MAX, dstIdx = UINT32_MAX;
            for (uint32_t i = 0; i < m_passes.size(); ++i) {
                if (m_passes[i].get() == from) srcIdx = i;
                if (m_passes[i].get() == to) dstIdx = i;
            }
            if (srcIdx != UINT32_MAX && dstIdx != UINT32_MAX) {
                m_manualDependencies.push_back({ srcIdx, dstIdx });
            }
        }
    private:
        struct VirtualTexture {
            TextureId id;
            RHI::TextureDesc desc;
            std::string name;
            bool imported;
            RHI::TextureHandle externalHandle;
            RHI::ImageLayout initialLayout;
        };

        struct VirtualBuffer {
            BufferId id;
            RHI::BufferDesc desc;
            std::string name;
            bool imported = false;
            RHI::BufferHandle externalHandle;
        };

        // 拓扑排序辅助
        std::vector<uint32_t> topologicalSort(const std::vector<std::vector<uint32_t>>& adj) const;

        std::shared_ptr<VulkanRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;

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
        std::vector<PassNode*> m_sortedPasses;                       // 拓扑排序后的 Pass 执行顺序
        std::unordered_map<TextureId, RHI::TextureHandle> m_textureMap; // 虚拟 -> 物理
        std::unordered_map<BufferId, RHI::BufferHandle> m_bufferMap;

        std::vector<RHI::FramebufferHandle> m_passFramebuffers;
    };

} // namespace StarryEngine::RenderGraph