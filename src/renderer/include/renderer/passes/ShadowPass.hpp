#pragma once
#include <renderer/passes/IPass.hpp>
#include <renderer/passExecutor/SceneDrawExecutor.hpp>
#include <renderer/graph/PassNode.hpp>
#include <assets/loader/ShaderLoader.hpp>
#include <glm/glm.hpp>

namespace StarryEngine {

    class ShadowPass : public IPass {
    public:
        static constexpr uint32_t kShadowMapSize = 2048;

        explicit ShadowPass(std::string name) : m_name(std::move(name)) {}

        bool configure(RenderGraph::RenderGraph& graph,
                       std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap,
                       uint32_t width, uint32_t height,
                       std::shared_ptr<RHI::ResourceManager> resMgr,
                       RHI::DescriptorSetLayoutHandle globalSetLayout) override;

        std::vector<PassSubpassInfo> getSubpasses() const override;

        void onSceneData(const AnalysisSceneResult& sceneData,
                         const IPass::CompileContext& ctx,
                         const std::string& defaultTag) override;

        const std::string& getName() const override { return m_name; }

    private:
        bool ensureShaders(const std::shared_ptr<RHI::ResourceManager>& resMgr);
        GraphicsPipelineState buildShadowPSO(const GraphicsPipelineState& src);
        static bool isSkinnedInput(const GraphicsPipelineState& pso);

        std::string m_name;
        RenderGraph::PassNode* m_passNode = nullptr;
        std::shared_ptr<SceneDrawExecutor> m_executor;

        RHI::ShaderHandle m_shadowVS;
        RHI::ShaderHandle m_shadowSkinnedVS;
        RHI::ShaderHandle m_shadowFS;
    };

} // namespace StarryEngine
