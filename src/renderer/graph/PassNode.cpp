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

    // ----- 原有 API 实现 -----
    SubpassBuilder& PassNode::addSubpass(const std::string& subpassName) {
        return m_builder.addSubpass(SubpassBuilder(subpassName));
    }

    void PassNode::setClearColor(const std::string& key, const RHI::Color& color) {
        RHI::ClearValue cv;
        cv.color = color;
        m_clearValueMap[key] = cv;
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

    // ----- 新增 API 实现 -----
    AttachmentConfig& PassNode::addColorOutput(TextureId texId) {
        if (m_texToRequestIndex.find(texId) != m_texToRequestIndex.end())
            throw std::runtime_error("Texture already added to this pass");
        AttachmentRequest req;
        req.texId = texId;
        req.type = AttachmentRequestType::ColorOutput;
        req.key = "auto_color_" + std::to_string(texId.id());  // 立即生成键
        m_texToRequestIndex[texId] = m_attachmentRequests.size();
        m_attachmentRequests.push_back(req);
        return m_attachmentRequests.back().config;
    }

    AttachmentConfig& PassNode::addDepthOutput(TextureId texId) {
        if (m_texToRequestIndex.find(texId) != m_texToRequestIndex.end())
            throw std::runtime_error("Texture already added to this pass");
        AttachmentRequest req;
        req.texId = texId;
        req.type = AttachmentRequestType::DepthOutput;
        req.key = "auto_depth_" + std::to_string(texId.id());
        m_texToRequestIndex[texId] = m_attachmentRequests.size();
        m_attachmentRequests.push_back(req);
        return m_attachmentRequests.back().config;
    }

    AttachmentConfig& PassNode::addInput(TextureId texId) {
        if (m_texToRequestIndex.find(texId) != m_texToRequestIndex.end())
            throw std::runtime_error("Texture already added to this pass");
        AttachmentRequest req;
        req.texId = texId;
        req.type = AttachmentRequestType::Input;
        req.key = "auto_input_" + std::to_string(texId.id());
        m_texToRequestIndex[texId] = m_attachmentRequests.size();
        m_attachmentRequests.push_back(req);
        return m_attachmentRequests.back().config;
    }

    AttachmentConfig& PassNode::addResolve(TextureId texId) {
        if (m_texToRequestIndex.find(texId) != m_texToRequestIndex.end())
            throw std::runtime_error("Texture already added to this pass");
        AttachmentRequest req;
        req.texId = texId;
        req.type = AttachmentRequestType::Resolve;
        req.key = "auto_resolve_" + std::to_string(texId.id());
        m_texToRequestIndex[texId] = m_attachmentRequests.size();
        m_attachmentRequests.push_back(req);
        return m_attachmentRequests.back().config;
    }

    AttachmentConfig& PassNode::addPreserve(TextureId texId) {
        if (m_texToRequestIndex.find(texId) != m_texToRequestIndex.end())
            throw std::runtime_error("Texture already added to this pass");
        AttachmentRequest req;
        req.texId = texId;
        req.type = AttachmentRequestType::Preserve;
        req.key = "auto_preserve_" + std::to_string(texId.id());
        m_texToRequestIndex[texId] = m_attachmentRequests.size();
        m_attachmentRequests.push_back(req);
        return m_attachmentRequests.back().config;
    }

    SubpassBuilderProxy PassNode::addSubpassProxy(const std::string& subpassName) {
        auto& builder = m_builder.addSubpass(SubpassBuilder(subpassName));
        return SubpassBuilderProxy(*this, builder);
    }

    std::string PassNode::getKeyForTexture(TextureId texId) const {
        auto it = m_texToRequestIndex.find(texId);
        if (it == m_texToRequestIndex.end())
            throw std::runtime_error("Texture not declared in pass");
        return m_attachmentRequests[it->second].key;
    }

    void PassNode::collectResourceUsage() {
        const auto& subpassBuilders = m_builder.getSubpassBuilders();

        m_readTextures.clear();
        m_writeTextures.clear();
        m_readBuffers.clear();
        m_writeBuffers.clear();

        // 从原有的子通道收集（基于键）
        for (const auto& subpass : subpassBuilders) {
            for (const auto& key : subpass.getColorAttachmentNames()) {
                auto it = m_attachmentBindings.find(key);
                if (it != m_attachmentBindings.end()) {
                    m_writeTextures.insert(it->second);
                }
            }
            for (const auto& key : subpass.getInputAttachmentNames()) {
                auto it = m_attachmentBindings.find(key);
                if (it != m_attachmentBindings.end()) {
                    m_readTextures.insert(it->second);
                }
            }
            if (subpass.getDepthStencilAttachmentName()) {
                const auto& key = *subpass.getDepthStencilAttachmentName();
                auto it = m_attachmentBindings.find(key);
                if (it != m_attachmentBindings.end()) {
                    m_writeTextures.insert(it->second);
                    m_readTextures.insert(it->second);
                }
            }
        }

        // 从附件请求收集（用于依赖分析）
        for (const auto& req : m_attachmentRequests) {
            if (req.type == AttachmentRequestType::ColorOutput ||
                req.type == AttachmentRequestType::DepthOutput ||
                req.type == AttachmentRequestType::Resolve) {
                m_writeTextures.insert(req.texId);
            }
            if (req.type == AttachmentRequestType::Input) {
                m_readTextures.insert(req.texId);
            }
            if (req.type == AttachmentRequestType::DepthOutput) {
                m_readTextures.insert(req.texId); // 深度也可能被后续读取
            }
        }
    }

    bool PassNode::compile(std::shared_ptr<RHI::ResourceManager> resMgr,
        const std::unordered_map<TextureId, PhysicalTextureInfo>& texMap,
        const std::unordered_map<TextureId, RHI::TextureDesc>& texDescMap,
        const std::unordered_map<BufferId, RHI::BufferHandle>& /*bufMap*/) {
        m_resMgr = resMgr;

        // 处理附件请求：调用 register 和 bind
        for (auto& req : m_attachmentRequests) {
            const auto& key = req.key;  // 键已在创建时生成
            // 获取纹理描述
            auto descIt = texDescMap.find(req.texId);
            if (descIt == texDescMap.end()) {
                throw std::runtime_error("Texture description not found for texId");
            }
            const auto& texDesc = descIt->second;

            // 根据类型调用相应的 register 函数
            const auto& config = req.config;
            switch (req.type) {
            case AttachmentRequestType::ColorOutput:
                m_builder.registerColorAttachment(key, texDesc.format,
                    config.getFinalLayout().value_or(RHI::ImageLayout::ColorAttachment),
                    config.getLoadOp().value_or(RHI::AttachmentLoadOp::Clear),
                    config.getStoreOp().value_or(RHI::AttachmentStoreOp::Store),
                    config.getInitialLayout().value_or(RHI::ImageLayout::Undefined));
                break;
            case AttachmentRequestType::DepthOutput:
                m_builder.registerDepthAttachment(key, texDesc.format,
                    config.getLoadOp().value_or(RHI::AttachmentLoadOp::Clear),
                    config.getStoreOp().value_or(RHI::AttachmentStoreOp::Store),
                    config.getInitialLayout().value_or(RHI::ImageLayout::Undefined),
                    config.getFinalLayout().value_or(RHI::ImageLayout::DepthStencilAttachment));
                break;
            case AttachmentRequestType::Input:
                m_builder.registerInputAttachment(key, texDesc.format,
                    config.getFinalLayout().value_or(RHI::ImageLayout::ShaderReadOnly),
                    config.getInitialLayout().value_or(RHI::ImageLayout::Undefined),
                    config.getLoadOp().value_or(RHI::AttachmentLoadOp::Load),
                    config.getStoreOp().value_or(RHI::AttachmentStoreOp::DontCare));
                break;
            case AttachmentRequestType::Resolve:
                m_builder.registerResolveAttachment(key, texDesc.format,
                    config.getFinalLayout().value_or(RHI::ImageLayout::ColorAttachment));
                break;
            case AttachmentRequestType::Preserve:
                // Preserve 不需要注册，只需绑定
                break;
            }

            // 绑定附件键到纹理 ID
            bindAttachment(key, req.texId);

            // 如果有清除值设置，记录到 m_clearValueMap
            if (config.getClearColor().has_value()) {
                setClearColor(key, *config.getClearColor());
            }
            else if (config.getClearDepth().has_value()) {
                setClearDepthStencil(key, *config.getClearDepth(), config.getClearStencil().value_or(0));
            }
        }

        // 原有的构建流程
        if (!m_cachedBuildResult) {
            m_cachedBuildResult = m_builder.build(true);
        }

        auto& buildResult = *m_cachedBuildResult;
        m_attachmentNameToIndex = buildResult.attachmentNameToIndex;

        m_renderPassHandle = resMgr->createRenderPass(buildResult.renderPassDesc, m_name);
        if (!m_renderPassHandle.isValid()) {
            throw std::runtime_error("Failed to create RenderPass: " + m_name);
        }

        m_pipelines.clear();
        m_subpassRecorders.clear();

        for (size_t i = 0; i < buildResult.pipelineDescriptions.size(); ++i) {
            const auto& pipelineDesc = buildResult.pipelineDescriptions[i];
            auto recorder = buildResult.subpassRecorders[i];
            bool hasPipeline = buildResult.subpassHasPipeline[i];  // 获取标记

            if (hasPipeline) {
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
            }
            else {
                // 无管线的子通道，放入无效句柄
                m_pipelines.push_back(RHI::PipelineHandle::Null());
            }
            m_subpassRecorders.push_back(recorder);
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

            RHI::Viewport viewport{ 0.0f, 0.0f, (float)m_width, (float)m_height, 0.0f, 1.0f };
            encoder->setViewport(viewport);
            RHI::Rect2D scissor{ {0, 0}, {m_width, m_height} };
            encoder->setScissor(scissor);

            // 绑定当前子通道的管线
            if (i < m_pipelines.size() && m_pipelines[i].isValid()) {
                encoder->bindPipeline(m_resMgr->getPipeline(m_pipelines[i]));
            }
            else {
                LOG_ERROR("Subpass {} has no valid pipeline bound!", i);
                continue;
            }

            PassContext ctx(m_resMgr, m_pipelines, frameIndex, framebuffer);
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

} // namespace StarryEngine::RenderGraph