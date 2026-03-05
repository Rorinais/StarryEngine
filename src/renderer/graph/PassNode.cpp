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
            for (auto& pipe : m_pipelines) {
                if (pipe.isValid()) {
                    m_resMgr->destroy(pipe);
                }
            }
        }
    }

    SubpassBuilder& PassNode::addSubpass(const std::string& subpassName) {
        return m_builder.addSubpass(SubpassBuilder(subpassName));
    }

    void PassNode::setClearColor(const std::string& key, const RHI::Color& color) {
        RHI::ClearValue cv;
        cv.color = color;
        m_clearValueMap[key] = cv;  // 注意：这里用键存储，后续映射到索引
    }

    void PassNode::setClearDepthStencil(const std::string& key, float depth, uint32_t stencil) {
        RHI::ClearValue cv;
        cv.depth = depth;
        cv.stencil = stencil;
        m_clearValueMap[key] = cv;
    }

    void PassNode::bindAttachment(const std::string& key, TextureId textureId) {
        m_attachmentBindings[key] = textureId;
    }

    TextureId PassNode::getBoundTextureId(const std::string& key) const {
        auto it = m_attachmentBindings.find(key);
        if (it == m_attachmentBindings.end()) {
            throw std::runtime_error("Attachment key not bound: " + key);
        }
        return it->second;
    }

    void PassNode::collectResourceUsage() {
        const auto& subpassBuilders = m_builder.getSubpassBuilders();

        m_readTextures.clear();
        m_writeTextures.clear();
        m_readBuffers.clear();
        m_writeBuffers.clear();

        for (const auto& subpass : subpassBuilders) {
            for (const auto& key : subpass.getColorAttachmentNames()) {
                auto it = m_attachmentBindings.find(key);
                if (it != m_attachmentBindings.end()) {
                    m_writeTextures.insert(it->second);
                }
                else {
                    throw std::runtime_error("Attachment key not bound: " + key);
                }
            }
            for (const auto& key : subpass.getInputAttachmentNames()) {
                auto it = m_attachmentBindings.find(key);
                if (it != m_attachmentBindings.end()) {
                    m_readTextures.insert(it->second);
                }
                else {
                    throw std::runtime_error("Attachment key not bound: " + key);
                }
            }
            if (subpass.getDepthStencilAttachmentName()) {
                const auto& key = *subpass.getDepthStencilAttachmentName();
                auto it = m_attachmentBindings.find(key);
                if (it != m_attachmentBindings.end()) {
                    m_writeTextures.insert(it->second);
                    m_readTextures.insert(it->second);
                }
                else {
                    throw std::runtime_error("Attachment key not bound: " + key);
                }
            }
            // 缓冲区暂时忽略
        }
    }

    bool PassNode::compile(std::shared_ptr<RHI::ResourceManager> resMgr,
        const std::unordered_map<TextureId, PhysicalTextureInfo>& /*texMap*/,
        const std::unordered_map<BufferId, RHI::BufferHandle>& /*bufMap*/) {
        m_resMgr = resMgr;

        if (!m_cachedBuildResult) {
            m_cachedBuildResult = m_builder.build(true);
        }

        auto& buildResult = *m_cachedBuildResult;
        m_attachmentNameToIndex = buildResult.attachmentNameToIndex;

        // 创建 RenderPass
        m_renderPassHandle = resMgr->createRenderPass(buildResult.renderPassDesc, m_name);
        if (!m_renderPassHandle.isValid()) {
            throw std::runtime_error("Failed to create RenderPass: " + m_name);
        }

        m_pipelines.clear();
        m_subpassRenderers.clear();

        for (size_t i = 0; i < buildResult.pipelineDescriptions.size(); ++i) {
            const auto& pipelineDesc = buildResult.pipelineDescriptions[i];
            auto renderer = buildResult.subpassRenderers[i];

            RHI::GraphicsPipelineDesc gpDesc = pipelineDesc;
            gpDesc.renderPass = m_renderPassHandle;
            gpDesc.subpass = static_cast<uint32_t>(i);
            if (gpDesc.debugName.empty()) {
                gpDesc.debugName = m_name + "_subpass" + std::to_string(i);
            }

            auto pipelineHandle = resMgr->createGraphicsPipeline(gpDesc);
            if (!pipelineHandle.isValid()) {
                throw std::runtime_error("Failed to create Pipeline for subpass " + std::to_string(i));
            }

            m_pipelines.push_back(pipelineHandle);
            m_subpassRenderers.push_back(renderer);
        }

        // 构建清除值列表（按附件顺序）
        m_clearValues.clear();
        for (const auto& key : buildResult.attachmentNames) {
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
        if (!m_renderPassHandle.isValid() || m_pipelines.empty()) {
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

        uint32_t subpassCount = static_cast<uint32_t>(m_pipelines.size());
        for (uint32_t i = 0; i < subpassCount; ++i) {
            if (i > 0) {
                encoder->nextSubpass(RHI::SubpassContents::Inline);
            }

            RHI::Viewport viewport{
                0.0f, 0.0f,
                static_cast<float>(m_width), static_cast<float>(m_height),
                0.0f, 1.0f
            };
            encoder->setViewport(viewport);

            RHI::Rect2D scissor{ {0, 0}, {m_width, m_height} };
            encoder->setScissor(scissor);

            PassContext ctx(m_resMgr, m_pipelines, frameIndex, framebuffer);
            if (m_subpassRenderers[i]) {
                m_subpassRenderers[i]->recordCommands(encoder, ctx, i, frameIndex);
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

} // namespace StarryEngine::RenderGraph