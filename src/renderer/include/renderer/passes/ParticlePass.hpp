#pragma once
#include <renderer/passes/IPass.hpp>
#include <renderer/passes/ComputePass.hpp>
#include <scene/ParticleEmitter.hpp>
#include <scene/Scene.hpp>
#include <vector>
#include <memory>

namespace StarryEngine {

    class ParticlePass : public IPass {
    public:
        explicit ParticlePass(std::string name) : m_name(std::move(name)) {}

        const std::string& getName() const override { return m_name; }


        bool configure(RenderGraph::RenderGraph& graph,
                       std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap,
                       uint32_t width, uint32_t height,
                       std::shared_ptr<RHI::ResourceManager> resMgr,
                       RHI::DescriptorSetLayoutHandle globalSetLayout) override;

        std::vector<PassSubpassInfo> getSubpasses() const override;

        void onAfterCompile(const CompileContext& ctx) override;

    private:
        struct EmitterState {
            std::shared_ptr<Scene::ParticleEmitter> emitter;
            RenderGraph::BufferId bufferId;
            std::shared_ptr<ComputePass> computePass;
            RenderGraph::GraphNode* renderPassNode = nullptr;
            uint32_t renderSubpassIndex = 0;
            std::shared_ptr<IPassExecutor> executor;

            RHI::DescriptorSetLayoutHandle set1Layout;
            RHI::PipelineLayoutHandle renderLayout;
            RHI::DescriptorPoolHandle pool;
            RHI::DescriptorSetHandle particleDescSet;
        };

        void destroyRenderResources(std::shared_ptr<RHI::ResourceManager> resMgr, EmitterState& st);
        std::shared_ptr<Assets::MaterialInstance> ensureDefaultMaterial(const CompileContext& ctx);

        std::string m_name;
        RenderGraph::GraphNode* m_renderPassNode = nullptr;
        std::vector<EmitterState> m_emitterStates;

        std::shared_ptr<Assets::MaterialInstance> m_defaultMaterial;
        RHI::DescriptorPoolHandle m_defaultPool;
    };

} // namespace StarryEngine
