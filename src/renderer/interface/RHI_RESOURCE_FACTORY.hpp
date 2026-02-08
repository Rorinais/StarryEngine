#pragma once
#include <unordered_map>
#include <iostream>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <string>
#include <array>
#include <functional>
#include <cassert>
#include <optional>
#include <chrono>
#include <thread>

#include "RHI_HANDLES_SYSTEM.hpp"
#include "RHI_VK_RESOURCE.hpp"
#include "RHI_TO_VK_FUNC.hpp"

namespace StarryEngine::RHI {
    class ResourceManager;

    class IResourceFactory {
    public:
        virtual ~IResourceFactory() = default;

        virtual std::unique_ptr<RHIBuffer> createBuffer(const BufferDesc& desc) = 0;
        virtual std::unique_ptr<RHITexture> createTexture(const TextureDesc& desc) = 0;
        virtual std::unique_ptr<RHIPipeline> createPipeline(const GraphicsPipelineDesc& desc) = 0;
        virtual std::unique_ptr<RHIPipeline> createComputePipeline(const ComputePipelineDesc& desc) = 0;
        virtual std::unique_ptr<RHIPipelineLayout> createPipelineLayout(const PipelineLayoutDesc& desc) = 0;
        virtual std::unique_ptr<RHIShaderModule> createShader(const ShaderModuleDesc& desc) = 0;
        virtual std::unique_ptr<RHISampler> createSampler(const SamplerDesc& desc) = 0;
        virtual std::unique_ptr<RHIRenderPass> createRenderPass(const RenderPassDesc& desc) = 0;
        virtual std::unique_ptr<RHIFramebuffer> createFramebuffer(const FramebufferDesc& desc) = 0;
        virtual std::unique_ptr<RHIDescriptorSet> createDescriptorSet(const DescriptorSetDesc& desc) = 0;
        virtual std::unique_ptr<RHIDescriptorPool> createDescriptorPool(const DescriptorPoolDesc& desc) = 0;
        virtual std::unique_ptr<RHIDescriptorSetLayout> createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) = 0;
        virtual std::unique_ptr<RHICommandBuffer> createCommandBuffer(const CommandBufferDesc& desc) = 0;
        virtual std::unique_ptr<RHICommandPool> createCommandPool(const CommandPoolDesc& desc) = 0;
        virtual std::unique_ptr<RHIFence> createFence(const FenceDesc& desc) = 0;
        virtual std::unique_ptr<RHISemaphore> createSemaphore(const SemaphoreDesc& desc) = 0;
        virtual std::unique_ptr<RHIEvent> createEvent(const EventDesc& desc) = 0;
        virtual std::unique_ptr<RHIQueryPool> createQueryPool(const QueryPoolDesc& desc) = 0;
        virtual std::unique_ptr<RHIAccelerationStructure> createAccelerationStructure(const AccelerationStructureDesc& desc) = 0;
        virtual std::unique_ptr<RHISwapChain> createSwapChain(const SwapChainDesc& desc) = 0;
        virtual std::unique_ptr<RHIQueue> createQueue(const QueueDesc& desc) = 0;

        virtual std::vector<std::unique_ptr<RHICommandBuffer>> createCommandBuffers(uint32_t count,const CommandBufferDesc& desc) = 0;
        virtual std::vector<std::unique_ptr<RHIDescriptorSet>> createDescriptorSets(uint32_t count,const DescriptorSetDesc& desc) = 0;

        virtual void setResourceManager(ResourceManager * ptr) = 0;
    };

    class VKResourceFactory : public IResourceFactory {
    public:
        VKResourceFactory(Device::Ptr device) : mDevice(device) {}

        ~VKResourceFactory() override = default;

        std::unique_ptr<RHIBuffer> createBuffer(const BufferDesc& desc) override;

        std::unique_ptr<RHITexture> createTexture(const TextureDesc& desc) override;

        std::unique_ptr<RHIPipeline> createPipeline(const GraphicsPipelineDesc& desc) override;

        std::unique_ptr<RHIPipeline> createComputePipeline(const ComputePipelineDesc& desc) override;

        std::unique_ptr<RHIPipelineLayout> createPipelineLayout(const PipelineLayoutDesc& desc) override;

        std::unique_ptr<RHIShaderModule> createShader(const ShaderModuleDesc& desc) override;

        std::unique_ptr<RHISampler> createSampler(const SamplerDesc& desc) override;

        std::unique_ptr<RHIRenderPass> createRenderPass(const RenderPassDesc& desc) override;

        std::unique_ptr<RHIFramebuffer> createFramebuffer(const FramebufferDesc& desc) override;

        std::unique_ptr<RHIDescriptorSet> createDescriptorSet(const DescriptorSetDesc& desc) override;

        std::unique_ptr<RHIDescriptorPool> createDescriptorPool(const DescriptorPoolDesc& desc) override;

        std::unique_ptr<RHIDescriptorSetLayout> createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) override;

        std::unique_ptr<RHICommandBuffer> createCommandBuffer(const CommandBufferDesc& desc) override;

        std::unique_ptr<RHICommandPool> createCommandPool(const CommandPoolDesc& desc) override;

        std::unique_ptr<RHIFence> createFence(const FenceDesc& desc) override;

        std::unique_ptr<RHISemaphore> createSemaphore(const SemaphoreDesc& desc) override;

        std::unique_ptr<RHIEvent> createEvent(const EventDesc& desc) override;

        std::unique_ptr<RHIQueryPool> createQueryPool(const QueryPoolDesc& desc) override;

        std::unique_ptr<RHIAccelerationStructure> createAccelerationStructure(const AccelerationStructureDesc& desc) override;

        std::unique_ptr<RHISwapChain> createSwapChain(const SwapChainDesc& desc) override;

        std::unique_ptr<RHIQueue> createQueue(const QueueDesc& desc) override;

        std::vector<std::unique_ptr<RHICommandBuffer>> createCommandBuffers(uint32_t count, const CommandBufferDesc& desc) override;

        std::vector<std::unique_ptr<RHIDescriptorSet>> createDescriptorSets(uint32_t count, const DescriptorSetDesc& desc) override;

        void setResourceManager(ResourceManager* ptr) override;
    private:
        Device::Ptr mDevice;

        ResourceManager* mResourceManager = nullptr;
    };
}