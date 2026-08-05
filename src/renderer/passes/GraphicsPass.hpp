#pragma once
#include "IPass.hpp"
#include "Type.hpp"
#include "../graph/PassNode.hpp"
#include "../passExecutor/IPassExecutor.hpp"
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>

namespace StarryEngine {

    // GraphicsPass — 一次 Graphics RenderPass，包含多个 Subpass
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

        // 场景数据更新：把 draw items 分发给各 subpass executor，并从 sceneData.PSO 构建网格管线
        void onSceneData(const AnalysisSceneResult& sceneData,
                         const IPass::CompileContext& ctx,
                         const std::string& defaultTag) override;

        const std::vector<SubpassDesc>& getSubpassDescs() const { return m_subpasses; }
        RenderGraph::PassNode* getPassNode() const { return m_passNode; }

    private:
        std::string m_name;
        std::vector<SubpassDesc> m_subpasses;
        RenderGraph::PassNode* m_passNode = nullptr;
    };

} // namespace StarryEngine
