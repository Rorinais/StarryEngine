#pragma once
#include "Subpass.hpp"
#include "PassWrapper.hpp"

namespace StarryEngine {
    class GeometrySubpass : public Subpass {
    public:
        GeometrySubpass(const std::string& name) {
            m_subpass.name = name;
            m_subpass.recorder = std::make_shared<MeshDrawRecorder>();

            auto colorAttach = PassWrapper::createColorAttachment(
                RHI::ImageLayout::Undefined, RHI::ImageLayout::ColorAttachment,
                RHI::AttachmentLoadOp::Clear, RHI::AttachmentStoreOp::Store);
            auto depthAttach = PassWrapper::createDepthAttachment(
                RHI::ImageLayout::Undefined, RHI::ImageLayout::DepthStencilAttachment,
                RHI::AttachmentLoadOp::Clear, RHI::AttachmentStoreOp::DontCare);

            addColorAttachment("Albedo", colorAttach);
            addColorAttachment("Normal", colorAttach);
            addColorAttachment("Material", colorAttach);
            setDepthAttachment("Depth", depthAttach);
        }
    };
}