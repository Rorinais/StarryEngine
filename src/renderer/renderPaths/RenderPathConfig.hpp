#pragma once
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "../../renderer/graph/Types.hpp"          
#include "../../renderer/subpassRecorder/ISubpassRecorder.hpp"
#include "../../scene/SceneType.hpp"             

namespace StarryEngine {

    struct SubpassAttachment {
        std::string textureName;
        RenderGraph::AttachmentParams params;
    };

    struct SubpassConfig {
        std::string name;
        std::vector<SubpassAttachment> colorAttachments;    
        std::optional<SubpassAttachment> depthAttachment;   
        std::vector<SubpassAttachment> inputAttachments;    
        std::vector<SubpassAttachment> resolveAttachments;  
        std::vector<std::string> preserveAttachments;       
        std::shared_ptr<RenderGraph::ISubpassRecorder> recorder;
        std::optional<RHI::GraphicsPipelineDesc> pipelineDesc;
    };

    // 渲染路径的整体配置：stage -> queue -> SubpassConfig
    using RenderPathConfig = std::unordered_map<Scene::RenderStage, std::unordered_map<Scene::RenderQueue, SubpassConfig>>;

} // namespace StarryEngine