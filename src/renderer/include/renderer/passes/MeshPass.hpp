#pragma once
#include <renderer/passes/GraphicsPass.hpp>
#include <renderer/renderPaths/LayerRegistry.hpp>
#include <renderer/passExecutor/SceneDrawExecutor.hpp>

namespace StarryEngine {

    class MeshPass : public GraphicsPass {
    public:
        MeshPass(std::string name,const RHI::Color& clearColor = { 0.0f, 0.0f, 0.0f, 1.0f })
            : GraphicsPass(std::move(name)) {

            SubpassDesc sp;
            sp.layer = RenderLayers::Opaque;   
            sp.queue = 0;
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
