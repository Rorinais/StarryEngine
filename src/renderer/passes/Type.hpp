#pragma once
#include "../../renderer/graph/Types.hpp"          
#include "../../scene/SceneType.hpp"   
#include "../../renderer/passExecutor/IPassExecutor.hpp"

namespace StarryEngine {

    //struct StagePassInfo {
    //    RHI::RenderPassHandle renderPassHandle;
    //    std::unordered_map<Scene::RenderQueue, uint32_t> queueToSubpass;
    //};

    struct SubpassAttachment {
        std::string textureName;
        RenderGraph::AttachmentParams params;
    };

    struct SubpassDesc {
        std::string name;
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