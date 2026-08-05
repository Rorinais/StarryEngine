#pragma once
#include "IPass.hpp"
#include "../graph/RenderGraph.hpp"
#include "../passExecutor/IPassExecutor.hpp"
#include <functional>

namespace StarryEngine {

    // 通用计算 pass 描述：声明式。configure 建图节点，onAfterCompile 建管线（声明与管线分离）。
    struct ComputePassDesc {
        std::string name;
        std::string shader;                 // .comp 路径
        uint32_t dispatchX = 1, dispatchY = 1, dispatchZ = 1;

        // 资源绑定（当前限定 set 0；按 binding 合并成单个描述符布局）
        struct Resource {
            RHI::DescriptorType type = RHI::DescriptorType::StorageBuffer;
            uint32_t binding = 0;
            std::string resourceName;       // 渲染图 buffer/纹理名（createVirtualBuffer / 纹理名）
            bool write = false;             // 写(StorageBuffer/StorageImage) or 读
            uint64_t bufferSize = 0;        // StorageBuffer 写入范围
        };
        std::vector<Resource> resources;

        // 每帧填充 push constant；nullptr = 无。大小须与 shader 声明一致
        std::function<void(void* out, float deltaTime)> fillPushConstants;
        uint32_t pushConstantSize = 0;
    };

    // 通用计算 pass：
    //   configure()      —— 声明 compute 节点 + 读/写资源 + dispatch（图结构）
    //   onAfterCompile() —— 建管线（shader → 布局 → 描述符 → executor）
    // 粒子物理更新、IBL 烘焙、后处理统一复用。
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
