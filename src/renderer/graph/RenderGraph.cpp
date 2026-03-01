#include "RenderGraph.hpp"
#include <queue>
#include <stack>
#include <iostream>

namespace StarryEngine::RenderGraph {

    RenderGraph::RenderGraph(std::shared_ptr<VulkanRHI> rhi)
        : m_rhi(rhi), m_resMgr(rhi->getResourceManager()) {
    }

    RenderGraph::~RenderGraph() {
        if (m_rhi) {
            m_rhi->waitIdle();
        }

        std::cout << "[RenderGraph] Destructor started" << std::endl;
        for (auto& vt : m_virtualTextures) {
            if (!vt.imported && vt.physicalHandle.isValid()) {
                std::cout << "[RenderGraph] Destroying texture: " << vt.name << " handle=" << vt.physicalHandle.toString() << std::endl;
                m_resMgr->destroy(vt.physicalHandle);
            }
        }
        for (auto& vb : m_virtualBuffers) {
            if (!vb.imported && vb.physicalHandle.isValid()) {
                std::cout << "[RenderGraph] Destroying buffer: " << vb.name << std::endl;
                m_resMgr->destroy(vb.physicalHandle);
            }
        }
        m_textureMap.clear();
        m_bufferMap.clear();
        std::cout << "[RenderGraph] Destructor finished" << std::endl;
    }

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

        std::cout << "debug0: " << std::endl;

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

        std::cout << "debug1: " << std::endl;

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
        std::cout << "debug2: " << std::endl;
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

        std::cout << "[RenderGraph] Current nameToTextureId: ";
        for (const auto& [name, id] : m_nameToTextureId) {
            std::cout << name << " ";
        }
        std::cout << std::endl;

        for (auto& vt : m_virtualTextures) {
            if (vt.imported) {
                m_textureMap[vt.id] = vt.externalHandle;
                vt.physicalHandle = vt.externalHandle;
            }
            else {
                RHI::TextureHandle handle = m_resMgr->createTexture(vt.desc, vt.name);
                std::cout << "[RenderGraph] Created texture handle: " << handle.toString()
                    << ", isValid=" << handle.isValid()
                    << ", category=" << (int)handle.getCategoryRaw()
                    << ", expected_category=" << static_cast<int>(RHI::ResourceCategory::Texture) << std::endl;
                if (!handle.isValid()) {
                    std::cerr << "[RenderGraph] Failed to create physical texture (handle invalid): " << vt.name << std::endl;
                    std::cerr << "  extent: " << vt.desc.extent.width << "x" << vt.desc.extent.height
                        << ", format: " << static_cast<int>(vt.desc.format)
                        << ", allowRenderTarget=" << vt.desc.allowRenderTarget
                        << ", allowInputAttachment=" << vt.desc.allowInputAttachment << std::endl;
                    // 清理已创建的纹理
                    for (auto& createdVt : m_virtualTextures) {
                        if (createdVt.physicalHandle.isValid() && !createdVt.imported) {
                            m_resMgr->destroy(createdVt.physicalHandle);
                        }
                    }
                    return false;
                }

                // 获取对象指针并检查有效性
                auto* textureObj = m_resMgr->getTexture(handle);
                if (!textureObj) {
                    std::cerr << "[RenderGraph] textureObj is null for handle " << handle.toString() << std::endl;
                    m_resMgr->destroy(handle);
                    // 清理已创建的纹理...
                    return false;
                }
                if (!textureObj->isValid()) {
                    std::cerr << "[RenderGraph] textureObj is invalid for handle " << handle.toString() << std::endl;
                    // 可以尝试获取更详细的内部状态（如果纹理类提供了方法）
                    m_resMgr->destroy(handle);
                    // 清理已创建的纹理...
                    return false;
                }

                m_textureMap[vt.id] = handle;
                vt.physicalHandle = handle;
            }
        }

        std::cout << "debug3: " << std::endl;
        // 缓冲区同理
        for (auto& vb : m_virtualBuffers) {
            if (vb.imported) {
                m_bufferMap[vb.id] = vb.externalHandle;
                vb.physicalHandle = vb.externalHandle;
            }
            else {
                RHI::BufferHandle handle = m_resMgr->createBuffer(vb.desc, vb.name);
                if (!handle.isValid()) {
                    std::cerr << "[RenderGraph] Failed to create physical buffer: " << vb.name << std::endl;
                    for (auto& createdVb : m_virtualBuffers) {
                        if (createdVb.physicalHandle.isValid() && !createdVb.imported) {
                            m_resMgr->destroy(createdVb.physicalHandle);
                        }
                    }
                    return false;
                }
                m_bufferMap[vb.id] = handle;
                vb.physicalHandle = handle;
            }
        }
        std::cout << "debug4: " << std::endl;

        // 5. 编译每个 Pass
        for (auto pass : m_sortedPasses) {
            if (!pass->compile(m_resMgr, m_textureMap, m_bufferMap)) {
                std::cerr << "[RenderGraph] Failed to compile pass: " << pass->getName() << std::endl;
                return false;
            }
        }
        std::cout << "debug5: " << std::endl;
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