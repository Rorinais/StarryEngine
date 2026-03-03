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

        // 销毁物理纹理
        for (auto& [id, handle] : m_textureMap) {
            if (handle.isValid()) m_resMgr->destroy(handle);
        }
        // 销毁物理缓冲区
        for (auto& [id, handle] : m_bufferMap) {
            if (handle.isValid()) m_resMgr->destroy(handle);
        }
    }

    TextureId RenderGraph::createVirtualTexture(const RHI::TextureDesc& desc, const std::string& name) {
        if (!name.empty() && m_nameToTextureId.find(name) != m_nameToTextureId.end()) {
            throw std::runtime_error("Texture name already exists: " + name);
        }
        TextureId id = TextureId::Create(m_nextTextureId++, 1);
        VirtualTexture vt{ id, desc, name, false, RHI::TextureHandle::Null(), RHI::ImageLayout::Undefined };
        m_virtualTextures.push_back(vt);
        if (!name.empty()) {
            m_nameToTextureId[name] = id;
        }
        return id;
    }

    TextureId RenderGraph::importExternalTexture(RHI::TextureHandle externalHandle,
        const RHI::TextureDesc& desc,
        RHI::ImageLayout initialLayout,
        const std::string& name) {
        if (!name.empty() && m_nameToTextureId.find(name) != m_nameToTextureId.end()) {
            throw std::runtime_error("Texture name already exists: " + name);
        }
        TextureId id = TextureId::Create(m_nextTextureId++, 1);
        VirtualTexture vt{ id, desc, name, true, externalHandle, initialLayout };
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

        // 收集资源使用
        for (auto& pass : m_passes) {
            pass->collectResourceUsage(m_nameToTextureId, m_nameToBufferId);
        }

        // 构建依赖图
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

        // 纹理读写依赖
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

        // 缓冲区读写依赖
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
        // ---------- 添加手动依赖 ----------
        for (const auto& [src, dst] : m_manualDependencies) {
            adj[src].push_back(dst);
        }

        // 拓扑排序
        auto order = topologicalSort(adj);
        m_sortedPasses.clear();
        for (uint32_t idx : order) {
            m_sortedPasses.push_back(m_passes[idx].get());
        }

        // 创建物理纹理
        for (auto& vt : m_virtualTextures) {
            if (vt.imported) {
                m_textureMap[vt.id] = vt.externalHandle;
            }
            else {
                RHI::TextureHandle handle = m_resMgr->createTexture(vt.desc, vt.name);
                if (!handle.isValid()) {
                    throw std::runtime_error("Failed to create physical texture: " + vt.name);
                }
                m_textureMap[vt.id] = handle;
            }
        }

        // 创建物理缓冲区
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

        // 编译每个 Pass
        for (auto pass : m_sortedPasses) {
            if (!pass->compile(m_resMgr, m_textureMap, m_bufferMap)) {
                throw std::runtime_error("Failed to compile pass: " + pass->getName());
            }
        }

        return true;
    }

    void RenderGraph::execute(uint32_t frameIndex, RHI::RHICommandEncoder* encoder) {
        if (m_sortedPasses.empty()) return;
        if (m_passFramebuffers.size() != m_sortedPasses.size()) {
            throw std::runtime_error("Framebuffer count mismatch in RenderGraph");
        }

        for (size_t i = 0; i < m_sortedPasses.size(); ++i) {
            auto* pass = m_sortedPasses[i];
            RHI::FramebufferHandle fb = m_passFramebuffers[i];
            if (!fb.isValid()) {
                throw std::runtime_error("Invalid framebuffer for pass: " + pass->getName());
            }
            pass->execute(encoder, frameIndex, fb);
        }
    }

    RHI::TextureHandle RenderGraph::getPhysicalTexture(TextureId id) const {
        auto it = m_textureMap.find(id);
        return it != m_textureMap.end() ? it->second : RHI::TextureHandle::Null();
    }

    RHI::BufferHandle RenderGraph::getPhysicalBuffer(BufferId id) const {
        auto it = m_bufferMap.find(id);
        return it != m_bufferMap.end() ? it->second : RHI::BufferHandle::Null();
    }

} // namespace StarryEngine::RenderGraph