#pragma once
#include "IPass.hpp"
#include "ComputePass.hpp"
#include "../../scene/ParticleEmitter.hpp"
#include "../../scene/Scene.hpp"
#include <vector>
#include <memory>

namespace StarryEngine {

    // 粒子 pass：渲染场景里所有 passTag 匹配的 ParticleEmitter（声明式，跟 MeshPass 同级）。
    // 模拟走通用 ComputePass；渲染走发射器自带的通用材质（shader + 混合），Sprite 点精灵。
    //
    // demo 用法：
    //   auto particles = std::make_shared<ParticlePass>("Particles", "Particles");
    //   particles->setScene(m_scene);
    //   passes.push_back(particles);
    //   m_scene->addParticleEmitter(fire);   // 增删后调 renderer->setNeedRebuildGraph()
    class ParticlePass : public IPass {
    public:
        ParticlePass(std::string name, std::string passTag = "Particles")
            : m_name(std::move(name)), m_passTag(std::move(passTag)) {}

        const std::string& getName() const override { return m_name; }
        const std::string& getPassTag() const { return m_passTag; }

        // 路由 tag 解析：粒子 pass 绑定的是 compute（模拟），不是材质（材质只是外观，不参与路由）。
        // emitter.passTag 显式分组 → 默认 "Particles"
        static std::string resolveEmitterTag(const Scene::ParticleEmitter& em) {
            if (!em.passTag.empty()) return em.passTag;
            return "Particles";
        }

        bool configure(RenderGraph::RenderGraph& graph,
                       std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap,
                       uint32_t width, uint32_t height,
                       std::shared_ptr<RHI::ResourceManager> resMgr,
                       RHI::DescriptorSetLayoutHandle globalSetLayout) override;

        std::vector<PassSubpassInfo> getSubpasses() const override;

        void onAfterCompile(const CompileContext& ctx) override;

    private:
        // 每个发射器的运行时状态
        struct EmitterState {
            std::shared_ptr<Scene::ParticleEmitter> emitter;
            RenderGraph::BufferId bufferId;
            std::shared_ptr<ComputePass> computePass;
            RenderGraph::PassNode* renderPassNode = nullptr;
            uint32_t renderSubpassIndex = 0;
            std::shared_ptr<IPassExecutor> executor;

            // 渲染资源（重建时销毁）
            RHI::DescriptorSetLayoutHandle set1Layout;
            RHI::PipelineLayoutHandle renderLayout;
            RHI::DescriptorPoolHandle pool;
            RHI::DescriptorSetHandle particleDescSet;
        };

        void destroyRenderResources(std::shared_ptr<RHI::ResourceManager> resMgr, EmitterState& st);
        // 默认 Sprite 材质（emitter 没给材质时用），懒创建
        std::shared_ptr<Assets::MaterialInstance> ensureDefaultMaterial(const CompileContext& ctx);

        std::string m_name;
        std::string m_passTag;
        RenderGraph::PassNode* m_renderPassNode = nullptr;
        std::vector<EmitterState> m_emitterStates;

        std::shared_ptr<Assets::MaterialInstance> m_defaultMaterial;
        RHI::DescriptorPoolHandle m_defaultPool;
    };

} // namespace StarryEngine
