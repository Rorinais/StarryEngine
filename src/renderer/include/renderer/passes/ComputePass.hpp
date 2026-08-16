#pragma once
#include <renderer/passes/IPass.hpp>
#include <renderer/graph/RenderGraph.hpp>
#include <renderer/passExecutor/IPassExecutor.hpp>
#include <functional>

namespace StarryEngine {

    struct ComputePassDesc {
        std::string name;
        std::string shader;                
        uint32_t dispatchX = 1, dispatchY = 1, dispatchZ = 1;

        struct Resource {
            RHI::DescriptorType type = RHI::DescriptorType::StorageBuffer;
            uint32_t binding = 0;
            std::string resourceName;      
            bool write = false;          
            uint64_t bufferSize = 0;     
        };
        std::vector<Resource> resources;

        std::function<void(void* out, float deltaTime)> fillPushConstants;
        uint32_t pushConstantSize = 0;
    };

    class ComputePass : public IPass {
    public:
        explicit ComputePass(const ComputePassDesc& desc) : m_desc(desc) {}

        const std::string& getName() const override { return m_desc.name; }

        bool configure(RenderGraph::RenderGraph& graph,
                       std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap,
                       uint32_t width, uint32_t height,
                       std::shared_ptr<RHI::ResourceManager> resMgr,
                       RHI::DescriptorSetLayoutHandle globalSetLayout) override;

        void onAfterCompile(const CompileContext& ctx) override;

    private:
        ComputePassDesc m_desc;
        RenderGraph::PassNode* m_passNode = nullptr;
        RHI::ShaderHandle m_shader;
        RHI::DescriptorSetLayoutHandle m_descLayout;
        RHI::PipelineLayoutHandle m_pipelineLayout;
        RHI::PipelineHandle m_pipeline;
        RHI::DescriptorPoolHandle m_pool;
        RHI::DescriptorSetHandle m_descSet;
        RHI::SamplerHandle m_sampler;
    };

} // namespace StarryEngine
