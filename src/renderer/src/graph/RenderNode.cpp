#include <renderer/graph/RenderNode.hpp>
#include <logging/Logger.hpp>
#include <algorithm>

namespace StarryEngine::RenderGraph {

    RenderNode::RenderNode(const std::string& name, PassType type)
        : GraphNode(name, type) {
    }

    RenderNode::~RenderNode() = default;

    std::string RenderNode::addColorOutput(TextureId texId, const AttachmentParams& params) {
        // 同一纹理 + 相同附件参数 → 复用已有颜色附件
        if (texId.isValid()) {
            auto it = m_colorOutputKeyByTex.find(texId);
            if (it != m_colorOutputKeyByTex.end()) return it->second;
        }
        std::string key = "auto_color_" + std::to_string(m_keyToTexId.size());
        m_keyToTexId[key] = texId;
        m_attachmentKeyToTexId[key] = texId;
        m_keyToParams[key] = params;
        m_colorOutputKeyByTex[texId] = key;
        m_writeTextures.insert(texId);
        return key;
    }

    std::string RenderNode::addDepthOutput(TextureId texId, const AttachmentParams& params) {
        if (texId.isValid()) {
            auto it = m_depthOutputKeyByTex.find(texId);
            if (it != m_depthOutputKeyByTex.end()) return it->second;
        }
        std::string key = "auto_depth_" + std::to_string(m_keyToTexId.size());
        m_keyToTexId[key] = texId;
        m_attachmentKeyToTexId[key] = texId;
        m_keyToParams[key] = params;
        m_depthOutputKeyByTex[texId] = key;
        m_writeTextures.insert(texId);
        return key;
    }

    std::string RenderNode::addInput(TextureId texId, const AttachmentParams& params) {
        std::string key = "auto_input_" + std::to_string(m_keyToTexId.size());
        m_keyToTexId[key] = texId;
        m_attachmentKeyToTexId[key] = texId;
        m_keyToParams[key] = params;
        m_inputLayouts[texId] = RHI::ImageLayout::ShaderReadOnly;
        m_readTextures.insert(texId);
        return key;
    }

    const std::vector<std::string>& RenderNode::getColorAttachmentNames() const {
        return m_colorAttachmentKeys;
    }

    const std::vector<std::string>& RenderNode::getDepthAttachmentNames() const {
        return m_depthAttachmentKeys;
    }

    TextureId RenderNode::getTextureIdForAttachmentKey(const std::string& key) const {
        auto it = m_attachmentKeyToTexId.find(key);
        return (it != m_attachmentKeyToTexId.end()) ? it->second : TextureId::Null();
    }

    std::pair<RHI::ImageLayout, RHI::ImageLayout> RenderNode::getTextureLayout(TextureId texId) const {
        // Compute：读 = ShaderReadOnly，写 = General
        if (m_type == PassType::Compute) {
            if (m_computeWriteLayouts.count(texId))
                return { RHI::ImageLayout::General, RHI::ImageLayout::General };
            if (m_readTextures.count(texId))
                return { RHI::ImageLayout::ShaderReadOnly, RHI::ImageLayout::ShaderReadOnly };
            return { RHI::ImageLayout::Undefined, RHI::ImageLayout::Undefined };
        }

        RHI::ImageLayout initial = RHI::ImageLayout::Undefined;
        RHI::ImageLayout final = RHI::ImageLayout::Undefined;
        for (const auto& [key, tid] : m_keyToTexId) {
            if (tid == texId) {
                auto it = m_keyToParams.find(key);
                if (it != m_keyToParams.end()) {
                    auto init = it->second.initialLayout.value_or(RHI::ImageLayout::Undefined);
                    if (initial == RHI::ImageLayout::Undefined) initial = init;
                    auto fin = it->second.finalLayout.value_or(RHI::ImageLayout::Undefined);
                    if (final == RHI::ImageLayout::Undefined) final = fin;
                }
            }
        }
        // 输入附件：读 = ShaderReadOnly
        auto inpIt = m_inputLayouts.find(texId);
        if (inpIt != m_inputLayouts.end()) return { inpIt->second, inpIt->second };
        // 普通读纹理（如 ShadowMap 采样）：读 = ShaderReadOnly
        if (m_readTextures.count(texId) && m_type != PassType::Compute) {
            return { RHI::ImageLayout::ShaderReadOnly, RHI::ImageLayout::ShaderReadOnly };
        }
        auto finIt = m_finalLayouts.find(texId);
        if (finIt != m_finalLayouts.end()) final = finIt->second;
        return { initial, final };
    }

    void RenderNode::resolveInferredAttachments(const std::function<bool(TextureId)>& isFirstWriter) {
        // 首写者 loadOp=Clear，后续 Load（动态渲染无 render pass，逐 pass 决定清除语义）。
        // 注意：本方法在图 compile 的 resolveInferredAttachments 阶段调用（在 pass->compile() 之前），
        // 因此必须用 configure 阶段已填充的 m_colorOutputKeyByTex / m_depthOutputKeyByTex。
        m_colorLoadOps.clear();
        m_colorLoadOps.reserve(m_colorOutputKeyByTex.size());
        for (const auto& [texId, key] : m_colorOutputKeyByTex) {
            m_colorLoadOps.push_back(isFirstWriter(texId) ? RHI::AttachmentLoadOp::Clear : RHI::AttachmentLoadOp::Load);
        }
        m_depthLoadOps.clear();
        m_depthLoadOps.reserve(m_depthOutputKeyByTex.size());
        for (const auto& [texId, key] : m_depthOutputKeyByTex) {
            m_depthLoadOps.push_back(isFirstWriter(texId) ? RHI::AttachmentLoadOp::Clear : RHI::AttachmentLoadOp::Load);
        }
    }

    bool RenderNode::isDepthFormat(RHI::Format format) {
        return format == RHI::Format::D16_UNorm || format == RHI::Format::D24_UNorm_S8_UInt
            || format == RHI::Format::D32_Float;
    }

    bool RenderNode::compile(std::shared_ptr<RHI::ResourceManager> resMgr,
        const std::unordered_map<TextureId, PhysicalTextureInfo>& texMap,
        const std::unordered_map<TextureId, RHI::TextureDesc>& texDescMap,
        const std::unordered_map<BufferId, RHI::BufferHandle>& /*bufMap*/) {
        m_resMgr = resMgr;

        // Compute：无渲染通道，仅记录最终布局
        if (m_type == PassType::Compute) {
            m_finalLayouts.clear();
            for (const auto& [texId, layout] : m_computeWriteLayouts) m_finalLayouts[texId] = layout;
            return true;
        }

        // 附件名列表（顺序稳定，供视图收集）
        m_attachmentNames.clear();
        for (const auto& [key, texId] : m_keyToTexId) m_attachmentNames.push_back(key);
        std::sort(m_attachmentNames.begin(), m_attachmentNames.end());

        // 颜色/深度附件 key 缓存（编译期单线程填充；运行期并行线程只读）
        m_colorAttachmentKeys.clear();
        m_colorAttachmentKeys.reserve(m_colorOutputKeyByTex.size());
        for (const auto& [texId, key] : m_colorOutputKeyByTex) m_colorAttachmentKeys.push_back(key);
        m_depthAttachmentKeys.clear();
        m_depthAttachmentKeys.reserve(m_depthOutputKeyByTex.size());
        for (const auto& [texId, key] : m_depthOutputKeyByTex) m_depthAttachmentKeys.push_back(key);

        // 收集 clear 值（颜色附件，顺序按注册）
        m_clearValues.clear();
        for (const auto& [texId, key] : m_colorOutputKeyByTex) {
            auto it = m_keyToParams.find(key);
            if (it == m_keyToParams.end()) continue;
            if (it->second.clearColor.has_value()) {
                m_clearValues.emplace_back(it->second.clearColor.value());
            } else {
                m_clearValues.emplace_back(RHI::Color{ 0.0f, 0.0f, 0.0f, 1.0f });
            }
        }
        if (m_clearValues.empty()) m_clearValues.emplace_back(RHI::Color{ 0.0f, 0.0f, 0.0f, 1.0f });

        // 最终布局：颜色附件 → 声明的 finalLayout（缺省 ColorAttachment）
        m_finalLayouts.clear();
        for (const auto& [texId, key] : m_colorOutputKeyByTex) {
            auto it = m_keyToParams.find(key);
            RHI::ImageLayout fin = it != m_keyToParams.end()
                ? it->second.finalLayout.value_or(RHI::ImageLayout::ColorAttachment)
                : RHI::ImageLayout::ColorAttachment;
            m_finalLayouts[texId] = fin;
        }
        for (const auto& [texId, key] : m_depthOutputKeyByTex) {
            auto it = m_keyToParams.find(key);
            RHI::ImageLayout fin = it != m_keyToParams.end()
                ? it->second.finalLayout.value_or(RHI::ImageLayout::DepthStencilAttachment)
                : RHI::ImageLayout::DepthStencilAttachment;
            m_finalLayouts[texId] = fin;
        }

        // 扁平化各段 executors（动态渲染无 subpass，同一通道内按序执行）
        m_passExecutors.clear();
        for (auto& sub : m_subpasses) {
            m_passExecutors.push_back(sub.getExecutor());
        }
        return true;
    }

    void RenderNode::executeDynamic(RHI::RHICommandEncoder* encoder,
        const RenderContext& context,
        uint32_t frameIndex,
        const std::vector<void*>& attachmentViews,
        uint32_t frameSlot,
        void* depthView) {
        if (m_type == PassType::Compute) {
            if (m_enabled && m_computeRecorder) {
                m_computeRecorder->execute(encoder, context, PassContext(m_resMgr, frameIndex, RHI::FramebufferHandle{}, frameSlot), 0);
            }
            return;
        }
        if (attachmentViews.empty() && depthView == nullptr) {
            LOG_WARN("[{}] executeDynamic: no attachment views — skipping pass", m_name);
            return;
        }

        RHI::RenderingInfo rInfo;
        rInfo.renderArea = { {0, 0}, { m_width, m_height } };
        rInfo.layerCount = 1;
        for (size_t vi = 0; vi < attachmentViews.size(); ++vi) {
            RHI::RenderingAttachmentInfo att;
            att.imageView = attachmentViews[vi];
            att.imageLayout = RHI::ImageLayout::ColorAttachment;
            att.loadOp = (vi < m_colorLoadOps.size()) ? m_colorLoadOps[vi] : RHI::AttachmentLoadOp::Clear;
            att.storeOp = RHI::AttachmentStoreOp::Store;
            if (!m_clearValues.empty() && att.loadOp == RHI::AttachmentLoadOp::Clear) att.clearValue = m_clearValues[0];
            rInfo.colorAttachments.push_back(att);
        }
        if (depthView != nullptr) {
            // 纯深度 pass（ShadowPass）：深度附件
            RHI::RenderingAttachmentInfo depthAtt;
            depthAtt.imageView = depthView;
            depthAtt.imageLayout = RHI::ImageLayout::DepthStencilAttachment;
            depthAtt.loadOp = m_depthLoadOps.empty() ? RHI::AttachmentLoadOp::Clear : m_depthLoadOps[0];
            depthAtt.storeOp = RHI::AttachmentStoreOp::Store;
            depthAtt.clearValue = RHI::ClearValue(1.0f, 0u);
            rInfo.depthAttachment = depthAtt;
            rInfo.hasDepth = true;
        }
        encoder->beginRendering(rInfo);

        // 传统 render pass 对深度附件无条件 stencilLoadOp=Clear（首用清模板）；
        // Vulkan 1.3 动态渲染 depth/stencil 共享 loadOp，depth=Load 时模板不会自动清。
        // StencilPass 依赖"开始时模板=0"，这里补发一次 stencil-only clear（StencilWrite 前清 0）。
        if (rInfo.hasDepth && rInfo.depthAttachment.loadOp == RHI::AttachmentLoadOp::Load) {
            RHI::ClearAttachment stencilClear;
            stencilClear.aspectMask = RHI::ImageAspect::Stencil;
            stencilClear.clearValue = RHI::ClearValue(1.0f, 0u);
            RHI::ClearRect rect;
            rect.rect = { {0, 0}, { m_width, m_height } };
            encoder->clearAttachments({ stencilClear }, { rect });
        }

        if (m_enabled) {
            encoder->setViewport({ 0.0f, 0.0f, (float)m_width, (float)m_height, 0.0f, 1.0f });
            encoder->setScissor({ {0, 0}, {m_width, m_height} });
            for (size_t i = 0; i < m_passExecutors.size(); ++i) {
                if (m_passExecutors[i]) {
                    m_passExecutors[i]->execute(encoder, context, PassContext(m_resMgr, frameIndex, RHI::FramebufferHandle{}, frameSlot), static_cast<uint32_t>(i));
                }
            }
        }
        encoder->endRendering();
    }

    void RenderNode::beginRenderingOnPrimary(RHI::RHICommandEncoder* encoder,
        const std::vector<RHI::RenderingAttachmentInfo>& colorAttachments,
        const RHI::RenderingAttachmentInfo& depthAttachment,
        bool hasDepth) {
        RHI::RenderingInfo rInfo;
        rInfo.renderArea = { {0, 0}, { m_width, m_height } };
        rInfo.layerCount = 1;
        rInfo.colorAttachments = colorAttachments;
        if (hasDepth) {
            rInfo.depthAttachment = depthAttachment;
            rInfo.hasDepth = true;
        }
        encoder->beginRendering(rInfo);
    }

    void RenderNode::endRenderingOnPrimary(RHI::RHICommandEncoder* encoder) {
        encoder->endRendering();
    }

    void RenderNode::recordBody(RHI::RHICommandEncoder* encoder,
        const RenderContext& context,
        uint32_t frameIndex,
        RHI::FramebufferHandle /*framebuffer*/,
        uint32_t subpassIndex,
        uint32_t frameSlot) {
        if (m_type == PassType::Compute) {
            if (m_enabled && m_computeRecorder) {
                m_computeRecorder->execute(encoder, context, PassContext(m_resMgr, frameIndex, RHI::FramebufferHandle{}, frameSlot), 0);
            }
            return;
        }
        encoder->setViewport({ 0.0f, 0.0f, (float)m_width, (float)m_height, 0.0f, 1.0f });
        encoder->setScissor({ {0, 0}, {m_width, m_height} });
        if (subpassIndex < m_passExecutors.size() && m_passExecutors[subpassIndex]) {
            m_passExecutors[subpassIndex]->execute(encoder, context,
                PassContext(m_resMgr, frameIndex, RHI::FramebufferHandle{}, frameSlot), subpassIndex);
        }
    }

    // ── 统一并行入口：主缓冲执行已录 secondary ──
    // compute：无渲染通道，直接执行（barrier 已由 RenderGraph 提前插入）。
    // graphics：动态渲染框架（beginRendering → stencil clear → 各 secondary → endRendering）。
    void RenderNode::executeSecondariesOnPrimary(RHI::RHICommandEncoder* encoder,
        RHI::FramebufferHandle /*framebuffer*/,
        const std::vector<void*>& attachmentViews,
        void* depthView,
        const std::vector<void*>& secondaries) {
        if (m_type == PassType::Compute) {
            if (!secondaries.empty() && secondaries[0] != nullptr) {
                encoder->executeCommands(secondaries);
            }
            return;
        }

        if (attachmentViews.empty() && depthView == nullptr) {
            LOG_WARN("[{}] executeSecondariesOnPrimary: 无附件视图 — 跳过", m_name);
            return;
        }

        // 颜色附件（按 loadOp/clear 推断）
        std::vector<RHI::RenderingAttachmentInfo> colorAtts;
        colorAtts.reserve(attachmentViews.size());
        for (size_t vi = 0; vi < attachmentViews.size(); ++vi) {
            RHI::RenderingAttachmentInfo att;
            att.imageView = attachmentViews[vi];
            att.imageLayout = RHI::ImageLayout::ColorAttachment;
            att.loadOp = (vi < m_colorLoadOps.size()) ? m_colorLoadOps[vi] : RHI::AttachmentLoadOp::Clear;
            att.storeOp = RHI::AttachmentStoreOp::Store;
            if (att.loadOp == RHI::AttachmentLoadOp::Clear && !m_clearValues.empty()) {
                att.clearValue = m_clearValues[vi < m_clearValues.size() ? vi : 0];
            }
            colorAtts.push_back(att);
        }

        // 深度附件（loadOp 按推断）
        RHI::RenderingAttachmentInfo depthAtt;
        if (depthView != nullptr) {
            depthAtt.imageView = depthView;
            depthAtt.imageLayout = RHI::ImageLayout::DepthStencilAttachment;
            depthAtt.loadOp = m_depthLoadOps.empty() ? RHI::AttachmentLoadOp::Clear : m_depthLoadOps[0];
            depthAtt.storeOp = RHI::AttachmentStoreOp::Store;
            depthAtt.clearValue = RHI::ClearValue(1.0f, 0u);
        }
        beginRenderingOnPrimary(encoder, colorAtts, depthAtt, depthView != nullptr);

        // 传统 render pass 对深度附件无条件 stencilLoadOp=Clear（首用清模板）；
        // Vulkan 1.3 动态渲染 depth/stencil 共享 loadOp，depth=Load 时模板不会自动清。
        // StencilPass 依赖"开始时模板=0"，这里补发一次 stencil-only clear（StencilWrite 前清 0）。
        if (depthView != nullptr && depthAtt.loadOp == RHI::AttachmentLoadOp::Load) {
            RHI::ClearAttachment stencilClear;
            stencilClear.aspectMask = RHI::ImageAspect::Stencil;
            stencilClear.clearValue = RHI::ClearValue(1.0f, 0u);
            RHI::ClearRect rect;
            rect.rect = { {0, 0}, { m_width, m_height } };
            encoder->clearAttachments({ stencilClear }, { rect });
        }

        // 禁用 pass：空体 begin/end，保持 loadOp 的 clear 语义
        if (!secondaries.empty()) {
            for (auto* sec : secondaries) {
                if (sec == nullptr) continue;
                encoder->executeCommands({ sec });
            }
        }
        endRenderingOnPrimary(encoder);
    }

} // namespace StarryEngine::RenderGraph
