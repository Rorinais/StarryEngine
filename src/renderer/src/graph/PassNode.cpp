#include <renderer/graph/PassNode.hpp>
#include <stdexcept>
#include <iostream>

namespace StarryEngine::RenderGraph {

    // 颜色附件参数是否一致（用于共享节点内复用同一个附件）
    static bool sameColorAttachmentParams(const AttachmentParams& a, const AttachmentParams& b) {
        auto colorEq = [](const std::optional<RHI::Color>& x, const std::optional<RHI::Color>& y) {
            if (x.has_value() != y.has_value()) return false;
            if (!x.has_value()) return true;
            return x->r == y->r && x->g == y->g && x->b == y->b && x->a == y->a;
        };
        return a.format == b.format && a.loadOp == b.loadOp && a.storeOp == b.storeOp &&
               a.initialLayout == b.initialLayout && a.finalLayout == b.finalLayout &&
               colorEq(a.clearColor, b.clearColor);
    }

    PassNode::PassNode(const std::string& name, PassType type)
        : m_name(name), m_builder(name) {
        m_type = type;
    }

    PassNode::~PassNode() {
        if (m_resMgr) {
            if (m_renderPassHandle.isValid()) {
                m_resMgr->destroy(m_renderPassHandle);
            }
        }
    }

    std::string PassNode::addColorOutput(TextureId texId, const AttachmentParams& params) {
        // 同一纹理 + 相同附件参数 → 复用已有颜色附件（多个 subpass 共享一个附件，避免重复附件/清屏）
        if (texId.isValid()) {
            auto it = m_colorOutputKeyByTex.find(texId);
            if (it != m_colorOutputKeyByTex.end()) {
                auto prevIt = m_keyToParams.find(it->second);
                if (prevIt != m_keyToParams.end() && sameColorAttachmentParams(prevIt->second, params)) {
                    return it->second;
                }
            }
        }

        std::string key = "auto_color_" + std::to_string(m_nextAttachmentKey++);
        m_builder.registerColorAttachment(key,
            params.format.value_or(RHI::Format::RGBA8_UNorm),
            params.finalLayout.value_or(RHI::ImageLayout::ColorAttachment),
            params.loadOp.value_or(RHI::AttachmentLoadOp::Clear),
            params.storeOp.value_or(RHI::AttachmentStoreOp::Store),
            params.initialLayout.value_or(RHI::ImageLayout::Undefined));
        m_keyToTexId[key] = texId;
        m_writeTextures.insert(texId);
        if (params.clearColor.has_value()) {
            m_clearValueMap[key] = RHI::ClearValue(params.clearColor.value());
        }
        m_keyToParams[key] = params;
        m_colorOutputKeyByTex[texId] = key;
        return key;
    }

    std::string PassNode::addDepthOutput(TextureId texId, const AttachmentParams& params) {
        std::string key = "auto_depth_" + std::to_string(m_nextAttachmentKey++);
        m_builder.registerDepthAttachment(key,
            params.format.value_or(RHI::Format::D32_Float),
            params.loadOp.value_or(RHI::AttachmentLoadOp::Clear),
            params.storeOp.value_or(RHI::AttachmentStoreOp::DontCare),
            params.initialLayout.value_or(RHI::ImageLayout::Undefined),
            params.finalLayout.value_or(RHI::ImageLayout::DepthStencilAttachment));
        m_keyToTexId[key] = texId;
        m_writeTextures.insert(texId);
        m_depthOutputKeyByTex[texId] = key;
        if (params.clearDepth.has_value()) {
            m_clearValueMap[key] = RHI::ClearValue(params.clearDepth.value(),
                params.clearStencil.value_or(0));
        }
        m_keyToParams[key] = params;
        return key;
    }

    std::string PassNode::addInput(TextureId texId, const AttachmentParams& params) {
        std::string key = "auto_input_" + std::to_string(m_nextAttachmentKey++);
        m_builder.registerInputAttachment(key,
            params.format.value_or(RHI::Format::RGBA8_UNorm),
            params.finalLayout.value_or(RHI::ImageLayout::ShaderReadOnly),
            params.initialLayout.value_or(RHI::ImageLayout::Undefined),
            params.loadOp.value_or(RHI::AttachmentLoadOp::Load),
            params.storeOp.value_or(RHI::AttachmentStoreOp::Store));
        m_keyToTexId[key] = texId;
        m_readTextures.insert(texId);
        m_keyToParams[key] = params;
        return key;
    }

    std::string PassNode::addResolve(TextureId texId, const AttachmentParams& params) {
        std::string key = "auto_resolve_" + std::to_string(m_nextAttachmentKey++);
        m_builder.registerResolveAttachment(key,
            params.format.value_or(RHI::Format::RGBA8_UNorm),
            params.finalLayout.value_or(RHI::ImageLayout::ColorAttachment));
        m_keyToTexId[key] = texId;
        m_writeTextures.insert(texId);
        m_keyToParams[key] = params;
        return key;
    }

    std::string PassNode::addPreserve(TextureId texId) {
        std::string key = "auto_preserve_" + std::to_string(m_nextAttachmentKey++);
        m_keyToTexId[key] = texId;
        return key;
    }

    SubpassBuilder& PassNode::addSubpass(const std::string& subpassName) {
        return m_builder.addSubpass(SubpassBuilder(subpassName));
    }

    bool PassNode::compile(std::shared_ptr<RHI::ResourceManager> resMgr,
        const std::unordered_map<TextureId, PhysicalTextureInfo>& texMap,
        const std::unordered_map<TextureId, RHI::TextureDesc>& texDescMap,
        const std::unordered_map<BufferId, RHI::BufferHandle>& /*bufMap*/) {
        m_resMgr = resMgr;

        // ── Compute Pass：无需 RenderPass/Framebuffer ──
        if (m_type == PassType::Compute) {
            m_finalLayouts.clear();
            for (auto& [texId, layout] : m_computeWriteLayouts) {
                m_finalLayouts[texId] = layout;
            }
            return true;
        }

        const auto& attachmentIndices = m_builder.getAttachmentIndices();
        for (const auto& [key, texId] : m_keyToTexId) {
            auto descIt = texDescMap.find(texId);
            if (descIt == texDescMap.end()) {
                throw std::runtime_error("Texture description not found for texId");
            }
            RHI::Format actualFormat = descIt->second.format;
            auto indexIt = attachmentIndices.find(key);
            if (indexIt != attachmentIndices.end()) {
                m_builder.updateAttachmentFormat(indexIt->second, actualFormat);
            }
        }

        auto buildResult = m_builder.build(true);
        m_cachedBuildResult = std::move(buildResult);

        // 在 PassNode::compile 中，构建完 m_cachedBuildResult 后
        for (size_t i = 0; i < m_cachedBuildResult->renderPassDesc.attachments.size(); ++i) {
            const auto& att = m_cachedBuildResult->renderPassDesc.attachments[i];
            const std::string& name = m_cachedBuildResult->attachmentNames[i];
            LOG_INFO("Attachment[{}] name={}, format={}, loadOp={}, storeOp={}, initialLayout={}, finalLayout={}",
                i, name, static_cast<int>(att.format),
                static_cast<int>(att.loadOp), static_cast<int>(att.storeOp),
                static_cast<int>(att.initialLayout), static_cast<int>(att.finalLayout));
        }

        // 根据 buildResult 中的附件名称，建立 key -> 纹理 ID 映射（用于执行时获取纹理）
        for (const auto& key : m_cachedBuildResult->attachmentNames) {
            auto it = m_keyToTexId.find(key);
            if (it == m_keyToTexId.end()) {
                throw std::runtime_error("Attachment key not found: " + key);
            }
            m_attachmentKeyToTexId[key] = it->second;
        }

        for (const auto& [key, params] : m_keyToParams) {
            auto texIt = texDescMap.find(m_keyToTexId[key]);
            if (texIt == texDescMap.end()) continue;
            bool isDepth = isDepthFormat(texIt->second.format);
            if (isDepth && params.clearColor.has_value()) {
                LOG_WARN("Depth attachment '{}' has clear color, ignored", key);
            }
            if (!isDepth && (params.clearDepth.has_value() || params.clearStencil.has_value())) {
                LOG_WARN("Color attachment '{}' has clear depth/stencil, ignored", key);
            }
        }

        m_renderPassHandle = resMgr->createRenderPass(m_cachedBuildResult->renderPassDesc, m_name);
        if (!m_renderPassHandle.isValid()) {
            throw std::runtime_error("Failed to create RenderPass: " + m_name);
        }

        // 计算每个纹理在 PassNode 中的最终布局
        m_finalLayouts.clear();
        const auto& subpasses = m_cachedBuildResult->renderPassDesc.subpasses;
        const auto& attachments = m_cachedBuildResult->renderPassDesc.attachments;

        struct LastUsage {
            uint32_t subpassIdx;
            uint32_t attachmentIdx;
        };
        std::unordered_map<TextureId, LastUsage> lastUsage;

        // 按子通道顺序遍历，记录每个纹理最后一次出现的附件
        for (uint32_t subpassIdx = 0; subpassIdx < subpasses.size(); ++subpassIdx) {
            const auto& subpass = subpasses[subpassIdx];

            // 颜色附件（写入）
            for (const auto& ref : subpass.colorAttachments) {
                if (ref.attachment == ATTACHMENT_UNUSED) continue;
                const auto& key = m_cachedBuildResult->attachmentNames[ref.attachment];
                auto texIdIt = m_attachmentKeyToTexId.find(key);
                if (texIdIt != m_attachmentKeyToTexId.end()) {
                    lastUsage[texIdIt->second] = { subpassIdx, ref.attachment };
                }
            }

            // 深度附件（写入）
            if (subpass.depthStencilAttachment.attachment != ATTACHMENT_UNUSED) {
                const auto& key = m_cachedBuildResult->attachmentNames[subpass.depthStencilAttachment.attachment];
                auto texIdIt = m_attachmentKeyToTexId.find(key);
                if (texIdIt != m_attachmentKeyToTexId.end()) {
                    lastUsage[texIdIt->second] = { subpassIdx, subpass.depthStencilAttachment.attachment };
                }
            }

            // 解析附件（写入）
            for (const auto& ref : subpass.resolveAttachments) {
                if (ref.attachment == ATTACHMENT_UNUSED) continue;
                const auto& key = m_cachedBuildResult->attachmentNames[ref.attachment];
                auto texIdIt = m_attachmentKeyToTexId.find(key);
                if (texIdIt != m_attachmentKeyToTexId.end()) {
                    lastUsage[texIdIt->second] = { subpassIdx, ref.attachment };
                }
            }

            // 输入附件（读取）
            for (const auto& ref : subpass.inputAttachments) {
                if (ref.attachment == ATTACHMENT_UNUSED) continue;
                const auto& key = m_cachedBuildResult->attachmentNames[ref.attachment];
                auto texIdIt = m_attachmentKeyToTexId.find(key);
                if (texIdIt != m_attachmentKeyToTexId.end()) {
                    lastUsage[texIdIt->second] = { subpassIdx, ref.attachment };
                }
            }
        }

        // 根据最后一次使用确定最终布局
        for (const auto& [texId, usage] : lastUsage) {
            const auto& att = attachments[usage.attachmentIdx];
            m_finalLayouts[texId] = att.finalLayout;
        }

        m_passExecutors = m_cachedBuildResult->passExecutors;

        m_clearValues.clear();
        for (const auto& key : m_cachedBuildResult->attachmentNames) {
            auto it = m_clearValueMap.find(key);
            if (it != m_clearValueMap.end()) {
                m_clearValues.push_back(it->second);
            }
            else {
                RHI::ClearValue defaultCV;
                defaultCV.color = { 0.0f, 0.0f, 0.0f, 1.0f };
                defaultCV.depth = 1.0f;
                defaultCV.stencil = 0;
                m_clearValues.push_back(defaultCV);
            }
        }

        return true;
    }

    void PassNode::execute(RHI::RHICommandEncoder* encoder,const RenderContext& context,uint32_t frameIndex,RHI::FramebufferHandle framebuffer,uint32_t frameSlot) {
        if (m_type == PassType::Compute) {
            // ── Compute Pass ── 执行全部交给 executor（绑管线 + 描述符 + push + dispatch）
            if (!m_enabled) return;
            if (m_computeRecorder) {
                m_computeRecorder->execute(encoder, context,PassContext(m_resMgr, frameIndex, {}, frameSlot), 0);
            }
            return;
        }

        // ── Graphics Pass ──
        if (!m_renderPassHandle.isValid()) throw std::runtime_error("Pass not compiled: " + m_name);

        auto* renderPassObj = m_resMgr->getRenderPass(m_renderPassHandle);
        auto* fbObj = m_resMgr->getFramebuffer(framebuffer);
        if (!renderPassObj || !fbObj) {LOG_WARN("[{}] renderPassObj={} fbObj={} — skipping pass",m_name, (void*)renderPassObj, (void*)fbObj);
            return;
        }

        RHI::RenderPassBeginInfo beginInfo{
            .renderPass = m_renderPassHandle,
            .framebuffer = framebuffer,
            .renderArea = { {0, 0}, { m_width, m_height } },
            .clearValues = m_clearValues
        };

        encoder->beginRenderPass(beginInfo, RHI::SubpassContents::Inline);

        if (m_enabled) {
            encoder->setViewport({ 0.0f, 0.0f, (float)m_width, (float)m_height, 0.0f, 1.0f });
            encoder->setScissor({ {0, 0}, {m_width, m_height} });

            for (uint32_t i = 0; i < m_passExecutors.size(); ++i) {
                if (i > 0) encoder->nextSubpass(RHI::SubpassContents::Inline);
                if (m_passExecutors[i]) {
                    m_passExecutors[i]->execute(encoder, context, PassContext(m_resMgr, frameIndex, framebuffer, frameSlot), i);
                }
            }
        }

        encoder->endRenderPass();
    }

    // ── 并行命令录制：主缓冲侧（渲染通道框架）──────────────────────────────

    void PassNode::beginPassOnPrimary(RHI::RHICommandEncoder* encoder,
        RHI::FramebufferHandle framebuffer,
        RHI::SubpassContents contents) {
        if (!m_renderPassHandle.isValid()) throw std::runtime_error("Pass not compiled: " + m_name);
        auto* renderPassObj = m_resMgr->getRenderPass(m_renderPassHandle);
        auto* fbObj = m_resMgr->getFramebuffer(framebuffer);
        if (!renderPassObj || !fbObj) {
            LOG_WARN("[{}] beginPassOnPrimary: renderPassObj={} fbObj={} — skipping pass", m_name,
                (void*)renderPassObj, (void*)fbObj);
            return;
        }

        RHI::RenderPassBeginInfo beginInfo{
            .renderPass = m_renderPassHandle,
            .framebuffer = framebuffer,
            .renderArea = { {0, 0}, { m_width, m_height } },
            .clearValues = m_clearValues
        };
        encoder->beginRenderPass(beginInfo, contents);
    }

    void PassNode::nextSubpassOnPrimary(RHI::RHICommandEncoder* encoder, RHI::SubpassContents contents) {
        encoder->nextSubpass(contents);
    }

    void PassNode::endPassOnPrimary(RHI::RHICommandEncoder* encoder) {
        encoder->endRenderPass();
    }

    // ── 并行命令录制：次缓冲侧（pass 主体）──────────────────────────────
    // 只录 viewport/scissor + 该 subpass 的 executor（或 compute recorder）。
    // 不含 barrier、不含 begin/endRenderPass——那些必须在主缓冲（Vulkan 能力约束）。

    void PassNode::recordBody(RHI::RHICommandEncoder* encoder,
        const RenderContext& context,
        uint32_t frameIndex,
        RHI::FramebufferHandle framebuffer,
        uint32_t subpassIndex,
        uint32_t frameSlot) {
        if (m_type == PassType::Compute) {
            if (m_enabled && m_computeRecorder) {
                m_computeRecorder->execute(encoder, context, PassContext(m_resMgr, frameIndex, {}, frameSlot), 0);
            }
            return;
        }

        if (!m_enabled) return;
        encoder->setViewport({ 0.0f, 0.0f, (float)m_width, (float)m_height, 0.0f, 1.0f });
        encoder->setScissor({ {0, 0}, {m_width, m_height} });

        if (subpassIndex < m_passExecutors.size() && m_passExecutors[subpassIndex]) {
            m_passExecutors[subpassIndex]->execute(encoder, context,
                PassContext(m_resMgr, frameIndex, framebuffer, frameSlot), subpassIndex);
        }
    }

    const std::vector<std::string>& PassNode::getAttachmentNames() const {
        if (!m_cachedBuildResult) {
            throw std::runtime_error("PassNode not built yet: " + m_name);
        }
        return m_cachedBuildResult->attachmentNames;
    }

    const std::vector<std::string>& PassNode::getColorAttachmentNames() const {
        // 动态视图收集仅在动态模式调用（PassNode 传统模式不参与），延迟缓存到成员避免 static 竞争
        m_colorKeysCache.clear();
        m_colorKeysCache.reserve(m_colorOutputKeyByTex.size());
        for (const auto& [texId, key] : m_colorOutputKeyByTex) m_colorKeysCache.push_back(key);
        return m_colorKeysCache;
    }

    const std::vector<std::string>& PassNode::getDepthAttachmentNames() const {
        m_depthKeysCache.clear();
        m_depthKeysCache.reserve(m_depthOutputKeyByTex.size());
        for (const auto& [texId, key] : m_depthOutputKeyByTex) m_depthKeysCache.push_back(key);
        return m_depthKeysCache;
    }

    TextureId PassNode::getTextureIdForAttachmentKey(const std::string& key) const {
        auto it = m_attachmentKeyToTexId.find(key);
        if (it == m_attachmentKeyToTexId.end()) {
            throw std::runtime_error("Attachment key not found: " + key);
        }
        return it->second;
    }

    void PassNode::addDependency(const RHI::SubpassDependency& dep) {
        m_builder.addDependency(dep);
    }



    bool PassNode::isDepthFormat(RHI::Format format) {
        switch (format) {
        case RHI::Format::D16_UNorm:
        case RHI::Format::D32_Float:
        case RHI::Format::D24_UNorm_S8_UInt:
        case RHI::Format::D32_Float_S8_UInt:
            return true;
        default:
            return false;
        }
    }

    // 声明式附件推断：只填未显式指定的字段（显式指定的一律保留）。
    // 颜色首写→Clear/Undefined/ShaderReadOnly；后续→Load/ShaderReadOnly/ShaderReadOnly。
    // 深度首写→Clear/Undefined/DepthStencilAttachment；后续→Load/DepthStencilAttachment/DepthStencilAttachment。
    void PassNode::resolveInferredAttachments(const std::function<bool(TextureId)>& isFirstWriter) {
        if (m_type != PassType::Graphics) return;

        auto infer = [&](TextureId texId, const std::string& key, bool isDepth) {
            auto it = m_keyToParams.find(key);
            if (it == m_keyToParams.end()) return;
            auto& params = it->second;
            bool first = isFirstWriter(texId);

            RHI::AttachmentLoadOp loadOp = params.loadOp.value_or(
                first ? RHI::AttachmentLoadOp::Clear : RHI::AttachmentLoadOp::Load);
            RHI::AttachmentStoreOp storeOp = params.storeOp.value_or(RHI::AttachmentStoreOp::Store);
            RHI::ImageLayout initial = params.initialLayout.value_or(
                first ? RHI::ImageLayout::Undefined
                      : (isDepth ? RHI::ImageLayout::DepthStencilAttachment : RHI::ImageLayout::ShaderReadOnly));
            RHI::ImageLayout final = params.finalLayout.value_or(
                isDepth ? RHI::ImageLayout::DepthStencilAttachment : RHI::ImageLayout::ShaderReadOnly);

            auto idxIt = m_builder.getAttachmentIndices().find(key);
            if (idxIt == m_builder.getAttachmentIndices().end()) return;
            m_builder.updateAttachmentParams(idxIt->second, loadOp, storeOp, initial, final);
            // 同步回 params，保证 getTextureLayout 读到推断后的值
            params.loadOp = loadOp;
            params.storeOp = storeOp;
            params.initialLayout = initial;
            params.finalLayout = final;
        };

        for (auto& [texId, key] : m_colorOutputKeyByTex) infer(texId, key, false);
        for (auto& [texId, key] : m_depthOutputKeyByTex) infer(texId, key, true);
    }

    std::pair<RHI::ImageLayout, RHI::ImageLayout> PassNode::getTextureLayout(TextureId texId) const {
        // Compute Pass：读 = ShaderReadOnly，写 = General（storage image 访问前必须在 General）
        if (m_type == PassType::Compute) {
            if (m_computeWriteLayouts.count(texId)) {
                return { m_computeWriteLayouts.at(texId),      // 写前需要 General
                         m_computeWriteLayouts.at(texId) };     // 写后 = General
            }
            if (m_readTextures.count(texId)) {
                return { RHI::ImageLayout::ShaderReadOnly, RHI::ImageLayout::ShaderReadOnly };
            }
            return { RHI::ImageLayout::Undefined, RHI::ImageLayout::Undefined };
        }

        RHI::ImageLayout initial = RHI::ImageLayout::Undefined;
        RHI::ImageLayout final = RHI::ImageLayout::Undefined;

        // 初始布局取第一次出现（任意一个附件）
        for (const auto& [key, params] : m_keyToParams) {
            auto it = m_keyToTexId.find(key);
            if (it != m_keyToTexId.end() && it->second == texId) {
                auto init = params.initialLayout.value_or(RHI::ImageLayout::Undefined);
                if (initial == RHI::ImageLayout::Undefined) initial = init;
            }
        }

        // 最终布局使用计算好的 m_finalLayouts
        auto finIt = m_finalLayouts.find(texId);
        if (finIt != m_finalLayouts.end()) {
            final = finIt->second;
        }

        return { initial, final };
    }

} // namespace StarryEngine::RenderGraph