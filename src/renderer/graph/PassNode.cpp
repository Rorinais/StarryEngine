#include "PassNode.hpp"
#include <stdexcept>
#include <iostream>

namespace StarryEngine::RenderGraph {

    PassNode::PassNode(const std::string& name)
        : m_name(name), m_builder(name) {
    }

    PassNode::~PassNode() {
        if (m_resMgr) {
            if (m_renderPassHandle.isValid()) {
                m_resMgr->destroy(m_renderPassHandle);
            }
        }
    }

    std::string PassNode::addColorOutput(TextureId texId, const AttachmentParams& params) {
        std::string key = "auto_color_" + std::to_string(texId.id());
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
        return key;
    }

    std::string PassNode::addDepthOutput(TextureId texId, const AttachmentParams& params) {
        std::string key = "auto_depth_" + std::to_string(texId.id());
        m_builder.registerDepthAttachment(key,
            params.format.value_or(RHI::Format::D32_Float),
            params.loadOp.value_or(RHI::AttachmentLoadOp::Clear),
            params.storeOp.value_or(RHI::AttachmentStoreOp::DontCare),
            params.initialLayout.value_or(RHI::ImageLayout::Undefined),
            params.finalLayout.value_or(RHI::ImageLayout::DepthStencilAttachment));
        m_keyToTexId[key] = texId;
        m_writeTextures.insert(texId);
        if (params.clearDepth.has_value()) {
            m_clearValueMap[key] = RHI::ClearValue(params.clearDepth.value(),
                params.clearStencil.value_or(0));
        }
        m_keyToParams[key] = params;
        return key;
    }

    std::string PassNode::addInput(TextureId texId, const AttachmentParams& params) {
        std::string key = "auto_input_" + std::to_string(texId.id());
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
        std::string key = "auto_resolve_" + std::to_string(texId.id());
        m_builder.registerResolveAttachment(key,
            params.format.value_or(RHI::Format::RGBA8_UNorm),
            params.finalLayout.value_or(RHI::ImageLayout::ColorAttachment));
        m_keyToTexId[key] = texId;
        m_writeTextures.insert(texId);
        m_keyToParams[key] = params;
        return key;
    }

    std::string PassNode::addPreserve(TextureId texId) {
        std::string key = "auto_preserve_" + std::to_string(texId.id());
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

        m_subpassRecorders = m_cachedBuildResult->subpassRecorders;

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

    void PassNode::execute(RHI::RHICommandEncoder* encoder,
        uint32_t frameIndex,
        RHI::FramebufferHandle framebuffer) {
        if (!m_renderPassHandle.isValid()) {
            throw std::runtime_error("Pass not compiled: " + m_name);
        }

        auto* renderPassObj = m_resMgr->getRenderPass(m_renderPassHandle);
        auto* fbObj = m_resMgr->getFramebuffer(framebuffer);
        if (!renderPassObj || !fbObj) return;

        RHI::RenderPassBeginInfo beginInfo{};
        beginInfo.renderPass = renderPassObj->getNativeHandle();
        beginInfo.framebuffer = fbObj->getNativeHandle();
        beginInfo.renderArea = { {0, 0}, { m_width, m_height } };
        beginInfo.clearValues = m_clearValues;

        encoder->beginRenderPass(beginInfo, RHI::SubpassContents::Inline);

        RHI::Viewport viewport{ 0.0f, 0.0f, (float)m_width, (float)m_height, 0.0f, 1.0f };
        encoder->setViewport(viewport);
        RHI::Rect2D scissor{ {0, 0}, {m_width, m_height} };
        encoder->setScissor(scissor);

        PassContext ctx(m_resMgr, frameIndex, framebuffer);

        uint32_t subpassCount = static_cast<uint32_t>(m_subpassRecorders.size());
        for (uint32_t i = 0; i < subpassCount; ++i) {
            if (i > 0) encoder->nextSubpass(RHI::SubpassContents::Inline);
            if (m_subpassRecorders[i]) {
                m_subpassRecorders[i]->recordCommands(encoder, ctx, i, frameIndex);
            }
        }

        encoder->endRenderPass();
    }

    const std::vector<std::string>& PassNode::getAttachmentNames() const {
        if (!m_cachedBuildResult) {
            throw std::runtime_error("PassNode not built yet: " + m_name);
        }
        return m_cachedBuildResult->attachmentNames;
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

} // namespace StarryEngine::RenderGraph