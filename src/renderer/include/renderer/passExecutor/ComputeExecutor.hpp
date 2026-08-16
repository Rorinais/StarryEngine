// SceneDrawExecutor.hpp
#pragma once
#include <renderer/passExecutor/IPassExecutor.hpp>
#include <renderer/graph/PassNode.hpp>
#include <renderer/interface/vulkan/VulkanRHI.hpp>
#include <scene/Scene.hpp>
#include <renderer/RenderTypes.hpp>
#include <core/base.hpp>   

namespace StarryEngine {
    
    class ComputeExecutor : public IPassExecutor {
    public:
        ComputeExecutor(RHI::PipelineHandle pipeline, RHI::PipelineLayoutHandle layout,
                        RHI::DescriptorSetHandle descSet,
                        uint32_t dispatchX, uint32_t dispatchY, uint32_t dispatchZ,
                        uint32_t pcSize, std::function<void(void*, float)> fill)
            : m_pipeline(pipeline), m_layout(layout), m_descSet(descSet),
              m_dispatchX(dispatchX), m_dispatchY(dispatchY), m_dispatchZ(dispatchZ),
              m_pcSize(pcSize), m_fill(std::move(fill)) {}

        void clearDrawItems() override {}
        void setDrawItems(const std::vector<std::shared_ptr<DrawItem>>&) override {}
        const std::vector<std::shared_ptr<DrawItem>>& getDrawItems() override { return m_empty; }
        void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>&) override {}
        void addDrawItem(std::shared_ptr<DrawItem>) override {}

        void execute(RHI::RHICommandEncoder* encoder, const RenderContext& rctx,
                     const PassContext& pctx, uint32_t) override {
            if (!m_pipeline.isValid()) return;
            encoder->bindComputePipeline(m_pipeline);

            if (m_layout.isValid() && m_descSet.isValid())
                encoder->bindDescriptorSets(RHI::PipelineBindPoint::Compute, m_layout, 0, {m_descSet}, {});
            if (m_pcSize > 0 && m_fill) {
                std::vector<uint8_t> data(m_pcSize);
                m_fill(data.data(), rctx.deltaTime);
                encoder->pushConstants(m_layout, RHI::ShaderStage::Compute, 0, m_pcSize, data.data());
            }
            encoder->dispatch(m_dispatchX, m_dispatchY, m_dispatchZ);
        }

    private:
        RHI::PipelineHandle m_pipeline;
        RHI::PipelineLayoutHandle m_layout;
        RHI::DescriptorSetHandle m_descSet;
        uint32_t m_dispatchX = 1, m_dispatchY = 1, m_dispatchZ = 1;
        uint32_t m_pcSize;
        std::function<void(void*, float)> m_fill;
        std::vector<std::shared_ptr<DrawItem>> m_empty;
    };
}