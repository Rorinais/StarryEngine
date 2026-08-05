#pragma once
#include "../graph/RenderGraph.hpp"
#include "../RenderTypes.hpp"
#include "../RenderBlackboard.hpp"
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

        // 数据源注入：render path 建图前自动调用（demo 无需手动注入），pass 在 configure/onAfterCompile 里按类型从黑板取数据
        void setDataProvider(RenderBlackboard* data) { m_data = data; }

        virtual bool configure(RenderGraph::RenderGraph& graph,
                               std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap,
                               uint32_t width, uint32_t height,
                               std::shared_ptr<RHI::ResourceManager> resMgr,
                               RHI::DescriptorSetLayoutHandle globalSetLayout) = 0;

        virtual std::vector<PassSubpassInfo> getSubpasses() const { return {}; }

        virtual void onAfterCompile(const CompileContext& /*ctx*/) {}

        // 场景分析完成后：分发 draw items + 构建依赖场景数据的管线（如网格 PSO）。
        // 与 onAfterCompile 分工：后者构建"只依赖图结构"的资源，前者构建"依赖场景数据"的资源。
        // defaultTag：场景里没显式指定 subpass tag 的 draw item 路由到哪个 subpass（由渲染路径算出）。
        virtual void onSceneData(const AnalysisSceneResult& /*sceneData*/,
                                 const CompileContext& /*ctx*/,
                                 const std::string& /*defaultTag*/) {}

        virtual const std::string& getName() const = 0;

    protected:
        RenderBlackboard* m_data = nullptr;   // 数据源（render path 注入的黑板）
    };

    using PassList = std::vector<std::shared_ptr<IPass>>;

} // namespace StarryEngine
