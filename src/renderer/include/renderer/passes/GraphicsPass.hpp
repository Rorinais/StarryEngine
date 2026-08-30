#pragma once
#include <renderer/passes/IPass.hpp>
#include <renderer/passes/Type.hpp>
#include <renderer/graph/PassNode.hpp>
#include <renderer/passExecutor/IPassExecutor.hpp>
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>

namespace StarryEngine {

    class GraphicsPass : public IPass {
    public:
        GraphicsPass(std::string name) : m_name(std::move(name)) {}

        void addSubpass(SubpassDesc desc) { m_subpasses.push_back(std::move(desc)); }

        const std::string& getName() const override { return m_name; }

        bool configure(RenderGraph::RenderGraph& graph,
                       std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap,
                       uint32_t width, uint32_t height,
                       std::shared_ptr<RHI::ResourceManager> resMgr,
                       RHI::DescriptorSetLayoutHandle globalSetLayout) override;

        std::vector<PassSubpassInfo> getSubpasses() const override;

        void onSceneData(const AnalysisSceneResult& sceneData,
                         const IPass::CompileContext& ctx,
                         const std::string& defaultTag) override;

        const std::vector<SubpassDesc>& getSubpassDescs() const { return m_subpasses; }
        RenderGraph::GraphNode* getPassNode() const { return m_passNode; }

    private:
        std::string m_name;
        std::vector<SubpassDesc> m_subpasses;
        RenderGraph::GraphNode* m_passNode = nullptr;
    };

} // namespace StarryEngine
