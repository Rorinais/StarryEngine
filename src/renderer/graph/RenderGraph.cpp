#include "RenderGraph.hpp"
#include <queue>
#include <stack>
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace StarryEngine::RenderGraph {

    RenderGraph::RenderGraph(std::shared_ptr<RHI::IRHI> rhi)
        : m_rhi(rhi), m_resMgr(rhi->getResourceManager()) {
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

    RHI::TextureDesc RenderGraph::createBaseTextureDesc(
        uint32_t width, uint32_t height,
        RHI::Format format,
        bool allowDepthStencil,
        bool allowRenderTarget,
        bool allowInputAttachment,
        RHI::TextureType type
    ) {
        RHI::TextureDesc desc;
        desc.extent = { width, height, 1 };
        desc.format = format;
        desc.type = type;
        desc.allowRenderTarget = allowRenderTarget;
        desc.allowDepthStencil = allowDepthStencil;
        desc.allowInputAttachment = allowInputAttachment;
        return desc;
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

    //注：ai注释
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

        //根据纹理的读写关系建立依赖
        for (const auto& [tex, writers] : texWriters) {
            auto& readers = texReaders[tex];
            // 写后读：写 Pass 必须在读 Pass 之前
            for (uint32_t w : writers) {
                for (uint32_t r : readers) {
                    if (w != r && writers.find(r) == writers.end()) {
                        addDependency(w, r);
                    }
                }
            }
            // 写后写：多个写 Pass 必须按顺序执行（通常需要）
            std::vector<uint32_t> wlist(writers.begin(), writers.end());
            std::sort(wlist.begin(), wlist.end());
            for (size_t i = 0; i + 1 < wlist.size(); ++i) {
                addDependency(wlist[i], wlist[i + 1]);
            }
        }

        //根据缓冲区的读写关系建立依赖（逻辑同上）
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

        // 拓扑排序得到 Pass 的执行顺序
        auto order = topologicalSort(adj);
        m_sortedPasses.clear();
        for (uint32_t idx : order) {
            m_sortedPasses.push_back(m_passes[idx].get());
        }
    }

    bool RenderGraph::compile() {
        dependencyAnalysis();

        // 分析纹理的读写 Pass 索引，用于自动插入子通道依赖（例如从上次写到第一次读）
        struct TexturePassInfo {
            int32_t lastWriterIndex = -1;               // 最后一个写入该纹理的 Pass 索引
            int32_t firstReaderIndex = -1;              // 第一个读取该纹理的 Pass 索引
            RHI::PipelineStageFlags writeStage;          // 写阶段（暂未使用，可扩展）
            RHI::AccessFlags writeAccess;                 // 写访问掩码
            RHI::PipelineStageFlags readStage;            // 读阶段
            RHI::AccessFlags readAccess;                  // 读访问掩码
        };

        std::unordered_map<TextureId, TexturePassInfo> texPassInfo;

        // 遍历排序后的 Pass，记录每个纹理的最后写入和首次读取
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

        // 如果存在从写入到读取的跨 Pass 依赖，添加一个子通道依赖（从外部到第一个读 Pass 的子通道）
        for (const auto& [tex, info] : texPassInfo) {
            if (info.lastWriterIndex != -1 && info.firstReaderIndex != -1 && info.firstReaderIndex > info.lastWriterIndex) {
                RHI::SubpassDependency dep{};
                dep.srcSubpass = SUBPASS_EXTERNAL;                // 外部（即上一个 Pass）
                dep.dstSubpass = 0;                                // 第一个子通道
                dep.srcStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::AllGraphics);
                dep.dstStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::FragmentShader);
                dep.srcAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::MemoryWrite);
                dep.dstAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::InputAttachmentRead);
                dep.byRegion = true;                               // 按区域依赖
                m_sortedPasses[info.firstReaderIndex]->addDependency(dep);
            }
        }

        // 为每个虚拟纹理创建/获取物理纹理，建立 ID 到物理信息的映射
        for (auto& vt : m_virtualTextures) {
            if (vt.imported) {                                     // 外部导入的纹理（如交换链）
                PhysicalTextureInfo info;
                info.handle = vt.externalHandle;
                info.views = vt.externalViews;
                m_textureMap[vt.id] = info;
            }
            else {                                                // 需要创建新纹理
                RHI::TextureHandle handle = m_resMgr->createTexture(vt.desc);
                if (!handle.isValid()) {
                    throw std::runtime_error("Failed to create physical texture: " + vt.name);
                }
                auto* texObj = m_resMgr->getTexture(handle);
                PhysicalTextureInfo info;
                info.handle = handle;
                info.views.push_back(texObj->getDefaultView());    // 默认视图（通常是第一个 mip/层）
                m_textureMap[vt.id] = info;
            }
        }

        //为每个虚拟缓冲区创建/获取物理缓冲区
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

        // 构建纹理描述映射（可能供 Pass 编译时使用）
        std::unordered_map<TextureId, RHI::TextureDesc> texDescMap;
        for (const auto& vt : m_virtualTextures) {
            texDescMap[vt.id] = vt.desc;
        }

        // 编译每个 Pass（例如创建 RenderPass 对象、管线等）
        for (auto* pass : m_sortedPasses) {
            if (!pass->compile(m_resMgr, m_textureMap, texDescMap, m_bufferMap)) {
                throw std::runtime_error("Failed to compile pass: " + pass->getName());
            }
        }

        createFrameBuffer();

        struct PassLayoutInfo {
            RHI::ImageLayout initial;
            RHI::ImageLayout final;
            bool used = false;
        };
        std::unordered_map<TextureId, std::vector<PassLayoutInfo>> texPassLayouts;
        texPassLayouts.reserve(m_virtualTextures.size());

        // 收集每个 PassNode 中使用的纹理及其布局
        for (size_t i = 0; i < m_sortedPasses.size(); ++i) {
            auto* pass = m_sortedPasses[i];
            // 获取该 Pass 中所有用到的纹理（读+写）
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

        // 生成跨 Pass 转换
        m_layoutTransitions.clear();
        for (const auto& [texId, layouts] : texPassLayouts) {
            int32_t lastWritePass = -1;
            RHI::ImageLayout lastWriteLayout = RHI::ImageLayout::Undefined;
            for (size_t i = 0; i < layouts.size(); ++i) {
                const auto& info = layouts[i];
                if (!info.used) continue;
                // 如果当前 Pass 是写入（final 有效），则记录为写入 Pass
                if (info.final != RHI::ImageLayout::Undefined) {
                    // 如果有上一个写入 Pass 且与当前写入 Pass 不同，但这里我们更关心写入到下一个读取的转换
                    // 暂时忽略写-写转换，只处理写后读
                    lastWritePass = static_cast<int32_t>(i);
                    lastWriteLayout = info.final;
                }
                // 如果是读取（initial 有效），且存在之前的写入 Pass，且写入 Pass 与当前读取 Pass 不同，则添加转换
                if (info.initial != RHI::ImageLayout::Undefined && lastWritePass != -1 && lastWritePass != static_cast<int32_t>(i)) {
                    // 如果写入布局与读取初始布局不同，则需要转换
                    if (lastWriteLayout != info.initial) {
                        m_layoutTransitions.push_back({
                            static_cast<uint32_t>(lastWritePass),
                            static_cast<uint32_t>(i),
                            texId,
                            lastWriteLayout,
                            info.initial
                            });
                    }
                    // 重置，避免同一个写入被多个读取重复使用（可根据需求调整）
                    lastWritePass = -1;
                    lastWriteLayout = RHI::ImageLayout::Undefined;
                }
            }
        }

        // 按目标 Pass 索引排序，便于执行时顺序处理
        std::sort(m_layoutTransitions.begin(), m_layoutTransitions.end(),
            [](const LayoutTransition& a, const LayoutTransition& b) {
                if (a.dstPassIdx != b.dstPassIdx) return a.dstPassIdx < b.dstPassIdx;
                return a.srcPassIdx < b.srcPassIdx;
            });

        return true;
    }

    void RenderGraph::createFrameBuffer() {
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

            // 对每个交换链图像索引（或帧索引）创建对应的帧缓冲
            for (uint32_t imgIdx = 0; imgIdx < m_swapchainImageCount; ++imgIdx) {
                std::vector<void*> attachments;                       // 附件视图列表
                for (const auto& key : attachmentKeys) {
                    TextureId texId = pass->getTextureIdForAttachmentKey(key);
                    auto it = m_textureMap.find(texId);
                    if (it == m_textureMap.end()) {
                        throw std::runtime_error("Texture not found for key: " + key);
                    }
                    const auto& texInfo = it->second;
                    void* view = nullptr;
                    if (texInfo.views.size() == 1) {                  // 单视图纹理（如深度、颜色）
                        view = texInfo.views[0];
                    }
                    else if (texInfo.views.size() > 1) {            // 多视图纹理（如交换链，每个图像一个视图）
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
    }

    void RenderGraph::execute(RHI::RHICommandEncoder* encoder, const RenderContext& context, uint32_t frameIndex) {
        if (m_sortedPasses.empty()) return;
        if (m_perPassFramebuffers.size() != m_sortedPasses.size()) {
            throw std::runtime_error("Framebuffer count mismatch in RenderGraph");
        }

        // 维护当前布局状态（可选，用于连续多个 Pass 共享同一纹理的布局）
        std::unordered_map<TextureId, RHI::ImageLayout> currentLayouts;
        // 初始化外部导入纹理的布局
        for (const auto& vt : m_virtualTextures) {
            if (vt.imported) {
                currentLayouts[vt.id] = vt.initialLayout;
            }
        }

        // 用于遍历转换列表的迭代器
        auto transIt = m_layoutTransitions.begin();
        for (size_t i = 0; i < m_sortedPasses.size(); ++i) {
            auto* pass = m_sortedPasses[i];
            RHI::FramebufferHandle fb = m_perPassFramebuffers[i][frameIndex];

            while (transIt != m_layoutTransitions.end() && transIt->dstPassIdx == i) {
                const auto& trans = *transIt;
                auto texIt = m_textureMap.find(trans.texId);
                if (texIt != m_textureMap.end() && texIt->second.handle.isValid()) {
                    // 确定源布局（可能已被之前的屏障更新）
                    RHI::ImageLayout srcLayout = trans.srcLayout;
                    auto curIt = currentLayouts.find(trans.texId);
                    if (curIt != currentLayouts.end() && curIt->second != RHI::ImageLayout::Undefined) {
                        srcLayout = curIt->second;
                    }

                    // 构建图像屏障（不包含阶段掩码）
                    RHI::ImageBarrier barrier{};
                    barrier.image = texIt->second.handle;
                    barrier.oldLayout = srcLayout;
                    barrier.newLayout = trans.dstLayout;
                    barrier.srcAccessMask = RHI::FUNC::layoutToAccessMask(srcLayout);
                    barrier.dstAccessMask = RHI::FUNC::layoutToAccessMask(trans.dstLayout);
                    barrier.aspectMask = RHI::ImageAspect::Color;
                    barrier.baseMipLevel = 0;
                    barrier.levelCount = 1;
                    barrier.baseArrayLayer = 0;
                    barrier.layerCount = 1;

                    // 推导阶段掩码（作为 pipelineBarrier 的参数）
                    RHI::PipelineStage srcStage = RHI::FUNC::layoutToSrcStage(srcLayout);
                    RHI::PipelineStage dstStage = RHI::FUNC::layoutToSrcStage(trans.dstLayout); // 注意：目标阶段可能不同，可根据新布局调整
                    // 如果新布局是 ShaderReadOnly，目标阶段应为 FragmentShader
                    if (trans.dstLayout == RHI::ImageLayout::ShaderReadOnly) {
                        dstStage = RHI::PipelineStage::FragmentShader;
                    }
                    LOG_INFO("Inserting barrier for texId {} before pass {}", trans.texId.id(), i);
                    // 插入屏障
                    encoder->pipelineBarrier(
                        srcStage,                           // 源阶段
                        dstStage,                           // 目标阶段
                        RHI::DependencyFlags::None,         // 依赖标志（0）
                        {},                                 // 内存屏障（无）
                        {},                                 // 缓冲区屏障（无）
                        { barrier }                         // 图像屏障列表
                    );

                    // 更新当前布局
                    currentLayouts[trans.texId] = trans.dstLayout;
                }
                ++transIt;
            }

            for (const auto& trans : m_layoutTransitions) {
                LOG_INFO("Transition: srcPass={}, dstPass={}, texId={}, srcLayout={}, dstLayout={}",
                    trans.srcPassIdx, trans.dstPassIdx, trans.texId.id(),
                    static_cast<int>(trans.srcLayout), static_cast<int>(trans.dstLayout));
            }

            // 执行当前 PassNode
            pass->execute(encoder, context,frameIndex, fb);

            // 执行后，更新当前布局：对于该 Pass 中写入的纹理，布局变为 finalLayout
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

    const std::vector<RHI::FramebufferHandle>& RenderGraph::getFramebuffersForPass(size_t passIndex) const {
        if (passIndex >= m_perPassFramebuffers.size())
            throw std::runtime_error("Invalid pass index");
        return m_perPassFramebuffers[passIndex];
    }

} // namespace StarryEngine::RenderGraph