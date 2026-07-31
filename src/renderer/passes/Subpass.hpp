#pragma once
#include "Type.hpp"
#include "../passExecutor/MeshDrawExecutor.hpp"
#include "../passExecutor/DeferredLightingExecutor.hpp"

namespace StarryEngine {
    class Subpass {
    public:
        Subpass() = default;
        virtual ~Subpass() = default;

        Subpass(const std::string& name,
            std::shared_ptr<IPassExecutor> executor,
            const std::vector<SubpassAttachment>& colorAttachments = {},
            const std::vector<SubpassAttachment>& inputAttachments = {},
            std::optional<SubpassAttachment> depthAttachment = std::nullopt,
            const std::vector<SubpassAttachment>& resolveAttachments = {},
            const std::vector<std::string>& preserveAttachments = {}) {
            m_subpass.name = name;
            m_subpass.executor = executor;
            m_subpass.colorAttachments = colorAttachments;
            m_subpass.inputAttachments = inputAttachments;
            m_subpass.depthAttachment = depthAttachment;
            m_subpass.resolveAttachments = resolveAttachments;
            m_subpass.preserveAttachments = preserveAttachments;
        }

        Subpass& addColorAttachment(const std::string& name, const RenderGraph::AttachmentParams& params) {
            m_subpass.colorAttachments.push_back({ name, params });
            return *this;
        }
        Subpass& addInputAttachment(const std::string& name, const RenderGraph::AttachmentParams& params) {
            m_subpass.inputAttachments.push_back({ name, params });
            return *this;
        }
        Subpass& addResolveAttachment(const std::string& name, const RenderGraph::AttachmentParams& params) {
            m_subpass.resolveAttachments.push_back({ name, params });
            return *this;
        }
        Subpass& setDepthAttachment(const std::string& name, const RenderGraph::AttachmentParams& params) {
            m_subpass.depthAttachment = { name, params };
            return *this;
        }
        Subpass& addPreserveAttachment(const std::string& name) {
            m_subpass.preserveAttachments.push_back(name);
            return *this;
        }

        // 获取最终配置
        SubpassDesc getConfig() const { return m_subpass; }

    protected:
        SubpassDesc m_subpass;
    };
}