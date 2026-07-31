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

        const std::vector<SubpassDesc>& getSubpassDescs() const { return m_subpasses; }
        RenderGraph::PassNode* getPassNode() const { return m_passNode; }

    private:
        std::string m_name;
        std::vector<SubpassDesc> m_subpasses;
        RenderGraph::PassNode* m_passNode = nullptr;
    };

} // namespace StarryEngine
