#pragma once
#include "IPass.hpp"
#include "../passExecutor/ParticleParams.hpp"
#include "../graph/PassNode.hpp"
#include <memory>
#include <string>
#include <functional>

namespace StarryEngine {

    struct ParticleSystemDesc {
        std::string name = "ParticleSystem";

        std::string computeShader  = "assets/shaders/test/particle.comp";
        std::string vertexShader   = "assets/shaders/test/particle.vert";
        std::string fragmentShader = "assets/shaders/test/particle.frag";

        uint32_t particleCount     = 1024;
        uint32_t perParticleFloats = 4;    
        ParticleParams params;

        // 初始数据生成器：对每个粒子调用 (index, outFloatData[perParticleFloats])
        // 默认：随机位置 + 随机生命周期（适合火焰/烟雾类粒子）
        std::function<void(uint32_t index, float* outData)> initParticle;
    };

    class ParticleSystemPass : public IPass {
    public:
        explicit ParticleSystemPass(const ParticleSystemDesc& desc);
        ~ParticleSystemPass() override = default;

        void setParticleParams(const ParticleParams& p) { m_desc.params = p; }
        ParticleParams& getParticleParams() { return m_desc.params; }
        const ParticleSystemDesc& getDesc() const { return m_desc; }

        // IPass
        bool configure(RenderGraph::RenderGraph& graph,
                       std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap,
                       uint32_t width, uint32_t height,
                       std::shared_ptr<RHI::ResourceManager> resMgr,
                       RHI::DescriptorSetLayoutHandle globalSetLayout) override;

        std::vector<PassSubpassInfo> getSubpasses() const override;

        void onAfterCompile(const CompileContext& ctx) override;

        const std::string& getName() const override { return m_desc.name; }

    private:
        void destroyResources(std::shared_ptr<RHI::ResourceManager> resMgr);
        void loadShaders(std::shared_ptr<RHI::ResourceManager> resMgr);
        void createComputeResources(std::shared_ptr<RHI::ResourceManager> resMgr);
        void createRenderPipelineLayout(std::shared_ptr<RHI::ResourceManager> resMgr,
                                        RHI::DescriptorSetLayoutHandle globalSetLayout);

        ParticleSystemDesc m_desc;

        // Render graph 资源
        RenderGraph::BufferId m_particleBufferId;

        // Shader handles
        RHI::ShaderHandle m_particleVS, m_particleFS, m_particleCS;

        // Descriptor set layouts
        RHI::DescriptorSetLayoutHandle m_particleCSDescLayout;
        RHI::DescriptorSetLayoutHandle m_particleRenderDescLayout;
        RHI::DescriptorSetLayoutHandle m_particleRenderSet1Layout;

        // Pipeline layouts
        RHI::PipelineLayoutHandle m_particleCSLayout;
        RHI::PipelineLayoutHandle m_particleRenderLayout;

        // Pipeline handles
        RHI::PipelineHandle m_particleCSPipeline;

        // Descriptor pools
        RHI::DescriptorPoolHandle m_particleCSPool;
        RHI::DescriptorPoolHandle m_particleRenderPool;

        // Pass nodes
        RenderGraph::PassNode* m_csPassNode = nullptr;
        RenderGraph::PassNode* m_renderPassNode = nullptr;

        // Dummy executor (configure 阶段) / 正式 executor (onAfterCompile 阶段)
        std::shared_ptr<IPassExecutor> m_currentExecutor;
    };

} // namespace StarryEngine
