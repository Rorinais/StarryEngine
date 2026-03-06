#include "RenderGraph.hpp"
#include <queue>
#include <stack>
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace StarryEngine::RenderGraph {

    RenderGraph::RenderGraph(std::shared_ptr<VulkanRHI> rhi)
        : m_rhi(rhi), m_resMgr(rhi->getResourceManager()) {
    }

    RenderGraph::~RenderGraph() {
        if (m_rhi) {
            m_rhi->waitIdle();
        }

        // 销毁所有帧缓冲
        for (auto& passFbs : m_perPassFramebuffers) {
            for (auto fb : passFbs) {
                if (fb.isValid()) m_resMgr->destroy(fb);
            }
        }

        // 销毁物理纹理
        for (auto& [id, info] : m_textureMap) {
            if (info.handle.isValid()) m_resMgr->destroy(info.handle);
        }

        // 销毁物理缓冲区
        for (auto& [id, handle] : m_bufferMap) {
            if (handle.isValid()) m_resMgr->destroy(handle);
        }
    }

    void RenderGraph::setSwapchainImageCount(uint32_t count) {
        m_swapchainImageCount = count;
    }

    TextureId RenderGraph::createVirtualTexture(const RHI::TextureDesc& desc, const std::string& name) {
        if (!name.empty() && m_nameToTextureId.find(name) != m_nameToTextureId.end()) {
            throw std::runtime_error("Texture name already exists: " + name);
        }
        TextureId id = TextureId::Create(m_nextTextureId++, 1);
        VirtualTexture vt{ id, desc, name, false, RHI::TextureHandle::Null(), {}, RHI::ImageLayout::Undefined };
        m_virtualTextures.push_back(vt);
        if (!name.empty()) {
            m_nameToTextureId[name] = id;
        }
        return id;
    }

    TextureId RenderGraph::importExternalTexture(
        RHI::TextureHandle externalHandle,
        const std::vector<void*>& imageViews,
        const RHI::TextureDesc& desc,
        RHI::ImageLayout initialLayout,
        const std::string& name) {
        if (!name.empty() && m_nameToTextureId.find(name) != m_nameToTextureId.end()) {
            throw std::runtime_error("Texture name already exists: " + name);
        }
        TextureId id = TextureId::Create(m_nextTextureId++, 1);
        VirtualTexture vt{ id, desc, name, true, externalHandle, imageViews, initialLayout };
        m_virtualTextures.push_back(vt);
        if (!name.empty()) {
            m_nameToTextureId[name] = id;
        }
        return id;
    }

    BufferId RenderGraph::createVirtualBuffer(const RHI::BufferDesc& desc, const std::string& name) {
        if (!name.empty() && m_nameToBufferId.find(name) != m_nameToBufferId.end()) {
            throw std::runtime_error("Buffer name already exists: " + name);
        }
        BufferId id = BufferId::Create(m_nextBufferId++, 1);
        VirtualBuffer vb{ id, desc, name, false, RHI::BufferHandle::Null() };
        m_virtualBuffers.push_back(vb);
        if (!name.empty()) {
            m_nameToBufferId[name] = id;
        }
        return id;
    }

    PassNode* RenderGraph::addPassNode(const std::string& name) {
        auto pass = std::make_unique<PassNode>(name);
        PassNode* ptr = pass.get();
        m_passes.push_back(std::move(pass));
        return ptr;
    }

    std::vector<uint32_t> RenderGraph::topologicalSort(const std::vector<std::vector<uint32_t>>& adj) const {
        size_t n = adj.size();
        std::vector<uint32_t> inDegree(n, 0);
        for (const auto& edges : adj) {
            for (uint32_t v : edges) {
                inDegree[v]++;
            }
        }

        std::queue<uint32_t> q;
        for (uint32_t i = 0; i < n; ++i) {
            if (inDegree[i] == 0) q.push(i);
        }

        std::vector<uint32_t> order;
        while (!q.empty()) {
            uint32_t u = q.front(); q.pop();
            order.push_back(u);
            for (uint32_t v : adj[u]) {
                if (--inDegree[v] == 0) q.push(v);
            }
        }

        if (order.size() != n) {
            throw std::runtime_error("RenderGraph: Cyclic dependency detected!");
        }
        return order;
    }

    bool RenderGraph::compile() {
        if (m_passes.empty()) {
            throw std::runtime_error("No passes to compile.");
        }

        for (auto& pass : m_passes) {
            pass->collectResourceUsage(); 
        }

        size_t passCount = m_passes.size();
        std::vector<std::vector<uint32_t>> adj(passCount);

        std::unordered_map<TextureId, std::set<uint32_t>> texReaders, texWriters;
        std::unordered_map<BufferId, std::set<uint32_t>> bufReaders, bufWriters;

        for (uint32_t i = 0; i < passCount; ++i) {
            for (auto tex : m_passes[i]->getReadTextures()) texReaders[tex].insert(i);
            for (auto tex : m_passes[i]->getWriteTextures()) texWriters[tex].insert(i);
            for (auto buf : m_passes[i]->getReadBuffers()) bufReaders[buf].insert(i);
            for (auto buf : m_passes[i]->getWriteBuffers()) bufWriters[buf].insert(i);
        }

        auto addDependency = [&](uint32_t src, uint32_t dst) {
            adj[src].push_back(dst);
            };

        for (const auto& [tex, writers] : texWriters) {
            auto& readers = texReaders[tex];
            for (uint32_t w : writers) {
                for (uint32_t r : readers) {
                    if (w != r && writers.find(r) == writers.end()) {
                        addDependency(w, r);
                    }
                }
            }
            std::vector<uint32_t> wlist(writers.begin(), writers.end());
            std::sort(wlist.begin(), wlist.end());
            for (size_t i = 0; i + 1 < wlist.size(); ++i) {
                addDependency(wlist[i], wlist[i + 1]);
            }
        }

        for (const auto& [buf, writers] : bufWriters) {
            auto& readers = bufReaders[buf];
            for (uint32_t w : writers) {
                for (uint32_t r : readers) {
                    if (w != r && writers.find(r) == writers.end()) {
                        addDependency(w, r);
                    }
                }
            }
            std::vector<uint32_t> wlist(writers.begin(), writers.end());
            std::sort(wlist.begin(), wlist.end());
            for (size_t i = 0; i + 1 < wlist.size(); ++i) {
                addDependency(wlist[i], wlist[i + 1]);
            }
        }

        for (const auto& [src, dst] : m_manualDependencies) {
            adj[src].push_back(dst);
        }

        // 拓扑排序
        auto order = topologicalSort(adj);
        m_sortedPasses.clear();
        for (uint32_t idx : order) {
            m_sortedPasses.push_back(m_passes[idx].get());
        }

        struct TexturePassInfo {
            int32_t lastWriterIndex = -1;
            int32_t firstReaderIndex = -1;
            RHI::PipelineStageFlags writeStage = static_cast<RHI::PipelineStageFlags>(0);
            RHI::AccessFlags writeAccess = static_cast<RHI::AccessFlags>(0);
            RHI::PipelineStageFlags readStage = static_cast<RHI::PipelineStageFlags>(0);
            RHI::AccessFlags readAccess = static_cast<RHI::AccessFlags>(0);
        };

        std::unordered_map<TextureId, TexturePassInfo> texPassInfo;

        for (int32_t passIdx = 0; passIdx < static_cast<int32_t>(m_sortedPasses.size()); ++passIdx) {
            auto* pass = m_sortedPasses[passIdx];
            for (auto tex : pass->getWriteTextures()) {
                auto& info = texPassInfo[tex];
                info.lastWriterIndex = passIdx;
                info.writeStage = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::ColorAttachmentOutput);
                info.writeAccess = static_cast<RHI::AccessFlags>(RHI::AccessFlag::ColorAttachmentWrite);
            }
            for (auto tex : pass->getReadTextures()) {
                auto& info = texPassInfo[tex];
                if (info.firstReaderIndex == -1) {
                    info.firstReaderIndex = passIdx;
                    info.readStage = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::FragmentShader);
                    info.readAccess = static_cast<RHI::AccessFlags>(RHI::AccessFlag::InputAttachmentRead);
                }
            }
        }

        for (const auto& [tex, info] : texPassInfo) {
            if (info.lastWriterIndex != -1 && info.firstReaderIndex != -1 && info.firstReaderIndex > info.lastWriterIndex) {
                RHI::SubpassDependency dep{};
                dep.srcSubpass = SUBPASS_EXTERNAL;
                dep.dstSubpass = 0;
                dep.srcStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::AllGraphics);
                dep.dstStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::FragmentShader);
                dep.srcAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::MemoryWrite);
                dep.dstAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::InputAttachmentRead);
                dep.byRegion = true;
                m_sortedPasses[info.firstReaderIndex]->getBuilder().addDependency(dep);
            }
        }

        for (auto& vt : m_virtualTextures) {
            if (vt.imported) {
                PhysicalTextureInfo info;
                info.handle = vt.externalHandle;
                info.views = vt.externalViews;
                m_textureMap[vt.id] = info;
            }
            else {
                RHI::TextureHandle handle = m_resMgr->createTexture(vt.desc, vt.name);
                if (!handle.isValid()) {
                    throw std::runtime_error("Failed to create physical texture: " + vt.name);
                }
                auto* texObj = m_resMgr->getTexture(handle);
                PhysicalTextureInfo info;
                info.handle = handle;
                info.views.push_back(texObj->getDefaultView());
                m_textureMap[vt.id] = info;
            }
        }

        for (auto& vb : m_virtualBuffers) {
            if (vb.imported) {
                m_bufferMap[vb.id] = vb.externalHandle;
            }
            else {
                RHI::BufferHandle handle = m_resMgr->createBuffer(vb.desc, vb.name);
                if (!handle.isValid()) {
                    throw std::runtime_error("Failed to create physical buffer: " + vb.name);
                }
                m_bufferMap[vb.id] = handle;
            }
        }

        std::unordered_map<TextureId, RHI::TextureDesc> texDescMap;
        for (const auto& vt : m_virtualTextures) {
            texDescMap[vt.id] = vt.desc;
        }

        // 编译每个 Pass
        for (auto* pass : m_sortedPasses) {
            if (!pass->compile(m_resMgr, m_textureMap, texDescMap, m_bufferMap)) {
                throw std::runtime_error("Failed to compile pass: " + pass->getName());
            }
        }

        if (m_swapchainImageCount == 0) {
            throw std::runtime_error("Swapchain image count not set before compile!");
        }

        m_perPassFramebuffers.clear();
        m_perPassFramebuffers.reserve(m_sortedPasses.size());

        for (auto* pass : m_sortedPasses) {
            std::vector<RHI::FramebufferHandle> framebuffersForPass;
            framebuffersForPass.reserve(m_swapchainImageCount);

            const auto& attachmentKeys = pass->getAttachmentNames();
            auto rpHandle = pass->getRenderPassHandle();
            auto* rpObj = m_resMgr->getRenderPass(rpHandle);
            if (!rpObj) {
                throw std::runtime_error("Invalid render pass for pass: " + pass->getName());
            }

            for (uint32_t imgIdx = 0; imgIdx < m_swapchainImageCount; ++imgIdx) {
                std::vector<void*> attachments;
                for (const auto& key : attachmentKeys) {
                    TextureId texId = pass->getBoundTextureId(key); 
                    auto it = m_textureMap.find(texId);
                    if (it == m_textureMap.end()) {
                        throw std::runtime_error("Texture not found for key: " + key);
                    }
                    const auto& texInfo = it->second;
                    void* view = nullptr;
                    if (texInfo.views.size() == 1) {
                        view = texInfo.views[0];
                    }
                    else if (texInfo.views.size() > 1) {
                        if (imgIdx >= texInfo.views.size()) {
                            throw std::runtime_error("View index out of range for texture");
                        }
                        view = texInfo.views[imgIdx];
                    }
                    else {
                        throw std::runtime_error("No views for texture");
                    }
                    attachments.push_back(view);
                }

                RHI::FramebufferDesc fbDesc;
                fbDesc.renderPass = rpObj->getNativeHandle();
                fbDesc.attachments = attachments;
                fbDesc.extent = { pass->getWidth(), pass->getHeight() };
                fbDesc.layers = 1;
                auto fb = m_resMgr->createFramebuffer(fbDesc);
                framebuffersForPass.push_back(fb);
            }
            m_perPassFramebuffers.push_back(std::move(framebuffersForPass));
        }
        return true;
    }

    void RenderGraph::execute(uint32_t frameIndex, RHI::RHICommandEncoder* encoder) {
        if (m_sortedPasses.empty()) return;
        if (m_perPassFramebuffers.size() != m_sortedPasses.size()) {
            throw std::runtime_error("Framebuffer count mismatch in RenderGraph");
        }
        for (size_t i = 0; i < m_sortedPasses.size(); ++i) {
            auto* pass = m_sortedPasses[i];
            RHI::FramebufferHandle fb = m_perPassFramebuffers[i][frameIndex];
            pass->execute(encoder, frameIndex, fb);
        }
    }

    RHI::TextureHandle RenderGraph::getPhysicalTextureHandle(TextureId id) const {
        auto it = m_textureMap.find(id);
        return it != m_textureMap.end() ? it->second.handle : RHI::TextureHandle::Null();
    }

    RHI::BufferHandle RenderGraph::getPhysicalBuffer(BufferId id) const {
        auto it = m_bufferMap.find(id);
        return it != m_bufferMap.end() ? it->second : RHI::BufferHandle::Null();
    }

    void RenderGraph::addDependency(PassNode* from, PassNode* to) {
        uint32_t srcIdx = UINT32_MAX, dstIdx = UINT32_MAX;
        for (uint32_t i = 0; i < m_passes.size(); ++i) {
            if (m_passes[i].get() == from) srcIdx = i;
            if (m_passes[i].get() == to) dstIdx = i;
        }
        if (srcIdx != UINT32_MAX && dstIdx != UINT32_MAX) {
            m_manualDependencies.push_back({ srcIdx, dstIdx });
        }
    }

} // namespace StarryEngine::RenderGraph