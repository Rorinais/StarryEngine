#pragma once
#include <renderer/graph/Types.hpp>
#include <renderer/RenderTypes.hpp>
#include <renderer/passExecutor/IPassExecutor.hpp>

namespace StarryEngine {

    struct SubpassAttachment {
        std::string textureName;
        RenderGraph::AttachmentParams params;
    };

    struct SubpassDesc {
        std::string tag;           
        std::shared_ptr<IPassExecutor> executor;
        std::vector<SubpassAttachment> colorAttachments;
        std::optional<SubpassAttachment> depthAttachment;
        std::vector<SubpassAttachment> inputAttachments;
        std::vector<SubpassAttachment> resolveAttachments;
        std::vector<std::string> preserveAttachments;
    };

    struct PassDesc {
        std::string name;
        std::vector<SubpassDesc> subpasses;
    };

    using RenderPathConfig = std::vector<PassDesc>;
}