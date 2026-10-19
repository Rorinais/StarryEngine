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
        std::string tag;           // 兼容：字符串标识（后续由 layer+queue 取代）
        int layer = -1;            ///< 所属渲染层（RenderLayers 索引，-1=未声明）
        int queue = 0;             ///< 同层内的段号/顺序
        
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