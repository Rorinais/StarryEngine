#pragma once
#include "Subpass.hpp"
#include "PassWrapper.hpp"

namespace StarryEngine {
    class LightSubpass : public Subpass {
    public:
        LightSubpass(const std::string& name) {
            m_subpass.name = name;
            m_subpass.executor = std::make_shared<DeferredLightingExecutor>();

            auto lightColor = PassWrapper::createColorAttachment(
                RHI::ImageLayout::Undefined, RHI::ImageLayout::ColorAttachment,
                RHI::AttachmentLoadOp::Clear, RHI::AttachmentStoreOp::Store);
            auto lightInput = PassWrapper::createColorAttachment(
                RHI::ImageLayout::ColorAttachment, RHI::ImageLayout::ShaderReadOnly,
                RHI::AttachmentLoadOp::Load, RHI::AttachmentStoreOp::DontCare);

            addColorAttachment("SceneColor", lightColor);
            addInputAttachment("Albedo", lightInput);
            addInputAttachment("Normal", lightInput);
            addInputAttachment("Material", lightInput);
        }
    };
}