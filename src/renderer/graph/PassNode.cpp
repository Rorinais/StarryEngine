#include "PassNode.hpp"
#include <stdexcept>
#include <iostream>

namespace StarryEngine::RenderGraph {

    PassNode::PassNode(const std::string& name)
        : m_name(name), m_builder(name) {
    }

    SubpassBuilder& PassNode::addSubpass(const std::string& subpassName) {
        return m_builder.addSubpass(SubpassBuilder(subpassName)); // 返回 SubpassBuilder&
    }

    void PassNode::setClearColor(const std::string& attachmentName, const RHI::Color& color) {
        RHI::ClearValue cv;
        cv.color = color;
        m_clearValueMap[attachmentName] = cv;
    }

    void PassNode::setClearDepthStencil(const std::string& attachmentName, float depth, uint32_t stencil) {
        RHI::ClearValue cv;
        cv.depth = depth;
        cv.stencil = stencil;
        m_clearValueMap[attachmentName] = cv;
    }

    void PassNode::collectResourceUsage(const std::unordered_map<std::string, TextureId>& nameToTexId,
        const std::unordered_map<std::string, BufferId>& nameToBufId) {
        // 如果没有缓存构建结果，则构建并缓存
        if (!m_cachedBuildResult) {
            m_cachedBuildResult = m_builder.build(true);
        }

        // 获取 SubpassBuilder 列表（需要在 RenderPassBuilder 中添加 getter）
        const auto& subpassBuilders = m_builder.getSubpassBuilders();

        m_readTextures.clear();
        m_writeTextures.clear();
        m_readBuffers.clear();
        m_writeBuffers.clear();

        for (const auto& subpass : subpassBuilders) {
            for (const auto& name : subpass.getColorAttachmentNames()) {
                auto it = nameToTexId.find(name);
                if (it != nameToTexId.end()) m_writeTextures.insert(it->second);
            }
            for (const auto& name : subpass.getInputAttachmentNames()) {
                auto it = nameToTexId.find(name);
                if (it != nameToTexId.end()) m_readTextures.insert(it->second);
            }
            if (subpass.getDepthStencilAttachmentName()) {
                const auto& name = *subpass.getDepthStencilAttachmentName();
                auto it = nameToTexId.find(name);
                if (it != nameToTexId.end()) {
                    m_writeTextures.insert(it->second);
                    m_readTextures.insert(it->second);
                }
            }
            // 缓冲区类似，可忽略或后续扩展
        }
    }

    // 修改 compile 函数，使用缓存的 buildResult
    bool PassNode::compile(std::shared_ptr<RHI::ResourceManager> resMgr,
        const std::unordered_map<TextureId, RHI::TextureHandle>& texMap,
        const std::unordered_map<BufferId, RHI::BufferHandle>& bufMap) {
        m_resMgr = resMgr;

        // 如果没有缓存，则构建（理论上 collectResourceUsage 已构建）
        if (!m_cachedBuildResult) {
            m_cachedBuildResult = m_builder.build(true);
        }

        auto& buildResult = *m_cachedBuildResult;
        m_attachmentNameToIndex = buildResult.attachmentNameToIndex;

        auto renderPassDesc = buildResult.renderPass->getRenderPassDesc();
        m_renderPassHandle = resMgr->createRenderPass(renderPassDesc, m_name);
        if (!m_renderPassHandle.isValid()) {
            std::cerr << "[PassNode] Failed to create RenderPass: " << m_name << std::endl;
            return false;
        }

        m_pipelines.clear();
        m_subpassRenderers.clear();

        for (size_t i = 0; i < buildResult.pipelineDescriptions.size(); ++i) {
            const auto& pipelineDesc = buildResult.pipelineDescriptions[i];
            auto renderer = buildResult.subpassRenderers[i];

            // 构建 GraphicsPipelineDesc（从 pipelineDesc 复制）
            RHI::GraphicsPipelineDesc gpDesc;
            gpDesc.vertexShader = pipelineDesc.vertexShader;
            gpDesc.fragmentShader = pipelineDesc.fragmentShader;
            gpDesc.vertexInput = pipelineDesc.vertexInput;
            gpDesc.topology = pipelineDesc.topology;
            gpDesc.rasterizer = pipelineDesc.rasterizer;
            gpDesc.multisample = pipelineDesc.multisample;
            gpDesc.depthStencil = pipelineDesc.depthStencil;
            gpDesc.colorBlend = pipelineDesc.colorBlend;
            gpDesc.dynamicStates = pipelineDesc.dynamicStates;
            gpDesc.viewport = pipelineDesc.viewport;
            gpDesc.pipelineLayoutHandle = pipelineDesc.pipelineLayoutHandle;
            gpDesc.renderPass = m_renderPassHandle;
            gpDesc.subpass = static_cast<uint32_t>(i);
            gpDesc.debugName = pipelineDesc.debugName.empty() ? m_name + "_subpass" + std::to_string(i) : pipelineDesc.debugName;

            auto pipelineHandle = resMgr->createGraphicsPipeline(gpDesc);
            if (!pipelineHandle.isValid()) {
                std::cerr << "[PassNode] Failed to create Pipeline for subpass " << i << std::endl;
                return false;
            }

            m_pipelines.push_back(pipelineHandle);
            m_subpassRenderers.push_back(renderer);
        }

        // 构建清除值列表
        m_clearValues.clear();
        for (const auto& name : buildResult.attachmentNames) {
            auto it = m_clearValueMap.find(name);
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

        std::cout << "PassNode::compile: attachmentNames.size() = " << buildResult.attachmentNames.size() << std::endl;
        std::cout << "m_clearValues.size() = " << m_clearValues.size() << std::endl;
        for (size_t i = 0; i < m_clearValues.size(); ++i) {
            std::cout << "  clearValue[" << i << "] type: " << (i == 0 ? "color" : "depth") << std::endl; // 假设你的假设
        }

        return true;
    }

    void PassNode::execute(RHI::RHICommandEncoder* encoder,
        uint32_t frameIndex,
        RHI::FramebufferHandle framebuffer) {
        if (!m_renderPassHandle.isValid() || m_pipelines.empty()) {
            std::cerr << "[PassNode] Pass not compiled: " << m_name << std::endl;
            return;
        }

        auto* renderPassObj = m_resMgr->getRenderPass(m_renderPassHandle);
        auto* fbObj = m_resMgr->getFramebuffer(framebuffer);
        if (!renderPassObj || !fbObj) return;

        // 构建 RenderPassBeginInfo
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

            // ***** 在这里设置动态状态 *****
            RHI::Viewport viewport{
                0.0f,
                0.0f,                      // 从顶部开始
                static_cast<float>(m_width),
                static_cast<float>(m_height),
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
} // namespace StarryEngine::RenderGraph