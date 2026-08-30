#include <renderer/graph/RenderGraph.hpp>
#include <core/JobSystem.hpp>
#include <logging/Logger.hpp>
#include <algorithm>
#include <queue>
#include <stack>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>

namespace StarryEngine::RenderGraph {

    RenderGraph::RenderGraph(std::shared_ptr<RHI::IRHI> rhi)
        : m_rhi(rhi), m_resMgr(rhi->getResourceManager()) {
        m_useDynamicRendering = rhi && rhi->supportsDynamicRendering();
        if (std::getenv("STARRY_FORCE_RENDER_PASS")) m_useDynamicRendering = false;
        if (m_useDynamicRendering) {
            LOG_INFO("[RenderGraph] 使用动态渲染（RenderNode，VK_KHR_dynamic_rendering）");
        } else {
            LOG_INFO("[RenderGraph] 使用传统 render pass（PassNode）");
        }
    }

    RenderGraph::~RenderGraph() {
        if (m_rhi) {
            m_rhi->waitIdle();
        }

        for (auto& passFbs : m_perPassFramebuffers) {
            for (auto fb : passFbs) {
                if (fb.isValid()) m_resMgr->destroy(fb);
            }
        }

        for (auto& [id, info] : m_textureMap) {
            if (info.handle.isValid()) m_resMgr->destroy(info.handle);
        }

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
        m_textureFormats.push_back(desc.format);
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
        m_textureFormats.push_back(desc.format);
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

    GraphNode* RenderGraph::addGraphicsPassNode(const std::string& name) {
        std::unique_ptr<GraphNode> node;
        if (m_useDynamicRendering) node = std::make_unique<RenderNode>(name);
        else node = std::make_unique<PassNode>(name);
        GraphNode* ptr = node.get();
        m_passes.push_back(std::move(node));
        return ptr;
    }

    GraphNode* RenderGraph::addGraphicsPassNodeShared(const std::string& name, const std::string& dedupKey) {
        auto it = m_sharedGraphicsNodes.find(dedupKey);
        if (it != m_sharedGraphicsNodes.end()) return it->second;  // 复用已有节点
        GraphNode* node = addGraphicsPassNode(name);
        m_sharedGraphicsNodes[dedupKey] = node;
        return node;
    }

    GraphNode* RenderGraph::addComputePassNode(const std::string& name) {
        std::unique_ptr<GraphNode> node;
        if (m_useDynamicRendering) node = std::make_unique<RenderNode>(name, PassType::Compute);
        else node = std::make_unique<PassNode>(name, PassType::Compute);
        GraphNode* ptr = node.get();
        m_passes.push_back(std::move(node));
        return ptr;
    }

    GraphNode* RenderGraph::findNode(const std::string& name) {
        for (auto& pass : m_passes) {
            if (pass->getName() == name) return pass.get();
        }
        return nullptr;
    }

    // 拓扑排序函数，根据邻接表返回节点的执行顺序，若存在环则抛出异常
    std::vector<uint32_t> RenderGraph::topologicalSort(const std::vector<std::vector<uint32_t>>& adj) const {
        size_t n = adj.size();
        std::vector<uint32_t> inDegree(n, 0);          // 记录每个节点的入度
        for (const auto& edges : adj) {
            for (uint32_t v : edges) {
                inDegree[v]++;                          // 统计入度
            }
        }

        std::queue<uint32_t> q;                         // 队列用于存储入度为0的节点
        for (uint32_t i = 0; i < n; ++i) {
            if (inDegree[i] == 0) q.push(i);
        }

        std::vector<uint32_t> order;                    // 存储拓扑排序结果
        while (!q.empty()) {
            uint32_t u = q.front(); q.pop();
            order.push_back(u);
            for (uint32_t v : adj[u]) {
                if (--inDegree[v] == 0) q.push(v);      // 移除边，若入度变为0则入队
            }
        }

        if (order.size() != n) {
            throw std::runtime_error("RenderGraph: Cyclic dependency detected!");
        }
        return order;
    }


    void RenderGraph::dependencyAnalysis() {
        if (m_passes.empty()) {
            throw std::runtime_error("No passes to compile.");
        }

        size_t passCount = m_passes.size();
        std::vector<std::vector<uint32_t>> adj(passCount);   // 邻接表，表示 Pass 之间的依赖关系

        // 记录每个纹理/缓冲区被哪些 Pass 读取或写入
        std::unordered_map<TextureId, std::set<uint32_t>> texReaders, texWriters;
        std::unordered_map<BufferId, std::set<uint32_t>> bufReaders, bufWriters;

        for (uint32_t i = 0; i < passCount; ++i) {
            for (auto tex : m_passes[i]->getReadTextures()) texReaders[tex].insert(i);
            for (auto tex : m_passes[i]->getWriteTextures()) texWriters[tex].insert(i);
            for (auto buf : m_passes[i]->getReadBuffers()) bufReaders[buf].insert(i);
            for (auto buf : m_passes[i]->getWriteBuffers()) bufWriters[buf].insert(i);
        }

        // 辅助函数：添加一条从 src 到 dst 的依赖边
        auto addDependency = [&](uint32_t src, uint32_t dst) {
            adj[src].push_back(dst);
            };

        // 根据读写关系建立依赖（纹理与缓冲区逻辑相同）：
        //   - 写后写：多个写 Pass 按 pass 列表顺序串行
        //   - 写后读：reader 只依赖"它之前最近的写者"（不是所有写者——否则中间读者会被
        //     后续写者倒序连成环），且必须在它之后的写者之前完成（否则读到的是覆盖后的内容）
        auto addResourceDeps = [&](auto& writersMap, auto& readersMap) {
            for (const auto& [res, writers] : writersMap) {
                auto& readers = readersMap[res];
                std::vector<uint32_t> wlist(writers.begin(), writers.end());
                std::sort(wlist.begin(), wlist.end());

                for (size_t i = 0; i + 1 < wlist.size(); ++i) {
                    addDependency(wlist[i], wlist[i + 1]);   // 写后写
                }
                for (uint32_t r : readers) {
                    auto it = std::lower_bound(wlist.begin(), wlist.end(), r);  // 第一个 >= r 的写者
                    if (it != wlist.begin()) addDependency(*std::prev(it), r);  // 最近前驱写者 → reader
                    for (auto it2 = it; it2 != wlist.end(); ++it2) {
                        if (*it2 != r) addDependency(r, *it2);   // reader → 后续写者
                    }
                }
            }
        };
        addResourceDeps(texWriters, texReaders);
        addResourceDeps(bufWriters, bufReaders);

        auto order = topologicalSort(adj);
        m_sortedPasses.clear();
        for (uint32_t idx : order) {
            m_sortedPasses.push_back(m_passes[idx].get());
        }
    }

    bool RenderGraph::compile() {
        // 依赖分析（pass 顺序 + 纹理生命周期）
        dependencyAnalysis();

        // Pass Culling：从最终输出纹理（Swapchain）反向裁剪不可达 Pass
        // 必须在 dependencyAnalysis() 后、物理资源分配前执行
        cullUnusedPasses();

        // 纹理使用分析（first/last user/writer/reader + 跨 pass 输入依赖）
        std::unordered_map<TextureId, TexturePassInfo> texPassInfo;
        analyzeTextureUsage(texPassInfo);

        // ── Memory Aliasing：贪心复用生命周期不重叠的 transient 纹理内存 ──
        performMemoryAliasing(texPassInfo);

        // 物理资源分配 + 附件 loadOp 推断 + pass 编译
        createPhysicalResources(texPassInfo);

        // 传统模式：创建 framebuffer；动态模式：收集附件视图
        createFrameBuffer();
        if (m_useDynamicRendering) collectDynamicViews();

        // 布局转换分析（首用/写读间 barrier）
        buildLayoutTransitions(texPassInfo);

        return true;
    }

    // 分析纹理使用：记录每个纹理的 first/last user、writer、reader，并生成跨 pass 输入依赖
    void RenderGraph::analyzeTextureUsage(std::unordered_map<TextureId, TexturePassInfo>& texPassInfo) {
        for (int32_t passIdx = 0; passIdx < static_cast<int32_t>(m_sortedPasses.size()); ++passIdx) {
            auto* pass = m_sortedPasses[passIdx];
            for (auto tex : pass->getWriteTextures()) {
                auto& info = texPassInfo[tex];
                if (info.firstUserIndex == -1) info.firstUserIndex = passIdx;
                info.lastUserIndex = passIdx;
                if (info.firstWriterIndex == -1) info.firstWriterIndex = passIdx;
                info.lastWriterIndex = passIdx;
            }
            for (auto tex : pass->getReadTextures()) {
                auto& info = texPassInfo[tex];
                if (info.firstUserIndex == -1) info.firstUserIndex = passIdx;
                info.lastUserIndex = passIdx;
                if (info.firstReaderIndex == -1) info.firstReaderIndex = passIdx;
            }
        }

        // 写后跨 pass 读：外部→首个读者 输入依赖（传统 render pass 的 subpass 外部依赖）
        for (const auto& [tex, info] : texPassInfo) {
            if (info.lastWriterIndex != -1 && info.firstReaderIndex != -1 && info.firstReaderIndex > info.lastWriterIndex) {
                RHI::SubpassDependency dep{};
                dep.srcSubpass = SUBPASS_EXTERNAL;
                dep.dstSubpass = 0;
                dep.srcStageMask = RHI::PipelineStage::AllGraphics;
                dep.dstStageMask = RHI::PipelineStage::FragmentShader;
                dep.srcAccessMask = RHI::AccessFlag::MemoryWrite;
                dep.dstAccessMask = RHI::AccessFlag::InputAttachmentRead;
                dep.byRegion = true;
                m_sortedPasses[info.firstReaderIndex]->addDependency(dep);
            }
        }
    }

    // 物理资源分配：纹理/缓冲创建 + 声明式附件 loadOp 推断 + pass 编译
    void RenderGraph::createPhysicalResources(const std::unordered_map<TextureId, TexturePassInfo>& texPassInfo) {
        // compute 写为 storage image 的纹理需要 STORAGE usage（允许无序访问）
        std::unordered_set<TextureId> storageTexIds;
        for (auto* pass : m_sortedPasses) {
            if (pass->getType() == PassType::Compute) {
                for (auto tex : pass->getWriteTextures()) storageTexIds.insert(tex);
            }
        }

        // 纹理物理分配
        for (auto& vt : m_virtualTextures) {
            if (vt.imported) {
                PhysicalTextureInfo info;
                info.handle = vt.externalHandle;
                info.views = vt.externalViews;
                m_textureMap[vt.id] = info;
            }
            else {
                if (storageTexIds.count(vt.id)) vt.desc.allowUnorderedAccess = true;
                RHI::TextureHandle handle = m_resMgr->createTexture(vt.desc);
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

        // 缓冲物理分配
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

        // 纹理描述映射（pass 编译用）
        std::unordered_map<TextureId, RHI::TextureDesc> texDescMap;
        for (const auto& vt : m_virtualTextures) {
            texDescMap[vt.id] = vt.desc;
        }

        // ── 声明式附件：推断每个 pass 未显式指定的 loadOp/布局（首写→Clear，后续→Load）──
        for (int32_t passIdx = 0; passIdx < static_cast<int32_t>(m_sortedPasses.size()); ++passIdx) {
            auto* pass = m_sortedPasses[passIdx];
            pass->resolveInferredAttachments([&](TextureId tex) {
                auto it = texPassInfo.find(tex);
                return it != texPassInfo.end() && it->second.firstWriterIndex == passIdx;
            });
        }

        // pass 编译（渲染通道/管线相关准备）
        for (auto* pass : m_sortedPasses) {
            if (!pass->compile(m_resMgr, m_textureMap, texDescMap, m_bufferMap)) {
                throw std::runtime_error("Failed to compile pass: " + pass->getName());
            }
        }
    }

    // 动态渲染：收集每 pass 的颜色/深度附件视图（[pass][交换链图像索引]）
    void RenderGraph::collectDynamicViews() {
        m_perPassViews.clear();
        m_perPassViews.reserve(m_sortedPasses.size());
        for (auto* pass : m_sortedPasses) {
            PassAttachmentViews pv;
            if (pass->getType() == PassType::Compute) {
                m_perPassViews.push_back(std::move(pv));
                continue;
            }

            // 颜色附件视图：colorViews[交换链图像索引][附件]
            const auto colorKeys = pass->getColorAttachmentNames();
            pv.colorViews.reserve(m_swapchainImageCount);
            for (uint32_t imgIdx = 0; imgIdx < m_swapchainImageCount; ++imgIdx) {
                std::vector<void*> imageViews;
                for (const auto& key : colorKeys) {
                    TextureId texId = pass->getTextureIdForAttachmentKey(key);
                    auto it = m_textureMap.find(texId);
                    if (it == m_textureMap.end()) continue;
                    const auto& texInfo = it->second;
                    if (texInfo.handle.isValid() && !texInfo.views.empty()) {
                        imageViews.push_back(texInfo.views[0]);
                    }
                    else if (!texInfo.handle.isValid()) {
                        if (texInfo.views.size() == 1) imageViews.push_back(texInfo.views[0]);
                        else if (imgIdx < texInfo.views.size()) imageViews.push_back(texInfo.views[imgIdx]);
                    }
                }
                pv.colorViews.push_back(std::move(imageViews));
            }

            // 深度附件视图（纯深度 pass 如 ShadowPass；无深度 = nullptr）
            const auto depthKeys = pass->getDepthAttachmentNames();
            if (!depthKeys.empty()) {
                TextureId texId = pass->getTextureIdForAttachmentKey(depthKeys[0]);
                auto it = m_textureMap.find(texId);
                if (it != m_textureMap.end() && !it->second.views.empty()) {
                    pv.depthView = it->second.views[0];
                }
            }
            m_perPassViews.push_back(std::move(pv));
        }
    }

    // 布局转换分析：生成首用（Undefined→目标）与写读间 barrier
    void RenderGraph::buildLayoutTransitions(const std::unordered_map<TextureId, TexturePassInfo>& texPassInfo) {
        struct PassLayoutInfo {
            RHI::ImageLayout initial;
            RHI::ImageLayout final;
            bool used = false;
        };
        std::unordered_map<TextureId, std::vector<PassLayoutInfo>> texPassLayouts;
        texPassLayouts.reserve(m_virtualTextures.size());

        // 收集每个 pass 对每个纹理的 initial/final 布局
        for (size_t i = 0; i < m_sortedPasses.size(); ++i) {
            auto* pass = m_sortedPasses[i];
            std::set<TextureId> allTex = pass->getReadTextures();
            allTex.insert(pass->getWriteTextures().begin(), pass->getWriteTextures().end());
            for (auto texId : allTex) {
                auto [init, fin] = pass->getTextureLayout(texId);
                if (texPassLayouts[texId].size() <= i) {
                    texPassLayouts[texId].resize(m_sortedPasses.size());
                }
                texPassLayouts[texId][i] = { init, fin, true };
            }
        }

        m_layoutTransitions.clear();

        // 首用转换：Undefined → 首个使用目标（动态渲染需显式转换，传统由 render pass 隐式处理）
        for (const auto& [texId, layouts] : texPassLayouts) {
            auto infoIt = texPassInfo.find(texId);
            if (infoIt == texPassInfo.end()) continue;
            int32_t firstUseIdx = infoIt->second.firstUserIndex;
            if (firstUseIdx == -1) continue;
            const auto& firstLayoutInfo = layouts[firstUseIdx];
            RHI::ImageLayout firstTarget = firstLayoutInfo.initial;
            if (m_useDynamicRendering && firstTarget == RHI::ImageLayout::Undefined
                && firstLayoutInfo.final != RHI::ImageLayout::Undefined) {
                firstTarget = firstLayoutInfo.final;
            }
            if (firstTarget == RHI::ImageLayout::Undefined) continue;

            RHI::Format format = getTextureFormat(texId);
            bool computeReader = m_sortedPasses[firstUseIdx]->getType() == PassType::Compute;
            auto [srcStage, srcAccess] = getStageAccessFromLayout(RHI::ImageLayout::Undefined);
            auto [dstStage, dstAccess] = getReadStageAccess(firstTarget, computeReader);
            uint32_t aspect = getAspectMask(format);
            m_layoutTransitions.push_back({
                -1, static_cast<uint32_t>(firstUseIdx), texId,
                RHI::ImageLayout::Undefined, firstTarget,
                srcStage, dstStage, srcAccess, dstAccess, aspect
                });
        }

        // 写读间转换：write pass 之后 read pass 之前。
        // 只用"最近的写者 → 读者"（writes/reads 按 pass 序递增，lower_bound 定位），
        // 避免 O(写者数×读者数) 的笛卡尔积产生大量冗余 barrier；同布局写读也在此同步。
        for (const auto& [texId, layouts] : texPassLayouts) {
            std::vector<std::pair<uint32_t, RHI::ImageLayout>> writes;
            std::vector<std::pair<uint32_t, RHI::ImageLayout>> reads;
            for (uint32_t i = 0; i < layouts.size(); ++i) {
                const auto& info = layouts[i];
                if (!info.used) continue;
                if (info.final != RHI::ImageLayout::Undefined) writes.emplace_back(i, info.final);
                if (info.initial != RHI::ImageLayout::Undefined) reads.emplace_back(i, info.initial);
            }
            if (writes.empty() || reads.empty()) continue;

            // 纹理格式（循环外查一次）
            RHI::Format format = getTextureFormat(texId);
            uint32_t aspect = getAspectMask(format);

            // 对每个 reader 找"它之前最近的 writer"
            auto wit = writes.begin();
            for (const auto& [readIdx, readLayout] : reads) {
                while (wit != writes.end() && wit->first < readIdx) ++wit;
                if (wit == writes.begin()) continue;          // 前面没有写者
                auto& [writeIdx, writeLayout] = *std::prev(wit);
                if (writeIdx >= readIdx) continue;

                bool computeReader = m_sortedPasses[readIdx]->getType() == PassType::Compute;
                auto [srcStage, srcAccess] = getStageAccessFromLayout(writeLayout);
                auto [dstStage, dstAccess] = getReadStageAccess(readLayout, computeReader);
                m_layoutTransitions.push_back({
                    static_cast<int32_t>(writeIdx), readIdx, texId,
                    writeLayout, readLayout,
                    srcStage, dstStage, srcAccess, dstAccess, aspect
                    });
            }
        }

        // 按目标 pass 排序（execute 顺序应用 barrier）
        std::sort(m_layoutTransitions.begin(), m_layoutTransitions.end(),
            [](const LayoutTransition& a, const LayoutTransition& b) {
                if (a.dstPassIdx != b.dstPassIdx) return a.dstPassIdx < b.dstPassIdx;
                if (a.srcPassIdx == -1 && b.srcPassIdx != -1) return true;
                if (a.srcPassIdx != -1 && b.srcPassIdx == -1) return false;
                return a.srcPassIdx < b.srcPassIdx;
            });
    }

    void RenderGraph::createFrameBuffer() {
        if (m_useDynamicRendering) return;   // 动态渲染无需 framebuffer 对象
        if (m_swapchainImageCount == 0) {
            throw std::runtime_error("Swapchain image count not set before compile!");
        }

        m_perPassFramebuffers.clear();
        m_perPassFramebuffers.reserve(m_sortedPasses.size());

        for (auto* pass : m_sortedPasses) {
            // Compute Pass 不需要 Framebuffer
            if (pass->getType() == PassType::Compute) {
                m_perPassFramebuffers.push_back({});
                continue;
            }

            std::vector<RHI::FramebufferHandle> framebuffersForPass;
            framebuffersForPass.reserve(m_swapchainImageCount);

            const auto& attachmentKeys = pass->getAttachmentNames();
            auto rpHandle = pass->getRenderPassHandle();
            auto* rpObj = m_resMgr->getRenderPass(rpHandle);
            if (!rpObj) {
                throw std::runtime_error("Invalid render pass for pass: " + pass->getName());
            }

            // 对每个交换链图像索引（或帧索引）创建对应的帧缓冲
            for (uint32_t imgIdx = 0; imgIdx < m_swapchainImageCount; ++imgIdx) {
                std::vector<RHI::TextureHandle> attachments;          // 有句柄的附件
                RHI::FramebufferDesc fbDesc;
                fbDesc.extent = { pass->getWidth(), pass->getHeight() };
                fbDesc.layers = 1;
                for (const auto& key : attachmentKeys) {
                    TextureId texId = pass->getTextureIdForAttachmentKey(key);
                    auto it = m_textureMap.find(texId);
                    if (it == m_textureMap.end()) {
                        throw std::runtime_error("Texture not found for key: " + key);
                    }
                    const auto& texInfo = it->second;
                    if (texInfo.handle.isValid()) {                   // 常规纹理 → 句柄（默认视图）
                        attachments.push_back(texInfo.handle);
                    }
                    else {                                            // 外部纹理（如交换链）→ 原生视图
                        void* view = nullptr;
                        if (texInfo.views.size() == 1) {              // 单视图
                            view = texInfo.views[0];
                        }
                        else if (texInfo.views.size() > 1) {          // 多视图（每图像一个视图）
                            if (imgIdx >= texInfo.views.size()) {
                                throw std::runtime_error("View index out of range for texture");
                            }
                            view = texInfo.views[imgIdx];
                        }
                        else {
                            throw std::runtime_error("No views for texture");
                        }
                        fbDesc.nativeAttachments.push_back(view);
                    }
                }
                auto fb = m_resMgr->createFramebuffer(rpHandle, attachments, fbDesc);
                framebuffersForPass.push_back(fb);
            }
            m_perPassFramebuffers.push_back(std::move(framebuffersForPass));
        }
    }

    void RenderGraph::execute(RHI::RHICommandEncoder* encoder, const RenderContext& context, uint32_t frameIndex,
        uint32_t frameSlot, const ParallelRecordingContext* parallel) {
        if (m_sortedPasses.empty()) return;
        // 动态渲染：附件视图替代 framebuffer（不校验 framebuffer 数量）
        if (!m_useDynamicRendering && m_perPassFramebuffers.size() != m_sortedPasses.size()) {
            throw std::runtime_error("Framebuffer count mismatch in RenderGraph");
        }

        const bool parallelEnabled = (parallel != nullptr && parallel->jobs != nullptr);

        // ── Phase 1（并行路径）：预分配 secondary + 逐 (pass×subpass) 提交录制 job ──
        // 录制只是把命令写进各自的 command buffer（互不相交），依赖顺序由 Phase 2
        // 主缓冲上的布局转换 barrier + executeCommands 保证，与录制先后无关。
        std::vector<std::vector<void*>> secondaries;   // secondaries[i][sub] = pass i subpass sub 的 native secondary 句柄
        if (parallelEnabled) {
            recordSecondariesParallel(encoder, context, frameIndex, frameSlot, parallel, secondaries);
        }

        // ── Phase 2：主缓冲逐 pass 执行（barrier + 串行/并行双模入口）──
        std::unordered_map<TextureId, RHI::ImageLayout> currentLayouts;
        for (const auto& vt : m_virtualTextures) {
            if (vt.imported) {
                currentLayouts[vt.id] = vt.initialLayout;
            }
        }

        auto transIt = m_layoutTransitions.begin();
        for (size_t i = 0; i < m_sortedPasses.size(); ++i) {
            // 当前 pass 前应用其依赖的布局转换
            while (transIt != m_layoutTransitions.end() && transIt->dstPassIdx == i) {
                applyLayoutTransition(encoder, *transIt, currentLayouts);
                ++transIt;
            }

            // pass 主体（串行直接执行 / 并行执行已录 secondary）
            recordPassOnPrimary(encoder, context, frameIndex, frameSlot, i, secondaries);

            // 更新布局跟踪（该 pass 写过的纹理 final 布局）
            auto* pass = m_sortedPasses[i];
            auto allTex = pass->getReadTextures();
            allTex.insert(pass->getWriteTextures().begin(), pass->getWriteTextures().end());
            for (auto texId : allTex) {
                auto [init, fin] = pass->getTextureLayout(texId);
                if (fin != RHI::ImageLayout::Undefined) {
                    currentLayouts[texId] = fin;
                }
            }
        }
    }

    // Phase 1：并行录制 secondary（每 pass×subpass 一个 job，主线程 waitAll 后读取）
    void RenderGraph::recordSecondariesParallel(RHI::RHICommandEncoder* encoder, const RenderContext& context,
        uint32_t frameIndex, uint32_t frameSlot, const ParallelRecordingContext* parallel,
        std::vector<std::vector<void*>>& secondaries) {
        secondaries.resize(m_sortedPasses.size());
        for (size_t i = 0; i < m_sortedPasses.size(); ++i) {
            auto* pass = m_sortedPasses[i];
            if (!pass->isEnabled()) continue;   // 禁用 pass：Phase 2 用 Inline 空体（保持 clear 语义）

            const bool isCompute = (pass->getType() == PassType::Compute);
            const uint32_t subpassCount = isCompute ? 1u : pass->getSubpassCount();

            // 继承信息（compute 不需要渲染通道继承）
            RHI::RenderPassHandle nativeRP = RHI::RenderPassHandle{};
            RHI::FramebufferHandle nativeFB = RHI::FramebufferHandle{};
            RHI::Format dynColorFmt = RHI::Format::Undefined;
            RHI::Format dynDepthFmt = RHI::Format::Undefined;
            if (!isCompute && !m_useDynamicRendering) {
                nativeRP = pass->getRenderPassHandle();
                nativeFB = m_perPassFramebuffers[i][frameIndex];
                if (!nativeRP.isValid() || !nativeFB.isValid()) {
                    LOG_WARN("[{}] 并行录制：renderPass={} fb={} — 跳过", pass->getName(),nativeRP.isValid(), nativeFB.isValid());
                    continue;
                }
            }
            else if (!isCompute && m_useDynamicRendering) {
                // 动态渲染继承：取该 pass 颜色/深度附件格式（与主缓冲 beginRendering 一致）
                const auto& colorKeys = pass->getColorAttachmentNames();
                if (!colorKeys.empty()) {
                    TextureId tid = pass->getTextureIdForAttachmentKey(colorKeys[0]);
                    dynColorFmt = getTextureFormat(tid);
                }
                const auto& depthKeys = pass->getDepthAttachmentNames();
                if (!depthKeys.empty()) {
                    TextureId tid = pass->getTextureIdForAttachmentKey(depthKeys[0]);
                    dynDepthFmt = getTextureFormat(tid);
                }
            }

            // 预置槽位：每个 job 只写自己的下标（互不相交），主线程 waitAll 后才读 → 无锁安全
            secondaries[i].resize(subpassCount, nullptr);
            for (uint32_t sub = 0; sub < subpassCount; ++sub) {
                // 分配 + beginSecondary 都移进 job（在"执行线程"上做）：
                // 执行线程从自己的 per-worker 命令池取 secondary，避免多线程并发录同一池
                // （验证层 UNASSIGNED-Threading-MultipleThreads-Write → NVIDIA 驱动 submit 段错误）。
                parallel->jobs->submit(
                    [pass, alloc = parallel->allocateSecondary, ctx = context, frameIndex, sub,
                     nativeRP, nativeFB, isCompute, useDyn = m_useDynamicRendering,
                     dynColorFmt, dynDepthFmt, frameSlot, slot = &secondaries[i][sub]]() {
                        auto sec = alloc(JobSystem::currentWorkerIndex());
                        if (!sec) return;
                        if (isCompute) {
                            sec->beginSecondary({ .renderPassContinue = false });
                        } else if (useDyn) {
                            sec->beginSecondary({ .useDynamicRendering = true,
                                                 .colorFormat = dynColorFmt,
                                                 .depthFormat = dynDepthFmt });
                        } else {
                            sec->beginSecondary({ .renderPass = nativeRP, .framebuffer = nativeFB,
                                                 .subpass = sub, .renderPassContinue = true });
                        }
                        pass->recordBody(sec.get(), ctx, frameIndex, RHI::FramebufferHandle{}, sub, frameSlot);
                        sec->end();   // vkEndCommandBuffer：Phase 2 才能 executeCommands
                        *slot = sec->getCommandBuffer();   // 存 native 句柄给 Phase 2
                    });
            }
        }
        parallel->jobs->waitAll();   // 帧 barrier：全部 secondary 录完
    }

    // 单 pass 主缓冲执行：串行（直接 execute）或并行（begin + executeCommands + end）
    void RenderGraph::recordPassOnPrimary(RHI::RHICommandEncoder* encoder, const RenderContext& context,
        uint32_t frameIndex, uint32_t frameSlot, size_t passIndex,
        const std::vector<std::vector<void*>>& secondaries) {
        auto* pass = m_sortedPasses[passIndex];

        // ── 串行路径：统一调 pass 虚接口（compute/graphics 差异在节点内部处理）──
        if (secondaries.empty()) {
            if (m_useDynamicRendering) {
                // RenderNode::executeDynamic 内部：compute → recorder；graphics → beginRendering + executors
                const auto& attViews = getAttachmentViewsForPass(passIndex, frameIndex);
                void* depthView = getDepthAttachmentViewForPass(passIndex, frameIndex);
                pass->executeDynamic(encoder, context, frameIndex, attViews, frameSlot, depthView);
            } else {
                // PassNode::execute 内部：compute → recorder；graphics → render pass + executors
                RHI::FramebufferHandle fb = getFramebufferForPass(passIndex, frameIndex);
                pass->execute(encoder, context, frameIndex, fb, frameSlot);
            }
            return;
        }

        // ── 并行路径：统一调"主缓冲执行已录 secondary"（compute/graphics 差异在节点内部处理）──
        RHI::FramebufferHandle fb = getFramebufferForPass(passIndex, frameIndex);
        const auto& attViews = getAttachmentViewsForPass(passIndex, frameIndex);
        void* depthView = getDepthAttachmentViewForPass(passIndex, frameIndex);
        pass->executeSecondariesOnPrimary(encoder, fb, attViews, depthView, secondaries[passIndex]);
    }

    // 取某 pass 的 framebuffer（传统模式 graphics pass）；compute/动态返回空 handle
    RHI::FramebufferHandle RenderGraph::getFramebufferForPass(size_t passIndex, uint32_t frameIndex) const {
        if (m_useDynamicRendering || m_sortedPasses[passIndex]->getType() == PassType::Compute) {
            return RHI::FramebufferHandle{};
        }
        return m_perPassFramebuffers[passIndex][frameIndex];
    }

    // 单个布局转换 barrier：应用 src→dst 布局/阶段/访问转换到 encoder
    void RenderGraph::applyLayoutTransition(RHI::RHICommandEncoder* encoder,
        const LayoutTransition& trans,
        std::unordered_map<TextureId, RHI::ImageLayout>& currentLayouts) {
        auto texIt = m_textureMap.find(trans.texId);
        if (texIt == m_textureMap.end() || !texIt->second.handle.isValid()) return;

        RHI::ImageLayout srcLayout = trans.srcLayout;
        auto curIt = currentLayouts.find(trans.texId);
        if (curIt != currentLayouts.end() && curIt->second != RHI::ImageLayout::Undefined) {
            srcLayout = curIt->second;
        }

        RHI::ImageBarrier barrier{};
        barrier.image = texIt->second.handle;
        barrier.oldLayout = srcLayout;
        barrier.newLayout = trans.dstLayout;
        barrier.srcAccessMask = trans.srcAccess;
        barrier.dstAccessMask = trans.dstAccess;
        barrier.aspectMask = trans.aspectMask;
        barrier.baseMipLevel = 0;
        barrier.levelCount = 1;
        barrier.baseArrayLayer = 0;
        barrier.layerCount = 1;

        encoder->pipelineBarrier(trans.srcStage, trans.dstStage, RHI::DependencyFlags::None, {}, {}, { barrier });

        currentLayouts[trans.texId] = trans.dstLayout;
    }

    RHI::TextureHandle RenderGraph::getPhysicalTextureHandle(TextureId id) const {
        auto it = m_textureMap.find(id);
        return it != m_textureMap.end() ? it->second.handle : RHI::TextureHandle::Null();
    }

    RHI::BufferHandle RenderGraph::getPhysicalBuffer(BufferId id) const {
        auto it = m_bufferMap.find(id);
        return it != m_bufferMap.end() ? it->second : RHI::BufferHandle::Null();
    }

    TextureId RenderGraph::getTextureId(const std::string& name) const {
        auto it = m_nameToTextureId.find(name);
        if (it == m_nameToTextureId.end())
            throw std::runtime_error("Texture not found: " + name);
        return it->second;
    }

    BufferId RenderGraph::getBufferId(const std::string& name) const {
        auto it = m_nameToBufferId.find(name);
        if (it == m_nameToBufferId.end())
            throw std::runtime_error("Buffer not found: " + name);
        return it->second;
    }

    // 动态渲染：取某 pass 在指定交换链图像索引下的颜色附件视图
    const std::vector<void*>& RenderGraph::getAttachmentViewsForPass(size_t passIndex, uint32_t imageIndex) const {
        static const std::vector<void*> empty;
        if (passIndex >= m_perPassViews.size()) return empty;
        if (imageIndex >= m_perPassViews[passIndex].colorViews.size()) return empty;
        return m_perPassViews[passIndex].colorViews[imageIndex];
    }

    // 动态渲染：取某 pass 的深度附件视图（无深度返回 nullptr）
    void* RenderGraph::getDepthAttachmentViewForPass(size_t passIndex, uint32_t /*imageIndex*/) const {
        if (passIndex >= m_perPassViews.size()) return nullptr;
        return m_perPassViews[passIndex].depthView;
    }

    std::pair<RHI::PipelineStage, RHI::AccessFlag> RenderGraph::getStageAccessFromLayout(RHI::ImageLayout layout) {
        switch (layout) {
        case RHI::ImageLayout::Undefined:
            return { RHI::PipelineStage::TopOfPipe, RHI::AccessFlag::None };
        case RHI::ImageLayout::ColorAttachment:
            return { RHI::PipelineStage::ColorAttachmentOutput, RHI::AccessFlag::ColorAttachmentWrite };
        case RHI::ImageLayout::DepthStencilAttachment:
            return { RHI::PipelineStage::EarlyFragmentTests, RHI::AccessFlag::DepthStencilAttachmentWrite };
        case RHI::ImageLayout::ShaderReadOnly:
            return { RHI::PipelineStage::FragmentShader, RHI::AccessFlag::ShaderRead };
        case RHI::ImageLayout::TransferSrc:
            return { RHI::PipelineStage::Transfer, RHI::AccessFlag::TransferRead };
        case RHI::ImageLayout::TransferDst:
            return { RHI::PipelineStage::Transfer, RHI::AccessFlag::TransferWrite };
        case RHI::ImageLayout::PresentSrc:
            return { RHI::PipelineStage::BottomOfPipe, RHI::AccessFlag::None };
        default:
            return { RHI::PipelineStage::AllCommands,
                     RHI::AccessFlag::MemoryRead | RHI::AccessFlag::MemoryWrite };
        }
    }

    std::pair<RHI::PipelineStage, RHI::AccessFlag> RenderGraph::getReadStageAccess(
        RHI::ImageLayout layout, bool computeReader) {
        if (computeReader && layout == RHI::ImageLayout::ShaderReadOnly)
            return { RHI::PipelineStage::ComputeShader, RHI::AccessFlag::ShaderRead };
        return getStageAccessFromLayout(layout);
    }

    // ──── Pass Culling ──────────────────────────────────────────────
    void RenderGraph::cullUnusedPasses() {
        if (m_passes.empty()) return;

        // 1. 找"根"纹理——被导入的外部纹理（如 Swapchain）
        //    这些是管线必须输出的目标。
        std::set<TextureId> rootTexIds;
        for (const auto& vt : m_virtualTextures) {
            if (vt.imported) {
                rootTexIds.insert(vt.id);
            }
        }

        if (rootTexIds.empty()) {
            // 没有任何导入纹理（异常情况），不做裁剪
            LOG_WARN("cullUnusedPasses: No imported textures found, skipping cull");
            return;
        }

        // 2. 找所有写入"根"纹理的 Pass 作为种子
        std::set<GraphNode*> reachable;
        std::queue<GraphNode*> queue;

        for (auto& pass : m_passes) {
            for (auto texId : rootTexIds) {
                if (pass->getWriteTextures().count(texId)) {
                    reachable.insert(pass.get());
                    queue.push(pass.get());
                    break;
                }
            }
        }

        // 3. BFS 反向遍历：可达 Pass 的输入 → 找生产者
        while (!queue.empty()) {
            auto* p = queue.front();
            queue.pop();

            // 纹理：读 → 找写者
            for (auto readTex : p->getReadTextures()) {
                for (auto& other : m_passes) {
                    if (other->getWriteTextures().count(readTex) && !reachable.count(other.get())) {
                        reachable.insert(other.get());
                        queue.push(other.get());
                    }
                }
            }
            // 缓冲：读 → 找写者
            for (auto readBuf : p->getReadBuffers()) {
                for (auto& other : m_passes) {
                    if (other->getWriteBuffers().count(readBuf) && !reachable.count(other.get())) {
                        reachable.insert(other.get());
                        queue.push(other.get());
                    }
                }
            }
        }

        // 4. 移除不可达 Pass
        size_t before = m_passes.size();
        m_passes.erase(
            std::remove_if(m_passes.begin(), m_passes.end(),
                [&reachable](const std::unique_ptr<GraphNode>& p) {
                    return !reachable.count(p.get());
                }),
            m_passes.end());

        size_t after = m_passes.size();
        if (before != after) {
            LOG_INFO("Pass Culling: removed {} / {} passes ({} remaining)",
                before - after, before, after);
        } else {
            LOG_INFO("Pass Culling: all {} passes reachable, nothing culled", after);
        }

        // 5. 重建 m_sortedPasses（因为索引变了）
        dependencyAnalysis();
    }

    // ──── DOT 可视化导出 ────────────────────────────────────────────
    void RenderGraph::exportDot(const std::string& filepath) const {
        std::ofstream f(filepath);
        if (!f.is_open()) {
            LOG_ERROR("exportDot: Cannot open file: {}", filepath);
            return;
        }

        f << "// RenderGraph visualization\n";
        f << "// Orange = Pass, Blue = Texture, Green = Read, Red = Write\n";
        f << "digraph RenderGraph {\n";
        f << "  rankdir=LR;\n";
        f << "  node [fontname=\"Helvetica\"];\n";
        f << "  edge [fontname=\"Helvetica\"];\n\n";

        // 纹理节点（蓝色矩形）
        for (const auto& vt : m_virtualTextures) {
            std::string label = vt.name.empty()
                ? "Tex#" + std::to_string(vt.id.id())
                : vt.name;
            std::string shape = vt.imported ? "box, style=filled, fillcolor=lightcyan"
                                            : "box, style=filled, fillcolor=lightblue";
            f << "  tex_" << vt.id.id() << " [label=\"" << label
              << "\", shape=" << shape << "];\n";
        }

        // 缓冲节点（黄色）
        for (const auto& vb : m_virtualBuffers) {
            std::string label = vb.name.empty()
                ? "Buf#" + std::to_string(vb.id.id())
                : vb.name;
            f << "  buf_" << vb.id.id() << " [label=\"" << label
              << "\", shape=box, style=filled, fillcolor=lightyellow];\n";
        }

        // Pass 节点（橙色椭圆）+ 数据边
        for (const auto& pass : m_passes) {
            std::string passId = "pass_" + pass->getName();
            f << "  " << passId << " [label=\"" << pass->getName()
              << "\", shape=ellipse, style=filled, fillcolor=orange";

            if (!pass->isEnabled()) {
                f << ", fontcolor=gray, color=gray";  // 禁用的 Pass 灰色
            }
            f << "];\n";

            // 读边（绿色）
            for (auto tex : pass->getReadTextures()) {
                f << "  tex_" << tex.id() << " -> " << passId
                  << " [color=darkgreen, penwidth=1.5];\n";
            }
            // 写边（红色）
            for (auto tex : pass->getWriteTextures()) {
                f << "  " << passId << " -> tex_" << tex.id()
                  << " [color=darkred, penwidth=1.5];\n";
            }
            // 缓冲读边（绿色虚线）
            for (auto buf : pass->getReadBuffers()) {
                f << "  buf_" << buf.id() << " -> " << passId
                  << " [color=darkgreen, style=dashed];\n";
            }
            // 缓冲写边（红色虚线）
            for (auto buf : pass->getWriteBuffers()) {
                f << "  " << passId << " -> buf_" << buf.id()
                  << " [color=darkred, style=dashed];\n";
            }
        }

        f << "}\n";
        f.close();
        LOG_INFO("Exported render graph DOT to: {}", filepath);
    }

    // ──── Memory Aliasing────────────
    void RenderGraph::performMemoryAliasing(
        const std::unordered_map<TextureId, TexturePassInfo>& texPassInfo)
    {
        // 收集 transient 纹理及其生命周期
        struct Lifetime {
            TextureId texId;
            uint32_t firstUse;
            uint32_t lastUse;
            uint64_t sizeBytes;
        };
        std::vector<Lifetime> lifetimes;

        for (const auto& vt : m_virtualTextures) {
            if (vt.imported) continue;  // 外部纹理不管
            auto it = texPassInfo.find(vt.id);
            if (it == texPassInfo.end() || it->second.firstUserIndex == -1) continue;  // 未使用

            uint64_t size = vt.desc.extent.width * vt.desc.extent.height
                          * vt.desc.arrayLayers * 8;  // ~RGBA16F×2 上限估计
            lifetimes.push_back({
                vt.id,
                static_cast<uint32_t>(it->second.firstUserIndex),
                static_cast<uint32_t>(it->second.lastUserIndex),
                size
            });
        }

        if (lifetimes.size() < 2) return;  // 少于 2 个 transient 不用别名

        // 按首次使用排序
        std::sort(lifetimes.begin(), lifetimes.end(),
            [](const Lifetime& a, const Lifetime& b) { return a.firstUse < b.firstUse; });

        // 空闲池：{texId, sizeBytes, freedAtPass}
        struct FreeSlot {
            TextureId texId;
            uint64_t sizeBytes;
            uint32_t freedAtPass;
        };
        std::vector<FreeSlot> freePool;

        size_t aliasCount = 0;
        uint64_t savedBytes = 0;

        for (auto& lt : lifetimes) {
            // 回收在当前 Pass 之前已过期的纹理到空闲池
            for (const auto& vt : m_virtualTextures) {
                if (vt.imported) continue;
                auto fi = texPassInfo.find(vt.id);
                if (fi == texPassInfo.end()) continue;
                if (fi->second.lastUserIndex >= 0 &&
                    static_cast<uint32_t>(fi->second.lastUserIndex) < lt.firstUse) {
                    // 检查是否已在池中
                    bool inPool = false;
                    for (auto& fs : freePool) {
                        if (fs.texId == vt.id) { inPool = true; break; }
                    }
                    if (!inPool) {
                        uint64_t sz = vt.desc.extent.width * vt.desc.extent.height
                                    * vt.desc.arrayLayers * 8;
                        freePool.push_back({vt.id, sz,
                            static_cast<uint32_t>(fi->second.lastUserIndex)});
                    }
                }
            }

            // 尝试从空闲池找到尺寸足够大的纹理来别名
            for (auto& fs : freePool) {
                if (fs.sizeBytes >= lt.sizeBytes && fs.texId != lt.texId) {
                    // 别名：令当前纹理复用已过期纹理的物理 handle
                    auto donorIt = m_textureMap.find(fs.texId);
                    auto targetIt = m_textureMap.find(lt.texId);
                    if (donorIt != m_textureMap.end() && targetIt != m_textureMap.end()) {
                        // 先释放目标纹理的物理资源（还未实际分配时跳过）
                        if (targetIt->second.handle.isValid() &&
                            targetIt->second.handle != donorIt->second.handle) {
                            m_resMgr->destroy(targetIt->second.handle);
                        }
                        targetIt->second.handle = donorIt->second.handle;
                        targetIt->second.views = donorIt->second.views;
                        aliasCount++;
                        savedBytes += lt.sizeBytes;
                    }
                    // 从池中移除已使用的 slot
                    fs = freePool.back();
                    freePool.pop_back();
                    break;
                }
            }
        }

        if (aliasCount > 0) {
            LOG_INFO("Memory Aliasing: {} textures aliased, saved ~{:.1f} MB",
                aliasCount, savedBytes / 1048576.0f);
        }
    }

} // namespace StarryEngine::RenderGraph