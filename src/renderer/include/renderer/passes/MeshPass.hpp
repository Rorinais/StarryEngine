#pragma once
#include <renderer/passes/GraphicsPass.hpp>
#include <renderer/passExecutor/SceneDrawExecutor.hpp>

namespace StarryEngine {

    class MeshPass : public GraphicsPass {
    public:
        MeshPass(std::string name, std::string tag,
                 const RHI::Color& clearColor = { 0.0f, 0.0f, 0.0f, 1.0f })
            : GraphicsPass(std::move(name)) {

            SubpassDesc sp;
            sp.tag = std::move(tag);
            sp.executor = std::make_shared<SceneDrawExecutor>();

            RenderGraph::AttachmentParams color;
            color.clearColor = clearColor;
            sp.colorAttachments.push_back({ "SceneColor", color });

            RenderGraph::AttachmentParams depth;
            depth.clearDepth = 1.0f;
            sp.depthAttachment = { "Depth", depth };

            addSubpass(std::move(sp));
        }
    };

} // namespace StarryEngine
