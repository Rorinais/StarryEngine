#include "RenderGraph.hpp"
#include <queue>
#include <stack>
#include <iostream>

namespace StarryEngine::RenderGraph {

    RenderGraph::RenderGraph(std::shared_ptr<VulkanRHI> rhi)
        : m_rhi(rhi), m_resMgr(rhi->getResourceManager()) {
    }

    RenderGraph::~RenderGraph() = default;

    TextureId RenderGraph::createVirtualTexture(const RHI::TextureDesc& desc, const std::string& name) {
        TextureId id = TextureId::Create(m_nextTextureId++, 1);
        VirtualTexture vt{ id,desc, name, false, RHI::TextureHandle::Null(), RHI::ImageLayout::Undefined, RHI::TextureHandle::Null() };
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
        TextureId id = TextureId::Create(m_nextTextureId++, 1);
        VirtualTexture vt{ id,desc, name, true, externalHandle, initialLayout, externalHandle };
        m_virtualTextures.push_back(vt);
        if (!name.empty()) {
            m_nameToTextureId[name] = id;
        }
        return id;
    }

    BufferId RenderGraph::createVirtualBuffer(const RHI::BufferDesc& desc, const std::string& name) {
        BufferId id = BufferId::Create(m_nextBufferId++, 1);
        VirtualBuffer vb{ id,desc, name, false, RHI::BufferHandle::Null(), RHI::BufferHandle::Null() };
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
        // 0. 检查是否有 Pass
        if (m_passes.empty()) {
            std::cerr << "[RenderGraph] No passes to compile." << std::endl;
            return false;
        }

        for (auto& pass : m_passes) {
            pass->collectResourceUsage(m_nameToTextureId, m_nameToBufferId);
        }

        // 2. 构建依赖图
        size_t passCount = m_passes.size();
        std::vector<std::vector<uint32_t>> adj(passCount);

        // 创建一个资源到读写 Pass 的映射
        std::unordered_map<TextureId, std::set<uint32_t>> texReaders, texWriters;
        std::unordered_map<BufferId, std::set<uint32_t>> bufReaders, bufWriters;

        for (uint32_t i = 0; i < passCount; ++i) {
            for (auto tex : m_passes[i]->getReadTextures()) texReaders[tex].insert(i);
            for (auto tex : m_passes[i]->getWriteTextures()) texWriters[tex].insert(i);
            for (auto buf : m_passes[i]->getReadBuffers()) bufReaders[buf].insert(i);
            for (auto buf : m_passes[i]->getWriteBuffers()) bufWriters[buf].insert(i);
        }

        // 根据资源依赖添加边
        auto addDependency = [&](uint32_t src, uint32_t dst) {
            adj[src].push_back(dst);
            };

        for (const auto& [tex, writers] : texWriters) {
            auto& readers = texReaders[tex];
            for (uint32_t w : writers) {
                for (uint32_t r : readers) {
                    if (w != r) addDependency(w, r); // 写者必须在读者之前
                }
            }
            // 写后写依赖：如果多个 Pass 写入同一资源，按顺序执行
            std::vector<uint32_t> wlist(writers.begin(), writers.end());
            for (size_t i = 0; i + 1 < wlist.size(); ++i) {
                addDependency(wlist[i], wlist[i + 1]);
            }
        }

        // 类似处理 Buffer
        for (const auto& [buf, writers] : bufWriters) {
            auto& readers = bufReaders[buf];
            for (uint32_t w : writers) {
                for (uint32_t r : readers) {
                    if (w != r) addDependency(w, r);
                }
            }
            std::vector<uint32_t> wlist(writers.begin(), writers.end());
            for (size_t i = 0; i + 1 < wlist.size(); ++i) {
                addDependency(wlist[i], wlist[i + 1]);
            }
        }

        // 3. 拓扑排序
        try {
            auto order = topologicalSort(adj);
            m_sortedPasses.clear();
            for (uint32_t idx : order) {
                m_sortedPasses.push_back(m_passes[idx].get());
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[RenderGraph] " << e.what() << std::endl;
            return false;
        }

        for (auto& vt : m_virtualTextures) {
            if (vt.imported) {
                m_textureMap[vt.id] = vt.externalHandle;
                vt.physicalHandle = vt.externalHandle;
            }
            else {
                RHI::TextureHandle handle = m_resMgr->createTexture(vt.desc, vt.name);
                if (!handle.isValid()) { std::cerr << "[RenderGraph] Failed to create physical texture: " << vt.name << std::endl; }
                m_textureMap[vt.id] = handle;
                vt.physicalHandle = handle;
            }
        }

        for (auto& vb : m_virtualBuffers) {
            if (vb.imported) {
                m_bufferMap[vb.id] = vb.externalHandle;
                vb.physicalHandle = vb.externalHandle;
            }
            else {
                RHI::BufferHandle handle = m_resMgr->createBuffer(vb.desc, vb.name);
                if (!handle.isValid()) { std::cerr << "[RenderGraph] Failed to create physical buffer: " << vb.name << std::endl; }
                m_bufferMap[vb.id] = handle;
                vb.physicalHandle = handle;
            }
        }

        // 5. 编译每个 Pass
        for (auto pass : m_sortedPasses) {
            if (!pass->compile(m_resMgr, m_textureMap, m_bufferMap)) {
                std::cerr << "[RenderGraph] Failed to compile pass: " << pass->getName() << std::endl;
                return false;
            }
        }

        return true;
    }

    void RenderGraph::execute(uint32_t frameIndex, RHI::RHICommandEncoder* encoder) {
        if (m_sortedPasses.empty()) return;
        if (m_passFramebuffers.size() != m_sortedPasses.size()) {
            std::cerr << "[RenderGraph] Framebuffer count mismatch" << std::endl;
            return;
        }

        for (size_t i = 0; i < m_sortedPasses.size(); ++i) {
            auto* pass = m_sortedPasses[i];
            RHI::FramebufferHandle fb = m_passFramebuffers[i];
            if (!fb.isValid()) {
                std::cerr << "[RenderGraph] Invalid framebuffer for pass: " << pass->getName() << std::endl;
                continue;
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