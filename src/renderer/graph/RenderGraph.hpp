#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../interface/RHI_ENUMS.hpp"
#include "../interface/RHI_STRUCTS_DESC.hpp"
#include "../interface/RHI_HANDLES_SYSTEM.hpp"
#include "../interface/RHI_STRUCTS_RESOURCE.hpp" 
#include "../backend/VulkanRHI.hpp"
#include "PassNode.hpp"

namespace StarryEngine::RenderGraph {
    class RenderGraph {
    public:
        explicit RenderGraph(std::shared_ptr<VulkanRHI> rhi);
        ~RenderGraph();

        // 禁止拷贝
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

    private:
        struct VirtualTexture {
            TextureId id;                      
            RHI::TextureDesc desc;
            std::string name;
            bool imported;
            RHI::TextureHandle externalHandle;
            RHI::ImageLayout initialLayout;
            RHI::TextureHandle physicalHandle;
        };

        struct VirtualBuffer {
            BufferId id;
            RHI::BufferDesc desc;
            std::string name;
            bool imported = false;
            RHI::BufferHandle externalHandle;
            RHI::BufferHandle physicalHandle;
        };

        // 依赖分析时记录每个资源的读写 Pass
        struct ResourceUsage {
            std::set<uint32_t> readingPasses;   // 读取该资源的 Pass 索引
            std::set<uint32_t> writingPasses;   // 写入该资源的 Pass 索引
        };

        // 拓扑排序辅助
        std::vector<uint32_t> topologicalSort(const std::vector<std::vector<uint32_t>>& adj) const;

        std::shared_ptr<VulkanRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;

        // 虚拟资源存储
        std::vector<VirtualTexture> m_virtualTextures;
        std::unordered_map<std::string, TextureId> m_nameToTextureId;  // 名称到虚拟纹理 ID
        uint32_t m_nextTextureId = 1;

        std::vector<VirtualBuffer> m_virtualBuffers;
        std::unordered_map<std::string, BufferId> m_nameToBufferId;
        uint32_t m_nextBufferId = 1;

        // Pass 存储
        std::vector<std::unique_ptr<PassNode>> m_passes;

        // 编译后数据
        std::vector<PassNode*> m_sortedPasses;               // 拓扑排序后的 Pass 执行顺序
        std::unordered_map<TextureId, RHI::TextureHandle> m_textureMap; // 虚拟 -> 物理
        std::unordered_map<BufferId, RHI::BufferHandle> m_bufferMap;

        std::vector<RHI::FramebufferHandle> m_passFramebuffers;
    };

} // namespace StarryEngine::RenderGraph