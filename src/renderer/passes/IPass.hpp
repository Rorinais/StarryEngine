#pragma once
#include "../graph/RenderGraph.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace StarryEngine {

    class IPassExecutor;

    struct PassSubpassInfo {
        std::string tag;
        uint32_t subpassIndex = 0;
        std::shared_ptr<IPassExecutor> executor;
        RenderGraph::PassNode* passNode = nullptr;
    };

    class IPass {
    public:
        struct CompileContext {
            std::shared_ptr<RHI::ResourceManager> resMgr;
            std::shared_ptr<RHI::IRHI> rhi;
            RenderGraph::RenderGraph* renderGraph = nullptr;
            RHI::DescriptorSetHandle globalDescSet;
            RHI::DescriptorSetLayoutHandle globalSetLayout;
        };

        virtual ~IPass() = default;

        virtual bool configure(RenderGraph::RenderGraph& graph,
                               std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap,
                               uint32_t width, uint32_t height,
                               std::shared_ptr<RHI::ResourceManager> resMgr,
                               RHI::DescriptorSetLayoutHandle globalSetLayout) = 0;

        virtual std::vector<PassSubpassInfo> getSubpasses() const { return {}; }

        virtual void onAfterCompile(const CompileContext& /*ctx*/) {}
        
        virtual const std::string& getName() const = 0;

    };

    using PassList = std::vector<std::shared_ptr<IPass>>;

} // namespace StarryEngine
