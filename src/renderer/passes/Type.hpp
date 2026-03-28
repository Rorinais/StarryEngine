#pragma once
#include "../../renderer/graph/Types.hpp"          
#include "../../scene/SceneType.hpp"   
#include "../../renderer/subpassRecorder/ISubpassRecorder.hpp"

namespace StarryEngine {
    struct TextureBinding {
        std::string textureName;        // 纹理名称（与纹理描述中的名称一致）
        uint32_t set;                   // 描述符集索引
        uint32_t binding;               // 绑定索引
        RHI::SamplerDesc samplerDesc;   // 采样器描述（可选）
    };

    struct StagePassInfo {
        RHI::RenderPassHandle renderPassHandle;
        std::unordered_map<Scene::RenderQueue, uint32_t> queueToSubpass;
    };

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

        std::shared_ptr<ISubpassRecorder> recorder;
        std::vector<TextureBinding> textureBindings;
        std::optional<Scene::GraphicsPipelineState> pipelineDesc;
    };
}