#pragma once
#include <renderer/passes/GraphicsPass.hpp>
#include <renderer/passExecutor/SceneDrawExecutor.hpp>

namespace StarryEngine {

    // 标准几何 pass：SceneDrawExecutor 渲染场景网格（含过程式天空盒）到 SceneColor+Depth。
    // 封装了"首次写 → Clear、产出 → ShaderReadOnly"的常见附件配置，demo 一行声明即可。
    // 步骤3（声明式附件）落地后，这里的附件参数将交给渲染图推断，此类只需声明渲染目标。
    class MeshPass : public GraphicsPass {
    public:
        MeshPass(std::string name, std::string tag,
                 const RHI::Color& clearColor = { 0.0f, 0.0f, 0.0f, 1.0f })
            : GraphicsPass(std::move(name)) {

            SubpassDesc sp;
            sp.tag = std::move(tag);
            sp.executor = std::make_shared<SceneDrawExecutor>();

            // loadOp/布局由渲染图推断（首写→Clear/Undefined/ShaderReadOnly），这里只保留推断不了的值
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
