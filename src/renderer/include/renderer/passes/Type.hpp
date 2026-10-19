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
        int layer = -1;            ///< 所属渲染层（RenderLayers 索引，-1=未声明）
        int queue = 0;             ///< 同层内的段号/顺序
        
        std::shared_ptr<IPassExecutor> executor;
        std::vector<SubpassAttachment> colorAttachments;
        std::optional<SubpassAttachment> depthAttachment;
        std::vector<SubpassAttachment> inputAttachments;
        std::vector<SubpassAttachment> resolveAttachments;
        std::vector<std::string> preserveAttachments;
    };

    /// (layer,queue) → 渲染图 subpass 名（标识用，不再有用户起名）
    inline std::string subpassName(int layer, int queue) { return "L" + std::to_string(layer) + "_Q" + std::to_string(queue); }

    struct PassDesc {
        std::string name;
        std::vector<SubpassDesc> subpasses;
    };

    using RenderPathConfig = std::vector<PassDesc>;
}